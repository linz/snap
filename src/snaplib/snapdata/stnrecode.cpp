#include "snapconfig.h"

/* Routines for managing station code translations */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "util/snapctype.h"
#include <algorithm>
#include <iterator>
#include <optional>
#include <string>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/numeric/conversion/cast.hpp>

#include "snapdata/stnrecode.h"
#include "snapdata/stnrecodefile.h"
#include "util/chkalloc.h"
#include "util/dstring.h"
#include "util/fieldscanner.hpp"
#include "snapdata/datatype.h"
#include "util/datafile.h"
#include "snapdata/loaddata.h"
#include "snapdata/survdata.h"
#include "util/dateutil.h"
#include "util/dstring.h"
#include "util/errdef.h"
#include "util/fileutil.h"
#include "util/linklist.h"
#include "util/symmatrx.h"
#include "util/progress.h"
#include "util/pi.h"

using boost::numeric_cast;

/* Tolerance in comparing dates */

#define DESCRIBE_MAX_LEN (STNCODELEN+(MAX_DATE_LEN)*2+80)

/// Returns -1, 0 or 1 as d0 is before, the same as or after d1
static int compare_three_way( const double d0, const double d1 )
{
    if( d0 < d1 )
    {
        return -1;
    }
    return d0 == d1 ? 0 : 1;
}

static int nextseqid=0;

/// Creates a recode to codeto, where a leading RECODE_IGNORE_CHAR marks the station as rejected
static stn_recode create_stn_recode( std::string_view codeto, double datefrom, double dateto, double herror, double verror )
{
    const bool reject=! codeto.empty() && codeto[0] == RECODE_IGNORE_CHAR;
    if( reject ) codeto.remove_prefix(1);
    return stn_recode( std::string(codeto), reject, datefrom, dateto, herror, verror, ++nextseqid );
}

/* Write description of recoding - assume buffer is big enough (DESCRIBE_MAX_LEN) */
static char *describe_stn_recode( const stn_recode &src, char *buffer, int stnwidth )
{
    int nch=0;
    int nch1=0;
    if( src.reject && src.codeto.empty() )
    {
        sprintf( buffer,"ignored");
        nch=7;
    }
    else
    {
        const std::string codeto=(src.reject ? std::string(1,RECODE_IGNORE_CHAR) : std::string())+src.codeto;
        sprintf( buffer, "%-*.*s%n",stnwidth,STNCODELEN+1,codeto.c_str(),&nch);
    }
    if( src.datefrom != UNDEFINED_DATE && src.dateto != UNDEFINED_DATE )
    {
        sprintf( buffer+nch, " between %s%n",date_as_string(src.datefrom,"DT?",0),&nch1 );
        nch += nch1;
        sprintf( buffer+nch, " and %s%n",date_as_string(src.dateto,"DT?",0),&nch1 );

    }
    else if( src.datefrom != UNDEFINED_DATE )
    {
        sprintf( buffer+nch, " after %s%n",date_as_string(src.datefrom,"DT?",0),&nch1 );
    }
    else if( src.dateto != UNDEFINED_DATE )
    {
        sprintf( buffer+nch, " before %s%n",date_as_string(src.dateto,"DT?",0),&nch1 );
    }
    nch += nch1;
    if( src.herror > 0.0 || src.verror > 0.0 )
    {
        sprintf( buffer+nch, " co-location error %.3lf %.3lf m",
                std::max(std::min(src.herror,9999.999),0.0),
                std::max(std::min(src.verror,9999.999),0.0));
    }
    return buffer;
}

/* Sort function sorts into order that ensures first match is the correct
 * one to use in get_stn_recode, except for "after ###" for which the date
 * order is ascending. 
 */
static int cmp_stn_recode( const stn_recode &src0, const stn_recode &src1 )
{
    if( src1.datefrom == UNDEFINED_DATE && src1.dateto == UNDEFINED_DATE )
    {
        if( src0.datefrom == UNDEFINED_DATE && src0.dateto == UNDEFINED_DATE )
        {
            return 0;
        }
        else
        {
            return -1;
        }
    }
    if( src0.datefrom == UNDEFINED_DATE && src0.dateto == UNDEFINED_DATE )
    {
        return 1;
    }
    if( src1.dateto == UNDEFINED_DATE )
    {
        if( src0.dateto != UNDEFINED_DATE )
        {
            return -1;
        }
        else
        {
            return compare_three_way(src0.datefrom,src1.datefrom);
        }
    }
    if( src0.dateto == UNDEFINED_DATE )
    {
        return 1;
    }
    if( src1.datefrom == UNDEFINED_DATE )
    {
        if( src0.datefrom != UNDEFINED_DATE )
        {
            return 1;
        }
        else
        {
            return compare_three_way(src0.dateto,src1.dateto);
        }
    }
    if( src0.dateto == UNDEFINED_DATE )
    {
        return -1;
    }
    int result = compare_three_way(src0.datefrom,src1.datefrom);
    if( result == 0 )
    {
        result=compare_three_way(src0.dateto,src1.dateto);
    }
    return result;
}

static void update_stn_recode( const std::string &codefrom, stn_recode &src, const stn_recode &src_update )
{
    int diffmark=stncodecmp(src.codeto,src_update.codeto);
    if( diffmark || src.reject != src_update.reject )
    {
        char errmsg[60+2*STNCODELEN+DESCRIBE_MAX_LEN];
        int nch;
        const std::string codeto=(src_update.reject ? std::string(1,RECODE_IGNORE_CHAR) : std::string())+src_update.codeto;
        sprintf(errmsg,"Overriding recode of %.*s to %.*s with recode to %n",
                STNCODELEN,codefrom.c_str(),STNCODELEN+1,codeto.c_str(),&nch);
        describe_stn_recode(src,errmsg+nch,0);
        handle_error(INFO_ERROR,errmsg,NO_MESSAGE);
        if( diffmark ) return;
    }
    if( src_update.herror <= 0.0 )
    {
        src.herror=0.0;
    }
    else if( src.herror > 0.0 && src_update.herror > src.herror )
    {
        src.herror=src_update.herror;
    }
    if( src_update.verror <= 0.0 )
    {
        src.verror=0.0;
    }
    else if( src.verror > 0.0 && src_update.verror > src.verror )
    {
        src.verror=src_update.verror;
    }
}

/// Adds a recode to the list of recodes of codefrom, in its sorted position.
/// It replaces any recode in the list for the same date range.
static void add_stn_recode_to_list( stn_recode_list &list, const std::string &codefrom, stn_recode recode )
{
    auto previous=list.before_begin();
    auto current=list.begin();
    int cmp=1;
    while( current != list.end() )
    {
        cmp=cmp_stn_recode(*current,recode);
        if( cmp >= 0 ) break;
        previous=current;
        ++current;
    }
    if( current != list.end() && cmp == 0 )
    {
        update_stn_recode(codefrom,recode,*current);
        list.erase_after(previous);
    }
    const auto added=list.insert_after(previous,std::move(recode));
    /* Check for incompatible recoding */
    for( auto other=list.begin(); other != list.end(); ++other )
    {
        if( other == added ) continue;
        const stn_recode &src=*other;
        const stn_recode &trans=*added;
        if( src.dateto == UNDEFINED_DATE && src.datefrom == UNDEFINED_DATE ) continue;
        if( trans.dateto == UNDEFINED_DATE && trans.datefrom == UNDEFINED_DATE ) continue;
        if( trans.dateto == UNDEFINED_DATE && src.dateto == UNDEFINED_DATE ) continue;
        if( trans.datefrom == UNDEFINED_DATE && src.datefrom == UNDEFINED_DATE ) continue;
        if( src.dateto != UNDEFINED_DATE
                && trans.datefrom != UNDEFINED_DATE
                && src.dateto < trans.datefrom
                ) continue;
        if( trans.dateto != UNDEFINED_DATE
                && src.datefrom != UNDEFINED_DATE
                && trans.dateto < src.datefrom
                ) continue;
        char errmsg[40+STNCODELEN+2*DESCRIBE_MAX_LEN];
        sprintf(errmsg,"Recode of %.*s to ", STNCODELEN,codefrom.c_str());
        describe_stn_recode(trans,errmsg+strlen(errmsg),0);
        strcat(errmsg," conflicts with ");
        describe_stn_recode(src,errmsg+strlen(errmsg),0);
        handle_error(INFO_ERROR,errmsg,NO_MESSAGE);
    }
}


stn_recode_map *create_stn_recode_map( network *net )
{
    return new stn_recode_map( net );
}

bool recodes_used( stn_recode_map *stt )
{
    return stt->used;
}

void delete_stn_recode_map( stn_recode_map *stt ) {
    delete stt;
}

static stn_recode_list *lookup_station_recode( stn_recode_map *stt, std::string_view code )
{
    if ( ! stt ) return nullptr;
    const auto match=stt->lists.find( code );
    return match != stt->lists.end() ? &match->second : nullptr;
}

static void add_stn_recode_to_map_err( stn_recode_map *stt, std::string_view codefrom, std::string_view codeto, double datefrom, double dateto, double herror, double verror )
{
    const bool global=boost::algorithm::iequals(codefrom,RECODE_IGNORE_CODE);
    /* Global recoding only applies for ignoring codes */
    if( global && ! boost::algorithm::iequals(codeto,RECODE_IGNORE_CODE) ) return;
    if( global )
    {
        add_stn_recode_to_list( stt->global, std::string(codefrom), create_stn_recode( codeto, datefrom, dateto, herror, verror ) );
    }
    else
    {
        const auto stlist=stt->lists.try_emplace( std::string(codefrom) ).first;
        add_stn_recode_to_list( stlist->second, stlist->first, create_stn_recode( codeto, datefrom, dateto, herror, verror ) );
    }
}

void add_stn_recode_to_map( stn_recode_map *stt, std::string_view codefrom, std::string_view codeto, double datefrom, double dateto )
{
    add_stn_recode_to_map_err( stt, codefrom, codeto, datefrom, dateto, 0.0, 0.0 );
}

struct stn_recode_suffix_data
{
    stn_recode_map *srm;
    char *suffix;
    double datefrom;
    double dateto;
    double herror;
    double verror;
};


static void apply_recode_suffix( station *st, void *psrd )
{
    char codeto[STNCODELEN+1];
    stn_recode_suffix_data *srd=(stn_recode_suffix_data *) psrd;
    char *suffix=srd->suffix;
    int sfxlen=strlen(suffix);
    const char *codefrom=st->Code;
    int codelen=strlen(codefrom);

    /* Check if reprocessing a code for which a suffix is already applied */
    if( codelen > sfxlen && _stricmp(suffix,codefrom+(codelen-sfxlen))==0 ) return;
    if( sfxlen > STNCODELEN ) return;

    strcpy(codeto,codefrom);
    codeto[STNCODELEN-sfxlen]=0;
    strcat(codeto,suffix);
    add_stn_recode_to_map_err( srd->srm, codefrom, codeto, srd->datefrom, srd->dateto, srd->herror, srd->verror );
}

struct RecodeTarget
{
    std::optional<std::string> suffix;  ///< set only for the "suffix <name>" form
    std::string codefrom;               ///< set only for the plain "<codefrom> [to] <codeto>" form
    std::string codeto;                 ///< set only for the plain "<codefrom> [to] <codeto>" form
};

/// Parses the recode target: "suffix <name>" or a plain
/// "<codefrom> [to] <codeto>" pair. Returns nullopt on any parse error,
/// with msg set to describe it.
/// e.g. "suffix RC1" (suffix form), or "ABC to XYZ" / "ABC XYZ" (plain
/// form, "to" optional).
static std::optional<RecodeTarget> parse_recode_codes(
    FieldScanner &scanner,        ///< field cursor, will be advanced past whatever this function consumes
    std::string_view firstField,  ///< the first field of the recode definition; the caller has already
                                   ///< checked it isn't "file", which is a separate grammar handled inline
    char *msg )                   ///< caller's fixed-size error message buffer, filled in on failure
{
    RecodeTarget target;

    if( boost::algorithm::iequals(firstField,"suffix") )
    {
        auto suffixField=scanner.next();
        if( ! suffixField )
        {
            strcpy(msg,"Suffix missing in station recode");
            return std::nullopt;
        }
        target.suffix=std::string(*suffixField);
        return target;
    }

    target.codefrom.assign(firstField);
    auto field=scanner.next();
    if( field && boost::algorithm::iequals(*field,"to") ) field=scanner.next();
    bool ok=true;
    if( ! field )
    {
        strcpy(msg,"Missing target station code in recode definition");
        ok=false;
    }
    else
    {
        target.codeto.assign(*field);
    }
    if( boost::algorithm::iequals(target.codefrom,RECODE_IGNORE_CODE) && ! boost::algorithm::iequals(target.codeto,RECODE_IGNORE_CODE) )
    {
        sprintf(msg,"Cannot recode from %.20s to %.20s",target.codefrom.c_str(),target.codeto.c_str());
        ok=false;
    }
    return ok ? std::optional<RecodeTarget>(target) : std::nullopt;
}

struct UncertaintyResult
{
    double herror=0.0;                          ///< horizontal error, 0.0 if no hv_error clause
    double verror=0.0;                          ///< vertical error, 0.0 if no hv_error clause
    int n_enu=0;                                ///< count of hv_error values parsed (0, 1 or 2)
    std::optional<std::string_view> nextField;   ///< the field after this clause, for the date-range parser
};

/// Parses the optional uncertainty clause ("disconnected" or
/// "hv_error <val> [<val>] m|metres") named by field. If the field names
/// neither keyword, nextField is set to field unchanged, so the caller can
/// reinterpret it as the start of the date-range clause. Returns nullopt
/// only on a genuine hv_error parse error, with msg set.
/// e.g. "disconnected", "hv_error 0.5 m", or "hv_error 0.5 0.6 m".
static std::optional<UncertaintyResult> parse_uncertainty(
    FieldScanner &scanner,     ///< field cursor, will be advanced past whatever this function consumes
    std::string_view field,    ///< the field to inspect; the caller has already confirmed it has a value
    char *msg )                ///< caller's fixed-size error message buffer, filled in on failure
{
    UncertaintyResult result;

    if( boost::algorithm::iequals(field,"disconnected") )
    {
        result.nextField=scanner.next();
        return result;
    }
    if( ! boost::algorithm::iequals(field,"hv_error") )
    {
        result.nextField=field;
        return result;
    }

    bool ok=true;
    while( ok )
    {
        auto errorField=scanner.next();
        if( ! errorField ) break;
        if( boost::algorithm::iequals(*errorField,"m") || boost::algorithm::iequals(*errorField,"metres") )
        {
            break;
        }
        else
        {
            if( result.n_enu > 1 )
            {
                strcpy(msg,"Missing m (metres) at end of hv_error in recode definition");
                ok=false;
                break;
            }
            auto verror = parse_positive_double( *errorField );
            if( ! verror )
            {
                std::string errorText(*errorField);
                sprintf(msg,"Invalid hv_error %s in recode definition",errorText.c_str());
                ok=false;
                break;
            }
            result.verror = *verror;
            if( result.n_enu == 0 ) result.herror=result.verror;
            result.n_enu++;
        }
    }
    if( ok && result.n_enu == 0 )
    {
        strcpy(msg,"Missing hv_error in recode definition");
        ok=false;
    }
    result.nextField=scanner.next();
    return ok ? std::optional<UncertaintyResult>(result) : std::nullopt;
}

struct DateRange
{
    double datefrom=UNDEFINED_DATE;  ///< start of the date range, UNDEFINED_DATE if unbounded
    double dateto=UNDEFINED_DATE;    ///< end of the date range, UNDEFINED_DATE if unbounded
};

/// Parses the optional date-range clause ("between X and Y" / "before Y" /
/// "after X") named by field. n_enu is the count of hv_error values already
/// parsed ("hv_error" and "between" cannot be combined). Returns nullopt on
/// any parse error, with msg set.
/// e.g. "between 2000-01-01 and 2001-01-01", "before 2000-01-01", or
/// "after 2000-01-01".
static std::optional<DateRange> parse_date_range(
    FieldScanner &scanner,   ///< field cursor, will be advanced past whatever this function consumes
    std::string_view field,  ///< the field to inspect; the caller has already confirmed it has a value
    int n_enu,               ///< count of hv_error values already parsed by parse_uncertainty
    char *msg )               ///< caller's fixed-size error message buffer, filled in on failure
{
    DateRange range;
    std::optional<std::string> fromdef;
    std::optional<std::string> todef;
    bool ok=true;
    if( boost::algorithm::iequals(field,"between") )
    {
        auto fromField=scanner.next();
        if( ! fromField )
        {
            strcpy(msg,"Date missing after \"between\"");
            ok=false;
        }
        else
        {
            fromdef=std::string(*fromField);
        }
        if( ok )
        {
            auto andField=scanner.next();
            if( ! ( andField && boost::algorithm::iequals(*andField,"and") ) )
            {
                strcpy(msg,"\"and\" missing after \"between\"");
                ok=false;
            }
        }
        if( ok )
        {
            auto toField=scanner.next();
            if( ! toField )
            {
                strcpy(msg,"Date missing after \"and\"");
                ok=false;
            }
            else
            {
                todef=std::string(*toField);
            }
        }
        if( ok && n_enu > 0 )
        {
            strcpy(msg,"Cannot use \"hv_error\" and \"between\" in recode definition");
            ok=false;
        }
    }
    else if( boost::algorithm::iequals(field,"before") )
    {
        auto toField=scanner.next();
        if( ! toField )
        {
            strcpy(msg,"Date missing after \"before\"");
            ok=false;
        }
        else
        {
            todef=std::string(*toField);
        }
    }
    else if( boost::algorithm::iequals(field,"after") )
    {
        auto fromField=scanner.next();
        if( ! fromField )
        {
            strcpy(msg,"Date missing after \"after\"");
            ok=false;
        }
        else
        {
            fromdef=std::string(*fromField);
        }
    }
    else
    {
        std::string fieldText(field);
        sprintf(msg,"Undefined field %.50s",fieldText.c_str());
        ok=false;
    }
    if( ok && fromdef )
    {
        range.datefrom=snap_datetime_parse( fromdef->c_str(), nullptr );
        if( ! range.datefrom )
        {
            sprintf(msg,"Invalid from date \"%.50s\"",fromdef->c_str());
            ok=false;
        }
    }
    if( ok && todef )
    {
        range.dateto=snap_datetime_parse( todef->c_str(), nullptr );
        if( ! range.dateto )
        {
            sprintf(msg,"Invalid to date \"%.50s\"",todef->c_str());
            ok=false;
        }
        else if( range.datefrom != UNDEFINED_DATE && range.datefrom >= range.dateto )
        {
            sprintf(msg,"Start date \"%.20s\" and end date \"%.20s\" inconsistent",fromdef ? fromdef->c_str() : "",todef->c_str());
            ok=false;
        }
    }
    return ok ? std::optional<DateRange>(range) : std::nullopt;
}

int read_station_recode_definition( stn_recode_map *stt, std::string_view def, const std::string &basefile )
{
    char msg[80+MAX_FILENAME_LEN];
    std::string codefrom;
    std::string codeto;
    double datefrom=UNDEFINED_DATE;
    double dateto=UNDEFINED_DATE;
    std::optional<std::string> suffix;
    int n_enu=0;
    double herror=0.0;
    double verror=0.0;
    bool ok=true;

/*
   recode xxx to yyyy uncertainty
   recode xxx to yyyy uncertainty between date and date
   recode xxx to yyyy uncertainty before date
   recode xxx to yyyy uncertainty after date
   recode suffix xxx uncertainty between/before/after for station_list

   uncertainty is optional and can be one of
   disconnected
   hv_error ###.# mm
   hv_error ###.# ###.# mm

*/
    FieldScanner scanner( def );
    auto field=scanner.next();

    if( ! field )
    {
        strcpy(msg,"Missing source station code in recode definition");
        ok=false;
    }
    else if ( boost::algorithm::iequals(*field,"file") )
    {
        // "recode file <recodefile>" is a distinct grammar from the other
        // two forms below, and shares none of their optional uncertainty or
        // date-range clauses etc. - the recode file's own columns are all it
        // supports. read_station_recode_file has already applied every
        // recode from the file by the time it returns, so this branch returns
        // immediately rather than falling into logic that assumes codefrom
        // and codeto are set.
        auto filenameField=scanner.next();
        if( ! filenameField )
        {
            strcpy(msg,"Filename missing in station recode");
            ok=false;
        }
        else
        {
            std::string filename(*filenameField);
            int sts=read_station_recode_file( stt, filename.c_str(), basefile.c_str() );
            if( sts != OK )
            {
                sprintf(msg,"Error reading station recode file %.*s",MAX_FILENAME_LEN,filename.c_str());
                ok=false;
            }
        }
        if( ! ok )
        {
            handle_error(INVALID_DATA,"Error reading station recoding",msg);
        }
        return ok ? OK : INVALID_DATA;
    }
    else
    {
        auto target=parse_recode_codes( scanner, *field, msg );
        if( ! target )
        {
            ok=false;
        }
        else
        {
            suffix=std::move(target->suffix);
            codefrom=std::move(target->codefrom);
            codeto=std::move(target->codeto);
        }
    }

    field=scanner.next();
    if( ok && field )
    {
        auto uncertainty=parse_uncertainty( scanner, *field, msg );
        if( ! uncertainty )
        {
            ok=false;
        }
        else
        {
            herror=uncertainty->herror;
            verror=uncertainty->verror;
            n_enu=uncertainty->n_enu;
            field=uncertainty->nextField;
        }
    }

    if( ok && field )
    {
        auto range=parse_date_range( scanner, *field, n_enu, msg );
        if( ! range )
        {
            ok=false;
        }
        else
        {
            datefrom=range->datefrom;
            dateto=range->dateto;
        }
    }

    if( suffix && ok )
    {
        field=scanner.next();
        if( ! field || ! boost::algorithm::iequals(*field,"for") )
        {
            strcpy(msg,"\"for\" missing in \"recode suffix\" definition");

        }
        std::string stationList(scanner.remainder());
        if( stationList.empty() )
        {
            strcpy(msg,"Station list missing from recode suffix definition");
            ok=false;
        }
        else
        {
            stn_recode_suffix_data srd;
            srd.srm=stt;
            srd.suffix=suffix->data();
            srd.datefrom=datefrom;
            srd.dateto=dateto;
            srd.herror=herror;
            srd.verror=verror;
            process_selected_stations( stt->net, stationList.c_str(), basefile, &srd, apply_recode_suffix );
        }
    }
    else if( ok )
    {
        add_stn_recode_to_map_err( stt, codefrom, codeto, datefrom, dateto, herror, verror );
    }

    if( ! ok )
    {
        handle_error(INVALID_DATA,"Error reading station recoding",msg);
    }
    return ok ? OK : INVALID_DATA;
}

void print_stn_recode_list( FILE *out, stn_recode_map *stt, bool onlyused, int stn_name_width, std::string_view prefix )
{
    char description[DESCRIBE_MAX_LEN];
    if( ! stt ) return;
    const int prefix_length=numeric_cast<int>(prefix.size());
    for( const auto &[codefrom,recodes] : stt->lists )
    {
        bool first=true;
        bool show_reloc=false;
        if( onlyused )
        {
            for( const stn_recode &src : recodes )
            {
                if( ! src.used ) continue;
                if( src.herror > 0.0 || src.verror > 0.0 )
                {
                    show_reloc=true;
                    break;
                }
            }
        }
        for( const stn_recode &src : recodes )
        {
            if( onlyused && ! src.used )
            {
                if( ! show_reloc ) continue;
                if( src.datefrom == UNDEFINED_DATE && src.dateto == UNDEFINED_DATE ) continue;
                if( src.datefrom != UNDEFINED_DATE && src.dateto != UNDEFINED_DATE ) continue;
            }
            if( first )
            {
                first=false;
                fprintf(out,"%.*s%-*s ", prefix_length,prefix.data(),stn_name_width,codefrom.c_str());
            }
            else
            {
                fprintf(out,"%.*s%-*s ", prefix_length,prefix.data(),stn_name_width," ");
            }
            fprintf(out,"to %s",describe_stn_recode( src, description, stn_name_width ));
            fprintf( out, "\n");
        }
    }
    bool first=true;
    for( const stn_recode &src : stt->global )
    {
        if( onlyused && ! src.used ) continue;
        if( first )
        {
            first=false;
            fprintf(out,"%.*sOther stations ",prefix_length,prefix.data());
        }
        else
        {
            fprintf(out,"%.*s              ", prefix_length,prefix.data());
        }
        fprintf(out,"to %s",describe_stn_recode( src, description, stn_name_width ));
        fprintf( out, "\n");
    }
}

const stn_recode_list *get_station_recodes( stn_recode_map *stt, std::string_view code )
{
    return lookup_station_recode( stt, code );
}

/// Finds the recode in a list that applies on date, or list.end() if none does
static stn_recode_list::iterator find_stn_recode( stn_recode_list &list, double date )
{
    for( auto src=list.begin(); src != list.end(); ++src )
    {
        if( src->datefrom == UNDEFINED_DATE && src->dateto == UNDEFINED_DATE ) return src;
        if( date == UNDEFINED_DATE ) continue;
        if( src->datefrom == UNDEFINED_DATE && date < src->dateto ) return src;
        if( date <= src->dateto && date >= src->datefrom ) return src;
        if( src->dateto == UNDEFINED_DATE && date >= src->datefrom )
        {
            /* Use the last of the "after" recodes that has started by date */
            auto latest=src;
            for( auto next=std::next(src);
                 next != list.end() && next->datefrom != UNDEFINED_DATE && date >= next->datefrom;
                 ++next )
            {
                latest=next;
            }
            return latest;
        }
    }
    return list.end();
}

std::optional<recode_result> get_stn_recode( stn_recode_map *stt, std::string_view code, double date )
{
    for( stn_recode_list *list : { lookup_station_recode( stt, code ), &stt->global } )
    {
        if( ! list ) continue;
        const auto src=find_stn_recode( *list, date );
        if( src == list->end() ) continue;
        src->used=true;
        stt->used=true;
        return recode_result{ src->codeto, src->reject };
    }
    return std::nullopt;
}

std::optional<recode_result> recoded_network_station( void *recode_data, std::string_view code, double date )
{
    stn_recode_data *srd=(stn_recode_data *)recode_data;
    if( ! srd ) return std::nullopt;
    std::optional<recode_result> file_recode;
    std::optional<recode_result> global_recode;
    if( srd->file_map ) file_recode=get_stn_recode(srd->file_map,code,date);
    if( srd->global_map ) global_recode=get_stn_recode(srd->global_map,file_recode ? file_recode->code : code,date);
    if( ! global_recode && ! file_recode ) return std::nullopt;
    const recode_result &recoded=global_recode ? *global_recode : *file_recode;
    const bool reject=( file_recode && file_recode->reject ) || ( global_recode && global_recode->reject );

    /* Ignored stations are not created; others are created if the recoded station does not exist */
    if( ! ( reject && recoded.code.empty() ) && srd->net )
    {
        const std::string recoded_code( recoded.code );
        int id = find_station( srd->net, recoded_code );
        if( ! id )
        {
            if( global_recode && file_recode ) id = find_station( srd->net, file_recode->code );
            if( ! id ) id = find_station( srd->net, code );
            if( id )
            {
                station *st=station_ptr(srd->net,id);
                duplicate_network_station( srd->net, st, recoded_code.c_str(), st->Name.c_str() );
            }
        }
    }
    return recode_result{ recoded.code, reject };
}
