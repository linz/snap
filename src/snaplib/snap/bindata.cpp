#include "snapconfig.h"


/*
   $Log: bindata.c,v $
   Revision 1.1  1995/12/22 17:38:46  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define BINDATA_C
#include "util/chkalloc.h"
#include "snapdata/survdata.h"
#include "snap/snapglob.h"
#include "snap/stnadj.h"
#include "snap/bindata.h"
#include "snap/bearing.h"
#include "snap/genparam.h"
#include "snap/rftrans.h"
#include "snap/survfile.h"
#include "util/pi.h"
#include "util/errdef.h"
#include "util/dateutil.h"
#include "util/fileutil.h"
#undef BINDATA_C


static int64_t start_loc;
static long maxsize = 0;

int init_bindata( FILE *f  )
{
    maxsize = 0;
    nbindata = 0;
    bindata_file = f ? f : snaptmpfile();

    if( !bindata_file )
    {
        handle_error( FATAL_ERROR, "Unable to open scratch file", NO_MESSAGE );
        return FATAL_ERROR;
    }
    start_loc = ftell64( bindata_file );
    return OK;
}

void end_bindata( void )
{
    write_bindata_header( 0, ENDDATA );
}

void init_get_bindata(int64_t loc )
{
    if( loc == 0 ) loc = start_loc;
    fseek64( bindata_file, loc, SEEK_SET );
}


int64_t write_bindata_header( long size, int type )
{
    const int64_t loc = ftell64( bindata_file );
    write_raw_as<int64_t>( bindata_file, size );
    write_raw( bindata_file, type );
    if( size > maxsize ) maxsize = size;
    if( type == SURVDATA ) nbindata++;
    return loc;
}

int read_bindata_header( long *size, int *type )
{
    int64_t disksize;
    if( fread( &disksize, sizeof(disksize), 1, bindata_file ) == 1 &&
            fread( type, sizeof( int ), 1, bindata_file ) == 1 )
    {
        *size = static_cast<long>(disksize);
        return 1;
    }
    else
    {
        return 0;
    }
}


static void reset_survdata_pointers( survdata *sd );

struct SurvdataDiskField { FieldKind kind; size_t offset; };

// Single source of truth for the on-disk survdata layout - the 16 genuinely-stored
// fields (`from` through `prmid`, survdata.h), in declared order. survdata's
// integer fields are already plain `int` (4 bytes on every target here), so writing
// them via write_raw_as<int32_t> is an explicit "the disk format is fixed-width
// regardless of the in-memory type" statement, not a narrowing conversion - kept
// uniform with genuinely-`long` fields elsewhere (binfile.h) rather than plain
// write_raw, so every integer-like field in the file format reads the same way.
//
// NOTE: this table must stay in sync with survdata's declared fields (see the
// matching note at survdata.h next to the "not stored in the binary file" comment) -
// adding/removing a field in one place without the other silently desyncs the
// on-disk format from the struct. survdata_disk_fields_contiguous() below verifies
// this at compile time.
static constexpr SurvdataDiskField SURVDATA_DISK_FIELDS[] = {
    { FieldKind::Int32,   offsetof(survdata, from) },
    { FieldKind::Float64, offsetof(survdata, fromhgt) },
    { FieldKind::Float64, offsetof(survdata, date) },
    { FieldKind::Int32,   offsetof(survdata, reffrm) },
    { FieldKind::Int32,   offsetof(survdata, file) },
    { FieldKind::Float64, offsetof(survdata, schval) },
    { FieldKind::Float64, offsetof(survdata, schvar) },
    { FieldKind::Int32,   offsetof(survdata, format) },
    { FieldKind::Int32,   offsetof(survdata, nobs) },
    { FieldKind::Int32,   offsetof(survdata, obssize) },
    { FieldKind::Int32,   offsetof(survdata, ncvr) },
    { FieldKind::Int32,   offsetof(survdata, nclass) },
    { FieldKind::Int32,   offsetof(survdata, nsyserr) },
    { FieldKind::Int32,   offsetof(survdata, options) },
    { FieldKind::Int32,   offsetof(survdata, nprms) },
    { FieldKind::Int32,   offsetof(survdata, prmid) },
};
static constexpr size_t SURVDATA_DISK_FIELD_COUNT = sizeof(SURVDATA_DISK_FIELDS) / sizeof(SURVDATA_DISK_FIELDS[0]);

// Verifies SURVDATA_DISK_FIELDS has no gap relative to survdata's actual memory
// layout: each field's end, rounded up to the next field's required alignment,
// must exactly equal the next field's real offset - and the last tracked field
// must, by the same rule, be immediately followed by the first non-disk member
// (obs). This tolerates genuine compiler-inserted alignment padding (there are
// two such gaps in survdata today, both exactly accounted for by alignment) while
// still catching a field added, removed, or reordered anywhere in the tracked
// region or right after it. It can't distinguish a hidden field from padding in
// the one case where that field's size exactly equals the alignment gap it sits
// in - a narrower residual risk than not checking at all.
static constexpr bool survdata_disk_fields_contiguous()
{
    for (size_t i = 0; i + 1 < SURVDATA_DISK_FIELD_COUNT; ++i)
    {
        const size_t end = SURVDATA_DISK_FIELDS[i].offset + field_in_memory_size(SURVDATA_DISK_FIELDS[i].kind);
        const size_t expected_next = round_up(end, field_in_memory_alignment(SURVDATA_DISK_FIELDS[i+1].kind));
        if (expected_next != SURVDATA_DISK_FIELDS[i+1].offset) return false;
    }
    const SurvdataDiskField &last = SURVDATA_DISK_FIELDS[SURVDATA_DISK_FIELD_COUNT-1];
    const size_t last_end = last.offset + field_in_memory_size(last.kind);
    const size_t expected_obs = round_up(last_end, alignof(decltype(survdata::obs)));
    return expected_obs == offsetof(survdata, obs);
}
// Runs entirely at compile time: if this ever fails, the build fails rather than
// the program misbehaving at runtime, and it costs nothing in the compiled binary
// either way - the check itself never executes outside the compiler.
static_assert(survdata_disk_fields_contiguous(),
    "SURVDATA_DISK_FIELDS has a gap relative to survdata's actual layout - a field "
    "was likely added, removed, or reordered in survdata.h without updating this table");

// Disk footprint of the fixed-width portion: the sum of each field's on-disk
// width (all int32_t/double here, both fixed across every target), independent
// of any in-memory padding between them.
static size_t survdata_disk_fixed_width_size()
{
    size_t total = 0;
    for (const auto &field : SURVDATA_DISK_FIELDS)
        total += field_in_memory_size(field.kind);
    return total;
}
static const size_t SURVDATA_DISK_FIXED_WIDTH_SIZE = survdata_disk_fixed_width_size();

// Writes the 16 tracked fields in table order through the fixed-width disk-cast
// templates from binfile.h - the only part of survdata serialized field-by-field;
// everything after it (obs array, classes, syserrs, covariances) is a raw
// fixed-stride copy, already portable without per-field handling.
static void write_survdata_fixed_width( const survdata &sd, FILE *f )
{
    const char *base = reinterpret_cast<const char*>(&sd);
    for (const auto &field : SURVDATA_DISK_FIELDS)
    {
        if (field.kind == FieldKind::Float64)
            write_raw(f, *reinterpret_cast<const double*>(base + field.offset));
        else
            write_raw_as<int32_t>(f, *reinterpret_cast<const int*>(base + field.offset));
    }
}

// Mirrors write_survdata_fixed_width: same table, same iteration order, reading
// into *reinterpret_cast<double*/int*>(base + field.offset) instead of writing.
static void read_survdata_fixed_width( FILE *f, survdata &sd )
{
    char *base = reinterpret_cast<char*>(&sd);
    for (const auto &field : SURVDATA_DISK_FIELDS)
    {
        if (field.kind == FieldKind::Float64)
            read_raw(f, *reinterpret_cast<double*>(base + field.offset));
        else
            read_raw_as<int32_t>(f, *reinterpret_cast<int*>(base + field.offset));
    }
}

int get_bindata( int bintype, bindata *b )
{
    int64_t loc;
    int sts;


    /* Read the size and type of the next data item */

    while( 1 )
    {
        loc = ftell64( bindata_file );
        sts = fread( &b->size, sizeof(b->size), 1, bindata_file );
        if( sts ) sts = fread( &b->bintype, sizeof(b->bintype), 1, bindata_file );
        if( !sts || b->bintype == ENDDATA ) 
        {
            fseek64( bindata_file, loc, SEEK_SET );
            return NO_MORE_DATA;
        }
        if( b->bintype < 0 || b->bintype > NOBINDATATYPES )
        {
            handle_error( INTERNAL_ERROR,
                          "Program error - invalid binary data type",
                          "Error occurred in get_bindata");

            return INVALID_DATA;
        }
        if( b->bintype == bintype || bintype == ANYDATATYPE ) break;
        fseek64(bindata_file, b->size, SEEK_CUR);
    }

    /* Check that we have enough room for the data, and if not allocate more.
       SURVDATA's in-memory footprint (sizeof(survdata) + variable-width tail)
       differs from its on-disk size (SURVDATA_DISK_FIXED_WIDTH_SIZE + the same
       tail), since the fixed-width header is smaller on disk than sizeof(survdata)
       once padding is excluded - everything else after the header is already
       fixed-width per-element and needs no such adjustment. */

    const int64_t reqsize = (b->bintype == SURVDATA)
        ? static_cast<int64_t>(sizeof(survdata)) + (b->size - static_cast<int64_t>(SURVDATA_DISK_FIXED_WIDTH_SIZE))
        : b->size;

    if( b->allocsize < reqsize )
    {
        if( b->data ) check_free( b->data );
        b->data = NULL;
        b->allocsize = 0;
    }
    if( !b->data )
    {
        b->data = check_malloc( reqsize );
        b->allocsize = reqsize;
    }

    if( b->bintype == SURVDATA )
    {
        survdata *sd = reinterpret_cast<survdata*>(b->data);
        memset( sd, 0, sizeof(survdata) );
        read_survdata_fixed_width( bindata_file, *sd );
        const int64_t variable_size = b->size - static_cast<int64_t>(SURVDATA_DISK_FIXED_WIDTH_SIZE);
        if( variable_size > 0 )
        {
            unsigned char *variable = reinterpret_cast<unsigned char*>(b->data) + sizeof(survdata);
            if( fread( variable, variable_size, 1, bindata_file ) != 1 ) return FILE_READ_ERROR;
        }
    }
    else
    {
        if( fread( b->data, b->size, 1, bindata_file ) != 1 ) return FILE_READ_ERROR;
    }
    b->loc = loc;

    switch (b->bintype)
    {
    case SURVDATA: reset_survdata_pointers( (survdata *) b->data );
        break;

    case NOTEDATA: break;

    default:      handle_error( INTERNAL_ERROR,
                                    "Program error - invalid binary data type",
                                    "Error occurred in get_bindata");
        break;
    }

    return OK;
}

void update_bindata( bindata *b )
{
    const int64_t loc = ftell64( bindata_file );
    fseek64( bindata_file, b->loc + sizeof(b->bintype) + sizeof(b->size), SEEK_SET );
    if( b->bintype == SURVDATA )
    {
        const survdata *sd = reinterpret_cast<const survdata*>(b->data);
        write_survdata_fixed_width( *sd, bindata_file );
        const int64_t variable_size = b->size - static_cast<int64_t>(SURVDATA_DISK_FIXED_WIDTH_SIZE);
        if( variable_size > 0 )
        {
            const unsigned char *variable = reinterpret_cast<const unsigned char*>(b->data) + sizeof(survdata);
            fwrite( variable, variable_size, 1, bindata_file );
        }
    }
    else
    {
        fwrite( b->data, b->size, 1, bindata_file );
    }
    fseek64( bindata_file, loc, SEEK_SET );
}


bindata *create_bindata( void  )
{
    bindata *b;
    b = (bindata *) check_malloc( sizeof(bindata) );
    if( maxsize > 0 )
    {
        b->data = check_malloc( maxsize );
        b->allocsize = maxsize;
    }
    else
    {
        b->data = NULL;
        b->allocsize = 0;
    }
    return b;
}

void delete_bindata( bindata *b )
{
    if( b->data ) check_free( b->data );
    check_free( b );
}




/* Routines for saving the survdata structure.
   All routines save
     1) the survdata structure
     2) the observation data
     3) the classifications of the data
     4) the systematic errors of the data
     5) the covariance of the observations

   The two routines for saving the data are:
     1) saving all the data
     2) saving a selected subset

   When the data is loaded back from the binary file it is dumped
   into a contiguous block of memory.  The internal pointers in the
   survdata structure are reset by assuming the order and size of the
   components within this structure.

   */



// On-disk record size: the fixed-width header above, plus each variable-width
// section at its own already-fixed-width per-element size (obs array, classes,
// syserrs, then the three ncvr-sized lower-triangular covariance blocks).
static long survdata_size( survdata *sd )
{
    return SURVDATA_DISK_FIXED_WIDTH_SIZE +
           sd->nobs * sd->obssize +
           sd->nclass * sizeof( classdata ) +
           sd->nsyserr * sizeof( syserrdata ) +
           (sd->ncvr ? (3L * sd->ncvr * (sd->ncvr+1)*sizeof(double)/2) : 0);
}

/*
   Save a subset of the data.  The subset is based upon
     a) iobs.  If iobs >= 0 then only the iobs observation is stored.
     b) type.  If type >= 0 then only observations of specified type
	       are stored.
     c) The unused flag.  If IGNORE_OBS_BIT is set, then the obs is
	       not saved.
     */

int64_t save_survdata( survdata *sd )
{
    const int64_t loc = write_bindata_header(survdata_size(sd), SURVDATA);
    write_survdata_fixed_width( *sd, bindata_file );
    fwrite( sd->obs.odata, sd->obssize, sd->nobs, bindata_file );
    if( sd->nclass )
    {
        fwrite( sd->clsf, sizeof(classdata), sd->nclass, bindata_file );
    }
    if( sd->nsyserr )
    {
        fwrite( sd->syserr, sizeof(syserrdata), sd->nsyserr, bindata_file );
    }
    if( sd->ncvr )
    {
        int cvrsize;
        cvrsize = ((sd->ncvr) * (sd->ncvr+1))/2;
        fwrite( sd->cvr, sizeof(double), cvrsize, bindata_file );
        fwrite( sd->calccvr, sizeof(double), cvrsize, bindata_file );
        fwrite( sd->rescvr, sizeof(double), cvrsize, bindata_file );
    }
    if( ! have_obs_ids )
    {
        for( int i=0; i<sd->nobs; i++ )
        {

            if( get_trgtdata(sd,i)->id != 0 )
            {
                have_obs_ids = 1;
                break;
            }
        }
    }
    return loc;
}

int64_t save_survdata_subset( survdata *sd, int iobs, int type )
{
    int nobs, nclass, nsyserr;
    int cvrperobs, cvrtype;
    int i, i0, i1;

    /* Determine the possible range of observations */

    if( iobs < 0 )
    {
        i0 = 0;
        i1 = sd->nobs;
    }
    else if( iobs >= sd->nobs )
    {
        return 0L;
    }
    else
    {
        i0 = iobs;
        i1 = iobs+1;
    }

    /* Count the observations to be loaded */

    nobs = 0;
    nclass = 0;
    nsyserr = 0;

    for( i=i0; i<i1; i++ )
    {
        trgtdata *t;
        t = get_trgtdata( sd, i );
        if( type >= 0 && t->type != type ) continue;
        if( t->unused & IGNORE_OBS_BIT ) continue;
        if( t->id != 0 ) have_obs_ids = 1;
        nobs++;
        nclass += t->nclass;
        nsyserr += t->nsyserr;
    }

    if( nobs < 0 ) return 0L;

    /* Save the header */

    cvrperobs = sd->ncvr/sd->nobs;
    if( !sd->cvr ) cvrperobs = 0;

    const int oldnobs = sd->nobs;
    const int oldnclass = sd->nclass;
    const int oldnsyserr = sd->nsyserr;
    const int oldncvr = sd->ncvr;

    sd->nobs = nobs;
    sd->nclass = nclass;
    sd->nsyserr = nsyserr;
    sd->ncvr = cvrperobs * nobs;

    const int64_t loc = write_bindata_header( survdata_size( sd ), SURVDATA );

    write_survdata_fixed_width( *sd, bindata_file );

    sd->nobs = oldnobs;
    sd->nclass = oldnclass;
    sd->nsyserr = oldnsyserr;
    sd->ncvr = oldncvr;

    /* Save the observations */

    {
        unsigned char *data;
        int iclass, isyserr;
        int oldiclass, oldisyserr;

        data = (unsigned char *) (void *) sd->obs.odata;
        data += (i0*sd->obssize);

        iclass = 0;
        isyserr = 0;

        for( i = i0; i < i1; i++, data += sd->obssize )
        {
            trgtdata *t;
            t = get_trgtdata( sd, i );
            if( type >= 0 && t->type != type ) continue;
            if( t->unused & IGNORE_OBS_BIT ) continue;
            oldiclass = t->iclass;
            oldisyserr = t->isyserr;
            t->iclass = iclass;
            t->isyserr = isyserr;
            iclass += t->nclass;
            isyserr += t->nsyserr;

            fwrite( data, sd->obssize, 1, bindata_file );

            t->iclass = oldiclass;
            t->isyserr = oldisyserr;
        }
    }

    /* Now dump the classifications and systematic errors */

    if( nclass )
    {

        for( i = i0; i < i1; i++ )
        {
            trgtdata *t;
            t = get_trgtdata( sd, i );
            if( type >= 0 && t->type != type ) continue;
            if( t->unused & IGNORE_OBS_BIT ) continue;

            fwrite( sd->clsf+t->iclass, sizeof(classdata), t->nclass, bindata_file );
        }
    }

    if( nsyserr )
    {

        for( i = i0; i < i1; i++ )
        {
            trgtdata *t;
            t = get_trgtdata( sd, i );
            if( type >= 0 && t->type != type ) continue;
            if( t->unused & IGNORE_OBS_BIT ) continue;

            fwrite( sd->syserr+t->isyserr, sizeof(syserrdata), t->nsyserr, bindata_file );
        }

    }

    /* And finally the covariances */

    if( cvrperobs ) for( cvrtype = 0; cvrtype < 3; cvrtype++ )
        {
            int ir0, ir1, ir, njr, j, jr0;
            ltmat cvr = 0;
            switch( cvrtype )
            {
            case 0: cvr = sd->cvr; break;
            case 1: cvr = sd->calccvr; break;
            case 2: cvr = sd->rescvr; break;
            }

            for( i = i0; i < i1; i++ )
            {
                trgtdata *t;
                t = get_trgtdata( sd, i );
                if( type >= 0 && t->type != type ) continue;
                if( t->unused & IGNORE_OBS_BIT ) continue;
                ir0 = i*cvrperobs;
                ir1 = ir0+cvrperobs;
                for( ir = ir0; ir < ir1; ir++ )
                {
                    njr = cvrperobs;
                    for( j = i0; j <= i; j++ )
                    {

                        if( j != i )
                        {
                            trgtdata *t;
                            t = get_trgtdata( sd, j );
                            if( type >= 0 && t->type != type ) continue;
                            if( t->unused & IGNORE_OBS_BIT ) continue;
                            jr0 = j * cvrperobs;
                            njr=cvrperobs;
                        }
                        else
                        {
                            jr0 = ir0;
                            njr = ir-ir0+1;
                        }
                        fwrite( &Lij(cvr,ir,jr0), sizeof(double), njr, bindata_file );
                    }
                }
            }
        }
    return loc;
}


/* Reset the pointers in a survdata entity, assuming that the data is
   located in a contiguous block of memory */

static void reset_survdata_pointers( survdata *sd )
{
    unsigned char *data;
    data = (unsigned char *) (void *) sd;
    data += sizeof( survdata );
    switch( sd->format )
    {
    case SD_OBSDATA: sd->obs.odata = (obsdata *) (void *) data; break;
    case SD_VECDATA: sd->obs.vdata = (vecdata *) (void *) data; break;
    case SD_PNTDATA: sd->obs.pdata = (pntdata *) (void *) data; break;
    }
    data += sd->nobs * sd->obssize;
    if( sd->nclass )
    {
        sd->clsf = (classdata *) data;
        data += sd->nclass * sizeof( classdata );
    }
    else
    {
        sd->clsf = NULL;
    }
    if( sd->nsyserr )
    {
        sd->syserr = (syserrdata *) data;
        data += sd->nsyserr * sizeof( syserrdata );
    }
    else
    {
        sd->syserr = NULL;
    }
    if( sd->ncvr )
    {
        int cvrsize;
        cvrsize = (sd->ncvr * (sd->ncvr+1))/2 * sizeof(double);
        sd->cvr = (ltmat) (void *) data;
        data += cvrsize;
        sd->calccvr = (ltmat) (void *) data;
        data += cvrsize;
        sd->rescvr = (ltmat) (void *) data;
    }
    else
    {
        sd->cvr = sd->calccvr = sd->rescvr = (ltmat) 0;
    }
}

char *get_obs_classification_name( survdata *sd, trgtdata *t, int class_id )
{
    int ic;
    for( ic = 0; ic < t->nclass; ic++ )
    {
        classdata *cd;
        cd = sd->clsf + ic + t->iclass;
        if( cd->class_id == class_id )
        {
            return class_value_name( &obs_classes, class_id, cd->name_id );
        }
    }
    return NULL;
}

void print_json_observation_types( FILE *out )
{
    int first=1;
    fprintf(out,"{\n");
    for( int type=0; type<NOBSTYPE; type++ ) if( obstypecount[type] )
    {
        if( first ) { first=0; } else { fprintf(out,","); }
        fprintf(out,"\n  \"%s\": \"%s\"",datatype[type].code,datatype[type].name);
    }
    fprintf(out,"\n}\n");
}

void print_json_observations( FILE *out )
{
    bindata *b = create_bindata();
    init_get_bindata( 0L );
    fprintf(out,"[");
    int first=1;
    while( get_bindata( SURVDATA, b ) == OK )
    {
        int iobs;
        int ncvrrow=0;
        survdata *sd = (survdata *) b->data;
        if( first ) { first=0; } else { fprintf(out,","); }
        fprintf( out, "\n  {\n  \"obs\":\n    [" );
        for( iobs=0; iobs < sd->nobs; iobs++ )
        {
            const char *fromstr="from";
            const char *tostr="to";
            const char *totype=fromstr;
            double *value=0;
            double *error=0;
            int nvalue=0;
            trgtdata *tgt=get_trgtdata(sd,iobs);

            if( iobs ) fprintf(out,",");
            fprintf( out, "\n      {\n");
            fprintf( out, "        \"obsid\":%d,\n",tgt->obsid);
            fprintf( out, "        \"srcid\":%d,\n",tgt->id);
            /* sd->from is 0 for point vector data */
            /* Could be more rigorous here! */
            if( sd->from )
            {
                fprintf( out, "        \"from\":\"%s\",\n",station_code(sd->from));
                fprintf( out, "        \"from_hgt\":%.4lf,\n",sd->fromhgt);
                totype=tostr;
            }
            if( tgt->to )
            {
                fprintf( out, "        \"%s\":\"%s\",\n",totype,station_code(tgt->to));
            }
            else
            {
                fprintf( out, "        \"%s\":null,\n",totype);
            }
            fprintf( out, "        \"%s_hgt\":%.4lf,\n",totype,tgt->tohgt);
            if( sd->date == UNDEFINED_DATE )
            {
                fprintf( out, "        \"date\":null,\n");
            }
            else
            {
                fprintf( out, "        \"date\":\"%s\",\n",date_as_string(sd->date,0,0));
            }
            fprintf( out, "        \"type\":\"%s\",\n",datatype[tgt->type].code);
            fprintf( out, "        \"errfct\":%.4lf,\n",tgt->errfct);

            switch( sd->format )
            {
            case SD_OBSDATA:
                {
                    obsdata *o=(obsdata *)(void *)tgt;
                    value=&(o->value);
                    error=&(o->error);
                    nvalue=1;
                }
                break;
            case SD_VECDATA:
                {
                    vecdata *v=(vecdata *)(void *)tgt;
                    value=&(v->vector[0]);
                    nvalue=3;
                    ncvrrow+=3;
                }
                break;
            case SD_PNTDATA:
                {
                    pntdata *p=(pntdata *)(void *)tgt;
                    value=&(p->value);
                    error=&(p->error);
                    nvalue=1;
                }
                break;
            }
            if( nvalue )
            {
                int ivalue;
                double factor=1.0;
                int ndp=datatype[tgt->type].dfltndp+2;
                if( datatype[tgt->type].isangle ) { ndp+=4; factor=RTOD; }
                fprintf( out, "        \"value\": [");
                for( ivalue=0; ivalue<nvalue; ivalue++ )
                {
                    if( ivalue ) fprintf(out,",");
                    fprintf( out, "%.*lf", ndp,value[ivalue]*factor);
                }
                fprintf( out, "],\n");
                if( error )
                {
                    fprintf( out, "        \"error\": [%.*lf],\n",
                            ndp+2,(*error)*factor);
                }
            }
            if( tgt->type == PB )
            {
                fprintf( out, "        \"projection\": \"%s\",\n", bproj_name(sd->reffrm));
            }
            if( sd->format == SD_VECDATA )
            {
                fprintf( out, "        \"ref_frame\": \"%s\",\n", 
                                  rftrans_name(rftrans_from_id(sd->reffrm)));
            }
            if( tgt->nclass )
            {
                int iclass;
                fprintf( out, "        \"classifications\": {");
                for( iclass=0; iclass < tgt->nclass; iclass++ )
                {
                    classdata *clsf=sd->clsf+iclass+tgt->iclass;
                    if( iclass ) fprintf(out,",");
                    fprintf( out, "\n          \"%s\":\"%s\"",
                            classification_name(&obs_classes,clsf->class_id),
                            class_value_name(&obs_classes,clsf->class_id,clsf->name_id));
                }
                fprintf( out, "\n          },\n");
            }
            if( tgt->nsyserr )
            {
                int isyserr;
                fprintf( out, "        \"systematic_errors\": {");
                for( isyserr=0; isyserr < tgt->nsyserr; isyserr++ )
                {
                    syserrdata *syserr=sd->syserr+isyserr+tgt->isyserr;
                    if( isyserr ) fprintf(out,",");
                    fprintf( out, "\n          \"%s\":%.8le",
                            param_type_name(PRM_SYSERR, syserr->prm_id), 
                            syserr->influence);
                }
                fprintf( out, "\n          },\n");
            }
            fprintf( out, "        \"useobs\":%s,\n",tgt->unused ? "false" : "true");
            fprintf( out, "        \"file\":\"%s\",\n",survey_data_file_name(sd->file));
            fprintf( out, "        \"file_line_no\":%d\n",tgt->lineno);
            fprintf( out, "      }");
        }
        fprintf( out, "\n    ]" );
        if( ncvrrow )
        {
            fprintf( out, ",\n  \"covariance\": " );
            print_ltmat_json(out,sd->cvr,ncvrrow,"%15.8le",4);
        }
        fprintf( out, "\n  }");
    }
    fprintf(out,"\n]\n");
    delete_bindata(b);
}
