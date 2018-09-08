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
    int64_t loc;
    loc = ftell( bindata_file );
    fwrite( &size, sizeof(size), 1, bindata_file );
    fwrite( &type, sizeof(type), 1, bindata_file );
    if( size > maxsize ) maxsize = size;
    nbindata++;
    return loc;
}

int read_bindata_header( long *size, int *type )
{
    if( fread( size, sizeof( long ), 1, bindata_file ) == 1 &&
            fread( type, sizeof( int ), 1, bindata_file ) == 1 )
    {
        return 1;
    }
    else
    {
        return 0;
    }
}


static void reset_survdata_pointers( survdata *sd );

int get_bindata( bindata *b )
{
    long loc;
    int sts;

    loc = ftell64( bindata_file );

    /* Read the size and type of the next data item */

    sts = fread( &b->size, sizeof(b->size), 1, bindata_file );
    if( sts ) sts = fread( &b->bintype, sizeof(b->bintype), 1, bindata_file );
    if( !sts || b->bintype == ENDDATA ) return NO_MORE_DATA;

    if( b->bintype < 0 || b->bintype > NOBINDATATYPES )
    {
        handle_error( INTERNAL_ERROR,
                      "Program error - invalid binary data type",
                      "Error occurred in get_bindata");

        return INVALID_DATA;
    }

    /* Check that the binary data was successfully read */

    if( sts != 1 ) return NO_MORE_DATA;

    /* Check that we have enough room for the data, and if not allocate more */

    if( b->allocsize < b->size )
    {
        if( b->data ) check_free( b->data );
        b->data = NULL;
        b->allocsize = 0;
    }
    if( !b->data )
    {
        b->data = check_malloc( b->size );
        b->allocsize = b->size;
    }

    if( fread( b->data, b->size, 1, bindata_file ) != 1 ) return FILE_READ_ERROR;
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
    int64_t loc;
    loc = ftell64( bindata_file );
    fseek64( bindata_file, b->loc + sizeof(b->bintype) + sizeof(b->size), SEEK_SET );
    fwrite( b->data, b->size, 1, bindata_file );
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


static long survdata_size( survdata *sd )
{
    return sizeof( survdata ) +
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

long save_survdata( survdata *sd )
{
    int64_t fileloc;
    long loc;
    // Cannot handle locations which are not within the range of long 
    // at the moment (and on MSW 64 long is 4 bytes).  This should be ok as 
    // observations are loaded into the binary file before the covariance 
    // which is the only bit likely to push the size over 2Gb.  For the moment
    // accept this limitation - this can be fixed as/when the snap codebase
    // is overhauled.
    fileloc = write_bindata_header(survdata_size(sd), SURVDATA);
    loc = (long)fileloc;
    if (loc != fileloc)
    {
        loc = 0;
        handle_error( FATAL_ERROR, "Cannot save data in binary file - too much data","");
    }
    fwrite( sd, sizeof(survdata), 1, bindata_file );
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

long save_survdata_subset( survdata *sd, int iobs, int type )
{
    int nobs, nclass, nsyserr;
    int cvrperobs, cvrtype;
    int64_t loc;
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

    {
        int oldnobs, oldnclass, oldnsyserr, oldncvr;
        int64_t fileloc;

        oldnobs = sd->nobs;
        oldnclass = sd->nclass;
        oldnsyserr = sd->nsyserr;
        oldncvr = sd->ncvr;

        sd->nobs = nobs;
        sd->nclass = nclass;
        sd->nsyserr = nsyserr;
        sd->ncvr = cvrperobs * nobs;

        fileloc = write_bindata_header( survdata_size( sd ), SURVDATA );
        loc = (long)fileloc;
        if (loc != fileloc)
        {
            loc = 0;
            handle_error(FATAL_ERROR, "Cannot save data in binary file - too much data", "");
        }

        fwrite( sd, sizeof(survdata), 1, bindata_file );

        sd->nobs = oldnobs;
        sd->nclass = oldnclass;
        sd->nsyserr = oldnsyserr;
        sd->ncvr = oldncvr;
    }

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
    while( get_bindata( b ) == OK )
    {
        int iobs;
        int ncvrrow=0;

        if( b->bintype != SURVDATA ) continue;
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
