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
#include "snap/bindata.h"
#include "util/errdef.h"
#include "util/fileutil.h"
#undef BINDATA_C


static long start_loc;
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
    start_loc = ftell( bindata_file );
    return OK;
}

void end_bindata( void )
{
    write_bindata_header( 0, ENDDATA );
}

void init_get_bindata( long loc )
{
    if( loc == 0 ) loc = start_loc;
    fseek( bindata_file, loc, SEEK_SET );
}


long write_bindata_header( long size, int type )
{
    long loc;
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

    loc = ftell( bindata_file );

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
    long loc;
    loc = ftell( bindata_file );
    fseek( bindata_file, b->loc + sizeof(b->bintype) + sizeof(b->size), SEEK_SET );
    fwrite( b->data, b->size, 1, bindata_file );
    fseek( bindata_file, loc, SEEK_SET );
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
    long loc;
    loc = write_bindata_header( survdata_size( sd ), SURVDATA );
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
    long loc;
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

        oldnobs = sd->nobs;
        oldnclass = sd->nclass;
        oldnsyserr = sd->nsyserr;
        oldncvr = sd->ncvr;

        sd->nobs = nobs;
        sd->nclass = nclass;
        sd->nsyserr = nsyserr;
        sd->ncvr = cvrperobs * nobs;

        loc = write_bindata_header( survdata_size( sd ), SURVDATA );

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
                            t = get_trgtdata( sd, i );
                            if( type >= 0 && t->type != type ) continue;
                            if( t->unused & IGNORE_OBS_BIT ) continue;
                            jr0 = j * cvrperobs;
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
