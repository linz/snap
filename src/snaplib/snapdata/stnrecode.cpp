#include "snapconfig.h"

/* Routines for managing station code translations */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "util/snapctype.h"
#include <algorithm>
#include <optional>
#include <string>
#include <boost/algorithm/string/predicate.hpp>

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

/* Tolerance in comparing dates */

#define CMPFLOAT(a,b) (((a)<(b)) ? -1 : ((a)==(b)) ? 0 : 1)
#define DESCRIBE_MAX_LEN (STNCODELEN+(MAX_DATE_LEN)*2+80)

static int nextseqid=0;

static stn_recode *create_stn_recode( const char *codeto, double datefrom, double dateto, double herror, double verror )
{
    int usemark=codeto[0] == RECODE_IGNORE_CHAR ? 0 : 1;
    if( ! usemark ) codeto++;

    stn_recode *sct=(stn_recode *) check_malloc(sizeof(stn_recode)+strlen(codeto)+2);
    char *sctcodeto = ((char *)(void *)sct)+sizeof(stn_recode);
    *sctcodeto=RECODE_IGNORE_CHAR;
    sctcodeto++;
    strcpy(sctcodeto,codeto);
    sct->seqid=++nextseqid;
    sct->codeto=sctcodeto;
    sct->usemark=usemark;
    sct->datefrom=datefrom;
    sct->dateto=dateto;
    sct->used=RECODE_UNUSED;
    sct->herror=herror;
    sct->verror=verror;
    sct->next=0;
    return sct;
}

static stn_recode_list *create_stn_recode_list( const char *codefrom )
{
    stn_recode_list *sctl=(stn_recode_list *) check_malloc(sizeof(stn_recode_list)+strlen(codefrom)+1);
    char *sctcodefrom = ((char *)(void *)sctl)+sizeof(stn_recode_list);
    strcpy(sctcodefrom,codefrom);
    sctl->codefrom=sctcodefrom;
    sctl->translations=0;
    sctl->next=0;
    return sctl;
}

/* Write description of recoding - assume buffer is big enough (DESCRIBE_MAX_LEN) */
static char *describe_stn_recode( stn_recode *src, char *buffer, int stnwidth )
{
    int nch=0;
    int nch1=0;
    if( ! src->usemark && src->codeto[0] == 0 )
    {
        sprintf( buffer,"ignored");
        nch=7;
    }
    else
    {
        const char *codeto=src->codeto;
        if( ! src->usemark ) codeto--;
        sprintf( buffer, "%-*.*s%n",stnwidth,STNCODELEN+1,codeto,&nch);
    }
    if( src->datefrom != UNDEFINED_DATE && src->dateto != UNDEFINED_DATE )
    {
        sprintf( buffer+nch, " between %s%n",date_as_string(src->datefrom,"DT?",0),&nch1 );
        nch += nch1;
        sprintf( buffer+nch, " and %s%n",date_as_string(src->dateto,"DT?",0),&nch1 );

    }
    else if( src->datefrom != UNDEFINED_DATE )
    {
        sprintf( buffer+nch, " after %s%n",date_as_string(src->datefrom,"DT?",0),&nch1 );
    }
    else if( src->dateto != UNDEFINED_DATE )
    {
        sprintf( buffer+nch, " before %s%n",date_as_string(src->dateto,"DT?",0),&nch1 );
    }
    nch += nch1;
    if( src->herror > 0.0 || src->verror > 0.0 )
    {
        sprintf( buffer+nch, " co-location error %.3lf %.3lf m",
                std::max(std::min(src->herror,9999.999),0.0),
                std::max(std::min(src->verror,9999.999),0.0));
    }
    return buffer;
}

/* Sort function sorts into order that ensures first match is the correct
 * one to use in get_stn_recode, except for "after ###" for which the date
 * order is ascending. 
 */
static int cmp_stn_recode( stn_recode *src0, stn_recode *src1 )
{
    if( src1->datefrom == UNDEFINED_DATE && src1->dateto == UNDEFINED_DATE )
    {
        if( src0->datefrom == UNDEFINED_DATE && src0->dateto == UNDEFINED_DATE )
        {
            return 0;
        }
        else
        {
            return -1;
        }
    }
    if( src0->datefrom == UNDEFINED_DATE && src0->dateto == UNDEFINED_DATE )
    {
        return 1;
    }
    if( src1->dateto == UNDEFINED_DATE )
    {
        if( src0->dateto != UNDEFINED_DATE )
        {
            return -1;
        }
        else
        {
            return CMPFLOAT(src0->datefrom,src1->datefrom);
        }
    }
    if( src0->dateto == UNDEFINED_DATE )
    {
        return 1;
    }
    if( src1->datefrom == UNDEFINED_DATE )
    {
        if( src0->datefrom != UNDEFINED_DATE )
        {
            return 1;
        }
        else
        {
            return CMPFLOAT(src0->dateto,src1->dateto);
        }
    }
    if( src0->dateto == UNDEFINED_DATE )
    {
        return -1;
    }
    int result = CMPFLOAT(src0->datefrom,src1->datefrom);
    if( result == 0 )
    {
        result=CMPFLOAT(src0->dateto,src1->dateto);
    }
    return result;
}

static void update_stn_recode( stn_recode_list *list, stn_recode *src, stn_recode *src_update )
{
    int diffmark=stncodecmp(src->codeto,src_update->codeto);
    if( diffmark || src->usemark != src_update->usemark )
    {
        char errmsg[60+2*STNCODELEN+DESCRIBE_MAX_LEN];
        int nch;
        const char *codeto=src_update->codeto;
        if( ! src_update->usemark ) codeto++;
        sprintf(errmsg,"Overriding recode of %.*s to %.*s with recode to %n",
                STNCODELEN,list->codefrom,STNCODELEN+1,codeto,&nch);
        describe_stn_recode(src,errmsg+nch,0);
        handle_error(INFO_ERROR,errmsg,NO_MESSAGE);
        if( diffmark ) return;
    }
    if( src_update->herror <= 0.0 ) 
    {
        src->herror=0.0;
    }
    else if( src->herror > 0.0 && src_update->herror > src->herror ) 
    {
        src->herror=src_update->herror;
    }
    if( src_update->verror <= 0.0 ) 
    {
        src->verror=0.0;
    }
    else if( src->verror > 0.0 && src_update->verror > src->verror ) 
    {
        src->verror=src_update->verror;
    }
}

static void add_stn_recode_to_list( stn_recode_list *list, stn_recode *trans )
{
    stn_recode **last=&(list->translations);
    int cmp=1;
    if( ! *last )
    {
        *last=trans;
        return;
    }
    while( *last )
    {
        cmp=cmp_stn_recode(*last,trans);
        if( cmp >= 0 ) break;
        last=&((*last)->next);
    }
    if( cmp == 0 )
    {
        update_stn_recode(list,trans,*last);
        trans->next=(*last)->next;
        check_free(*last);
        *last=trans;
    }
    else
    {
        trans->next=*last;
        *last=trans;
    }
    /* Check for incompatible recoding */
    for( stn_recode *src=list->translations; src; src=src->next )
    {
        if( src == trans ) continue;
        if( src->dateto == UNDEFINED_DATE && src->datefrom == UNDEFINED_DATE ) continue;
        if( trans->dateto == UNDEFINED_DATE && trans->datefrom == UNDEFINED_DATE ) continue;
        if( trans->dateto == UNDEFINED_DATE && src->dateto == UNDEFINED_DATE ) continue;
        if( trans->datefrom == UNDEFINED_DATE && src->datefrom == UNDEFINED_DATE ) continue;
        if( src->dateto != UNDEFINED_DATE 
                && trans->datefrom != UNDEFINED_DATE 
                && src->dateto < trans->datefrom 
                ) continue;
        if( trans->dateto != UNDEFINED_DATE 
                && src->datefrom != UNDEFINED_DATE 
                && trans->dateto < src->datefrom 
                ) continue;
        char errmsg[40+STNCODELEN+2*DESCRIBE_MAX_LEN];
        sprintf(errmsg,"Recode of %.*s to ", STNCODELEN,list->codefrom);
        describe_stn_recode(trans,errmsg+strlen(errmsg),0);
        strcat(errmsg," conflicts with ");
        describe_stn_recode(src,errmsg+strlen(errmsg),0);
        handle_error(INFO_ERROR,errmsg,NO_MESSAGE);
    }
    return;
}


static void delete_stn_recode_list( stn_recode_list *list )
{
    stn_recode *sct=list->translations;
    while( sct )
    {
        stn_recode *next=sct->next;
        check_free( sct );
        sct=next;
    }
    check_free( list );
}


stn_recode_map *create_stn_recode_map( network *net )
{
    stn_recode_map *stt=(stn_recode_map *) check_malloc( sizeof(stn_recode_map));
    stt->stlists=0;
    stt->index=0;
    stt->global=0;
    stt->used=0;
    stt->net=net;
    return stt;
}

int recodes_used( stn_recode_map *stt )
{
    return stt->used;
}

void delete_stn_recode_map( stn_recode_map *stt ) {
    stn_recode_list *lists=stt->stlists;
    while( lists )
    {
        stn_recode_list *next=lists->next;
        delete_stn_recode_list(lists);
        lists=next;
    }
    if( stt->index ) check_free( stt->index );
    if( stt->global ) delete_stn_recode_list(stt->global);
    stt->index=0;
    stt->stlists=0;
    check_free( stt );
}

static void index_recode_map( stn_recode_map *stt )
{
    stn_recode_list *stlist=stt->stlists;
    int nstlist=0;
    if( stt->index ) return;
    if( ! stlist ) return; 
    while( stlist ){ nstlist++; stlist=stlist->next; }
    stt->index=(stn_recode_list **)check_malloc( nstlist * (sizeof(stn_recode_list *)));
    stt->nindex=nstlist;
    stlist=stt->stlists;
    nstlist=0;
    while( stlist )
    { 
        stt->index[nstlist]=stlist;
        nstlist++; 
        stlist=stlist->next; 
    }
}

static int srmcodecmp( const void *code, const void *st )
{
    const char *s1 = (char *) code;
    const char *s2 = (*(stn_recode_list **)st)->codefrom;
    return stncodecmp( s1, s2 );
}

static stn_recode_list * lookup_station_recode( stn_recode_map *stt, const char *code )
{
    stn_recode_list **match;
    if ( ! stt || ! stt->stlists ) return 0;
    if( ! stt->index ) index_recode_map( stt );
    match = (stn_recode_list **) bsearch( code, stt->index, stt->nindex, 
            sizeof(stn_recode_map *), srmcodecmp );
    return match ? *match : 0;
}

static void add_stn_recode_to_map_err( stn_recode_map *stt, const char *codefrom, const char *codeto, double datefrom, double dateto, double herror, double verror )
{
    stn_recode_list *stlist=stt->stlists;
    stn_recode *src;
    int global=_stricmp(codefrom,RECODE_IGNORE_CODE) == 0 ? 1 : 0;
    /* Global recoding only applies for ignoring codes */
    if( global && _stricmp(codeto,RECODE_IGNORE_CODE) != 0 ) return;
    if( global )
    {
        if( ! stt->global )
        {
            stt->global=create_stn_recode_list( codefrom );
        }
        stlist=stt->global;
    }
    else
    {
        stn_recode_list **stref=&(stt->stlists);
        stlist=(*stref);
        while( stlist )
        {
            int cmp=stncodecmp((*stref)->codefrom,codefrom);
            if( cmp == 0 ) break;
            if( cmp > 0 ) { stlist=0; break; }
            stref=&(stlist->next);
            stlist=(*stref);
        }
        if( ! stlist )
        {
            stlist=create_stn_recode_list( codefrom );
            stlist->next=(*stref);
            (*stref)=stlist;
            if( stt->index ) { check_free( stt->index ); stt->index=0; }
        }
    }
    src=create_stn_recode( codeto, datefrom, dateto, herror, verror );
    add_stn_recode_to_list( stlist, src );
}

void add_stn_recode_to_map( stn_recode_map *stt, const char *codefrom, const char *codeto, double datefrom, double dateto )
{
    add_stn_recode_to_map_err( stt, codefrom, codeto, datefrom, dateto, 0.0, 0.0 );
}

typedef struct 
{
    stn_recode_map *srm;
    char *suffix;
    double datefrom;
    double dateto;
    double herror;
    double verror;
} stn_recode_suffix_data;


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

int read_station_recode_definition( stn_recode_map *stt, char *def, char *basefile )
{
    char msg[80+MAX_FILENAME_LEN];
    std::string codefrom;
    std::string codeto;
    double datefrom;
    double dateto;
    std::string suffix;
    bool have_suffix=false;
    int nenu=0;
    double herror=0.0;
    double verror=0.0;
    int ok=1;

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
        ok=0;
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
        int sts;
        if( ! filenameField )
        {
            strcpy(msg,"Filename missing in station recode");
            ok=0;
        }
        else
        {
            std::string filename(*filenameField);
            sts=read_station_recode_file( stt, filename.c_str(), basefile );
            if( sts != OK )
            {
                sprintf(msg,"Error reading station recode file %.*s",MAX_FILENAME_LEN,filename.c_str());
                ok=0;
            }
        }
        if( ! ok )
        {
            handle_error(INVALID_DATA,"Error reading station recoding",msg);
        }
        return ok ? OK : INVALID_DATA;
    }
    else if ( boost::algorithm::iequals(*field,"suffix") )
    {
        auto suffixField=scanner.next();
        if( ! suffixField )
        {
            strcpy(msg,"Suffix missing in station recode");
            ok=0;
        }
        else
        {
            suffix.assign(*suffixField);
            have_suffix=true;
        }
    }
    else
    {
        codefrom.assign(*field);
        field=scanner.next();
        if( field && boost::algorithm::iequals(*field,"to") ) field=scanner.next();
        if( ! field )
        {
            strcpy(msg,"Missing target station code in recode definition");
            ok=0;
        }
        else
        {
            codeto.assign(*field);
        }
        if( boost::algorithm::iequals(codefrom,RECODE_IGNORE_CODE) && ! boost::algorithm::iequals(codeto,RECODE_IGNORE_CODE) )
        {
            sprintf(msg,"Cannot recode from %.20s to %.20s",codefrom.c_str(),codeto.c_str());
            ok=0;
        }
    }

    field=scanner.next();
    herror=verror=0.0;
    if( ok && field )
    {
        if( boost::algorithm::iequals(*field,"disconnected") )
        {
            field=scanner.next();
        }
        else if( boost::algorithm::iequals(*field,"hv_error") )
        {
            while( ok)
            {
                auto errorField=scanner.next();
                if( ! errorField ) break;
                if( boost::algorithm::iequals(*errorField,"m") || boost::algorithm::iequals(*errorField,"metres") )
                {
                    break;
                }
                else
                {
                    if( nenu > 1 )
                    {
                        strcpy(msg,"Missing m (metres) at end of hv_error in recode definition");
                        ok=0;
                        break;
                    }
                    if( ! parse_positive_double( *errorField, verror ) )
                    {
                        std::string errorText(*errorField);
                        sprintf(msg,"Invalid hv_error %s in recode definition",errorText.c_str());
                        ok=0;
                        break;
                    }
                    if( nenu == 0 ) herror=verror;
                    nenu++;
                }
            }
            if( ok && nenu == 0 )
            {
                strcpy(msg,"Missing hv_error in recode definition");
                ok=0;
            }
            field=scanner.next();
        }
    }

    datefrom=UNDEFINED_DATE;
    dateto=UNDEFINED_DATE;
    if( ok && field )
    {
        std::optional<std::string> fromdef;
        std::optional<std::string> todef;
        if( boost::algorithm::iequals(*field,"between") )
        {
            auto fromField=scanner.next();
            if( ! fromField )
            {
                strcpy(msg,"Date missing after \"between\"");
                ok=0;
            }
            else
            {
                fromdef=std::string(*fromField);
            }
            if( ok )
            {
                field=scanner.next();
                if( ! ( field && boost::algorithm::iequals(*field,"and") ) )
                {
                    strcpy(msg,"\"and\" missing after \"between\"");
                    ok=0;
                }
            }
            if( ok )
            {
                auto toField=scanner.next();
                if( ! toField )
                {
                    strcpy(msg,"Date missing after \"and\"");
                    ok=0;
                }
                else
                {
                    todef=std::string(*toField);
                }
            }
            if( ok && nenu > 0 )
            {
                strcpy(msg,"Cannot use \"hv_error\" and \"between\" in recode definition");
                ok=0;

            }
        }
        else if( boost::algorithm::iequals(*field,"before") )
        {
            auto toField=scanner.next();
            if( ! toField )
            {
                strcpy(msg,"Date missing after \"before\"");
                ok=0;
            }
            else
            {
                todef=std::string(*toField);
            }
        }
        else if( boost::algorithm::iequals(*field,"after") )
        {
            auto fromField=scanner.next();
            if( ! fromField )
            {
                strcpy(msg,"Date missing after \"after\"");
                ok=0;
            }
            else
            {
                fromdef=std::string(*fromField);
            }
        }
        else
        {
            std::string fieldText(*field);
            sprintf(msg,"Undefined field %.50s",fieldText.c_str());
            ok=0;
        }
        if( ok && fromdef )
        {
            datefrom=snap_datetime_parse( fromdef->c_str(), nullptr );
            if( ! datefrom )
            {
                sprintf(msg,"Invalid from date \"%.50s\"",fromdef->c_str());
                ok=0;
            }
        }
        if( ok && todef )
        {
            dateto=snap_datetime_parse( todef->c_str(), nullptr );
            if( ! dateto )
            {
                sprintf(msg,"Invalid to date \"%.50s\"",todef->c_str());
                ok=0;
            }
            else if( datefrom != UNDEFINED_DATE && datefrom >= dateto )
            {
                sprintf(msg,"Start date \"%.20s\" and end date \"%.20s\" inconsistent",fromdef ? fromdef->c_str() : "",todef->c_str());
                ok=0;
            }
        }
    }

    if( have_suffix && ok )
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
            ok=0;
        }
        else
        {
            stn_recode_suffix_data srd;
            srd.srm=stt;
            srd.suffix=suffix.data();
            srd.datefrom=datefrom;
            srd.dateto=dateto;
            srd.herror=herror;
            srd.verror=verror;
            process_selected_stations( stt->net, stationList.c_str(), basefile, &srd, apply_recode_suffix );
        }
    }
    else if( ok )
    {
        add_stn_recode_to_map_err( stt, codefrom.c_str(), codeto.c_str(), datefrom, dateto, herror, verror );
    }

    if( ! ok )
    {
        handle_error(INVALID_DATA,"Error reading station recoding",msg);
    }
    return ok ? OK : INVALID_DATA;
}

void print_stn_recode_list( FILE *out, stn_recode_map *stt, int onlyused, int stn_name_width, const char *prefix )
{
    char description[DESCRIBE_MAX_LEN];
    if( ! stt ) return;
    if( ! stt->index ) index_recode_map( stt );
    for( int i=0; i<stt->nindex; i++ )
    {
        stn_recode_list *srl=stt->index[i];
        int first=1;
        int show_reloc=0;
        if( onlyused )
        {
            for( stn_recode *src=srl->translations; src; src=src->next )
            {
                if( src->used == RECODE_UNUSED) continue;
                if( src->herror > 0.0 || src->verror > 0.0 )
                {
                    show_reloc=1;
                    break;
                }
            }
        } 
        for( stn_recode *src=srl->translations; src; src=src->next )
        {
            if( onlyused && (src->used == RECODE_UNUSED) ) 
            {
                if( ! show_reloc ) continue;
                if( src->datefrom == UNDEFINED_DATE && src->dateto == UNDEFINED_DATE ) continue;
                if( src->datefrom != UNDEFINED_DATE && src->dateto != UNDEFINED_DATE ) continue;
            }
            if( first )
            {
                first=0;
                fprintf(out,"%s%-*s ", prefix,stn_name_width,srl->codefrom);
            }
            else
            {
                fprintf(out,"%s%-*s ", prefix,stn_name_width," ");
            }
            fprintf(out,"to %s",describe_stn_recode( src, description, stn_name_width ));
            fprintf( out, "\n");
        }
    }
    if( stt->global )
    {
        int first=1;
        for( stn_recode *src=stt->global->translations; src; src=src->next )
        {
            if( onlyused && src->used == RECODE_UNUSED ) continue;
            if( first )
            {
                first=0;
                fprintf(out,"%sOther stations ",prefix);
            }
            else
            {
                fprintf(out,"%s              ", prefix);
            }
            fprintf(out,"to %s",describe_stn_recode( src, description, stn_name_width ));
            fprintf( out, "\n");
        }
    }
}

stn_recode *get_station_recodes( stn_recode_map *stt, const char *code )
{
    stn_recode_list *list=lookup_station_recode( stt, code );
    return list ? list->translations: 0;
}

const char *get_stn_recode( stn_recode_map *stt, const char *code, double date, int *reject )
{
    stn_recode *src=0;
    stn_recode_list *list=lookup_station_recode( stt, code );
    int global;
    if( reject ) *reject=0;
    for( global=0; global<2; global++ )
    {
        if( global ) list=stt->global;
        if( ! list ) continue;
        for( src=list->translations; src; src=src->next )
        {
            if( src->datefrom == UNDEFINED_DATE && src->dateto == UNDEFINED_DATE ) break;
            if( date == UNDEFINED_DATE ) continue;
            if( src->datefrom == UNDEFINED_DATE && date < src->dateto ) break;
            if(date <= src->dateto && date >= src->datefrom ) break;
            if( src->dateto == UNDEFINED_DATE && date >= src->datefrom ) 
            {
                while( src->next && 
                       src->next->datefrom != UNDEFINED_DATE &&
                       date >= src->next->datefrom  )
                {
                    src=src->next;
                }
                break;
            }
        }
        if( src ) break;
    }
    if( ! src ) return 0;
    src->used=RECODE_USED;
    stt->used=RECODE_USED;
    if( reject ) *reject = 1-src->usemark;
    return src->codeto;
}

const char *recoded_network_station( void *recode_data, const char *code, double date )
{
    const char *code1=0;
    const char *code2=0;
    const char *recoded;
    int rej1=0;
    int rej2=0;
    int id;
    stn_recode_data *srd=(stn_recode_data *)recode_data;
    if( ! srd ) return 0;
    if( srd->file_map ) code1=get_stn_recode(srd->file_map,code,date,&rej1);
    if( srd->global_map ) code2=get_stn_recode(srd->global_map,code1 ? code1 : code,date, &rej2);
    if( ! code2 ) { code2 = code1; code1 = 0; }
    if( ! code2 ) return 0;
    recoded=code2;
    if( rej1 || rej2 ) recoded--;  /* Reset pointer to include '*' char */

    if( _stricmp(recoded,RECODE_IGNORE_CODE) != 0 && srd->net )
    {
        id = find_station( srd->net, code2 );
        if( ! id )
        {
            if( code1 ) id = find_station( srd->net, code1 );
            if( ! id ) id = find_station( srd->net, code );
            if( id )
            {
                station *st=station_ptr(srd->net,id);
                duplicate_network_station( srd->net, st, code2, st->Name.c_str() );
            }
        }
    }
    return recoded;
}
