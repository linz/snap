/*************************************************************************
**
**  Filename:    %M%
**
**  Version:     %I%
**
**  What string: %W%
**
** $Id: dbl4_utl_shiftmodel.c,v 1.3 2005/04/04 23:57:58 ccrook Exp $
**//**
** \file
**      Functions for evaluating the a LINZ shift model
**      accessed via a binary source object.
**
**      The shift model defines a "rubber sheeting" type shift that is to be
**      applied to a set of points.  It comprises one or more components, each of
**      which may be a grid or triangulation, and may define horizontal, vertical or
**      3d offsets.
**
**      A shift model can be constructed using the perl program makelinzshiftmodel.pl
**      in the dbl4u folder
**
**      The main components used in this code are:
**         ShiftMod      The shift model
**         ShiftCmp      A component in the sequence
**
**      The binary format is portable between SUN and Intel DOS/Windows
**      environments which differ only in endianness
**
*************************************************************************
*/

static char sccsid[] = "%W%";

#include "dbl4_common.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "dbl4_utl_shiftmodel.h"
#include "dbl4_utl_trig.h"
#include "dbl4_utl_grid.h"

#include "dbl4_utl_alloc.h"
#include "dbl4_utl_error.h"

#define DTOR (M_PI/180)

typedef enum { defModelGrid, defModelTrig } ShiftModelType;

typedef struct
{
    double xmin;
    double ymin;
    double xmax;
    double ymax;
} CrdRange, *hCrdRange;

typedef struct s_ShiftCmp
{
    int id;
    char *description;
    CrdRange range;
    int dimension;
    int negative;
    ShiftModelType type;
    INT4 offset;
    hBinSrc refbinsrc;
    hBinSrc binsrc;
    union
    {
        hGrid grid;
        hTrig trig;
    } model;
    Boolean loaded;
    StatusType loadstatus;
    struct s_ShiftCmp *nextcmp;
} ShiftCmp, *hShiftCmp;

typedef struct
{
    char *name;
    char *version;
    char *crdsyscode;
    char *description;
    CrdRange range;
    hShiftCmp firstcmp;
    hShiftCmp lastcmp;
    hBinSrc binsrc;
    double lat0;
    double latcnv0;
    double latcnv1;
    double loncnv0;
    double loncnv1;
    double loncnv2;
} ShiftMod, *hShiftMod;



/*************************************************************************
** Function name: check_header
**//**
**    Reads the binary source header and checks that it is compatible with the
**    one of the defined valid headers, each defining a version of the linz
**    shift model format.  Also sets the binary source endian-ness to
**    match the model and retrieves the location of the shift model
**    index data in the data stream.
**
**  \param binsrc              The binary data source to read from
**  \param indexloc            Returns the location of the grid index
**                             data
**
**  \return                    Returns the grid format version number
**
**************************************************************************
*/

static int check_header( hBinSrc binsrc, INT4 *indexloc)
{
    char buf[80];
    INT4 len;
    int version;
    int big_endian=0;
    version = 0;
    len = strlen( SHIFTMODEL_FILE_HEADER_1 );
    if( utlBinSrcLoad1( binsrc, 0, len, buf ) != STS_OK ) return 0;

    if(  memcmp( buf, SHIFTMODEL_FILE_HEADER_1, len ) == 0 )
    {
        version = 1;
        big_endian = 0;
    }
    else if(  memcmp( buf, SHIFTMODEL_FILE_HEADER_2, len ) == 0 )
    {
        version = 1;
        big_endian = 1;
    }
    utlBinSrcSetBigEndian( binsrc, big_endian );

    if( utlBinSrcLoad4( binsrc, BINSRC_CONTINUE, 1, indexloc ) != STS_OK ) return
            0;
    if( ! *indexloc ) version = 0;
    return version;
}


/*************************************************************************
** Function name: delete_def_comp
**//**
**    Deletes a shift model component, releasing any resources that
**    it uses.
**
**  \param cmp                   The shift model component to delete
**
**  \return
**
**************************************************************************
*/

static void delete_def_comp( hShiftCmp cmp )
{
    if( cmp->description )
    {
        utlFree( cmp->description );
        cmp->description = NULL;
    }
    /*> If a grid or triangulation model has been loaded to calculate the
        component, then release it with utlReleaseGrid or utlReleaseTrig */

    if( cmp->loaded )
    {
        switch (cmp->type)
        {
        case defModelGrid:
            if( cmp->model.grid )
            {
                utlReleaseGrid( cmp->model.grid );
                cmp->model.grid = NULL;
            }
            break;
        case defModelTrig:
            if( cmp->model.trig )
            {
                utlReleaseTrig( cmp->model.trig );
                cmp->model.trig = NULL;
            }
            break;
        };
    };
    if( cmp->binsrc )
    {
        utlReleaseBinSrc(cmp->binsrc);
        cmp->binsrc = 0;
    }

    /*> Release memory allocated to the component */
    utlFree( cmp );
}

/*************************************************************************
** Function name: delete_def_mod
**//**
**    Deletes a shift model and releases any resources it uses.
**
**  \param mod                   The shift model to delete
**
**  \return
**
**************************************************************************
*/

static void delete_def_mod( hShiftMod mod )
{
    hShiftCmp cmp;
    hShiftCmp nextcmp;

    /*> Deallocate string resources */

    if( mod->name )
    {
        utlFree(mod->name);
        mod->name = NULL;
    }
    if( mod->version )
    {
        utlFree(mod->version);
        mod->version = NULL;
    }
    if( mod->crdsyscode )
    {
        utlFree(mod->crdsyscode);
        mod->crdsyscode = NULL;
    }
    if( mod->description )
    {
        utlFree(mod->description);
        mod->description = NULL;
    }

    /*> Delete all shift model components with delete_def_comp */
    cmp  = mod->firstcmp;
    while( cmp )
    {
        nextcmp = cmp->nextcmp;
        delete_def_comp( cmp );
        cmp = nextcmp;
    }
    mod->firstcmp = NULL;
    mod->lastcmp = NULL;

    /*> Release the model itself */

    utlFree( mod );
}


/*************************************************************************
** Function name: load_component
**//**
**    Creates and loads a shift model component
**
**    Note: this doesn't read the grid or trig model itself, just the
**    definition of it.  The actual model is loaded only when it is first
**    required.
**
**  \param binsrc              The binary source from which to load
**                             the component
**  \param pcmp                  The component object that is created.
**
**  \return                    The return status
**
**************************************************************************
*/


static StatusType load_component( hBinSrc binsrc, hShiftCmp *pcmp )
{
    hShiftCmp cmp;
    StatusType sts;

    INT2 usebefore=0;
    INT2 useafter=0;
    INT2 istrig=0;
    INT2 dimension=0;
    INT2 negative=0;

    /*> Allocate a ShiftCmp object */

    (*pcmp) = NULL;
    cmp = (hShiftCmp) utlAlloc( sizeof( ShiftCmp ) );
    if( ! cmp ) RETURN_STATUS(STS_ALLOC_FAILED);

    TRACE_SHIFT(("Loading component"));

    cmp->id = 0;
    cmp->description = NULL;
    cmp->binsrc = NULL;
    cmp->refbinsrc = binsrc;
    cmp->loaded = BLN_FALSE;
    cmp->loadstatus = STS_OK;
    cmp->type = defModelGrid;
    cmp->nextcmp = NULL;
    cmp->dimension = 0;
    cmp->negative = 0;

    /*> Load the object from the binary source file */

    sts = STS_OK;
    if( sts == STS_OK ) sts = utlBinSrcLoadString( binsrc, BINSRC_CONTINUE, &(cmp->description) );
    if( sts == STS_OK ) TRACE_SHIFT(("Component %s loaded",
                                         cmp->description ? cmp->description : "(No description)"));

    if( sts == STS_OK ) sts = utlBinSrcLoad8( binsrc, BINSRC_CONTINUE, 1, &(cmp->range.ymin) );
    if( sts == STS_OK ) sts = utlBinSrcLoad8( binsrc, BINSRC_CONTINUE, 1, &(cmp->range.ymax) );
    if( sts == STS_OK ) sts = utlBinSrcLoad8( binsrc, BINSRC_CONTINUE, 1, &(cmp->range.xmin) );
    if( sts == STS_OK ) sts = utlBinSrcLoad8( binsrc, BINSRC_CONTINUE, 1, &(cmp->range.xmax) );

    if( sts == STS_OK ) TRACE_SHIFT(("Range (%.4lf %.4lf %.4lf %.4lf)",
                                         cmp->range.xmin, cmp->range.xmax,
                                         cmp->range.ymin, cmp->range.ymax ));
    if( sts == STS_OK ) sts = utlBinSrcLoad2( binsrc, BINSRC_CONTINUE, 1, &istrig );
    if( istrig ) cmp->type = defModelTrig;
    if( sts == STS_OK ) TRACE_SHIFT(("Trig? %d: ",(int) cmp->type ));

    if( sts == STS_OK ) sts = utlBinSrcLoad2( binsrc, BINSRC_CONTINUE, 1, &dimension );
    cmp->dimension = dimension;
    if( sts == STS_OK ) TRACE_SHIFT(("Dimension %d: ",(int) cmp->dimension ));

    if( sts == STS_OK ) sts = utlBinSrcLoad2( binsrc, BINSRC_CONTINUE, 1, &negative );
    cmp->negative = negative;
    if( sts == STS_OK ) TRACE_SHIFT(("Negative? %d: ",(int) cmp->negative ));

    if( sts == STS_OK ) sts = utlBinSrcLoad4( binsrc, BINSRC_CONTINUE, 1, &(cmp->offset) );
    if( sts == STS_OK ) TRACE_SHIFT(("Location  %d: ",(int) cmp->offset ));

    /*> If the load was not successful, delete the partially loaded object with
        delete_def_comp */

    if( sts != STS_OK )
    {
        delete_def_comp( cmp );
        RETURN_STATUS( sts );
    }

    (*pcmp) = cmp;

    return sts;
}


/*************************************************************************
** Function name: load_component_model
**//**
**    Loads the actual model from the component
**
**  \param cmp                   The definition of the shift model
**                             component.
**
**  \return                    The return status
**
**************************************************************************
*/

static StatusType load_component_model( hShiftCmp cmp )
{
    StatusType sts;

    /*> If the model has already been loaded then return success */

    if( cmp->loaded ) return STS_OK;

    /*> If the model has already tried to load and failed, then return failure */

    if( cmp->loadstatus != STS_OK ) RETURN_STATUS( cmp->loadstatus );

    TRACE_SHIFT(("Loading component model %s at offset %d",cmp->description,cmp->offset));

    /*> Create a new binary source object from the original source with the
        model offset applied using utlCreateEmbeddedBinSrc */

    sts = utlCreateEmbeddedBinSrc( cmp->refbinsrc, cmp->offset, &(cmp->binsrc) );
    if( sts == STS_OK )
    {

        /*> Depending upon the model type, create the model with utlCreateGrid
            or utlCreateTrig */

        switch (cmp->type)
        {
        case defModelGrid:
            sts = utlCreateGrid( cmp->binsrc, &(cmp->model.grid) );
            break;
        case defModelTrig:
            sts = utlCreateTrig( cmp->binsrc, &(cmp->model.trig) );
            break;
        default:
            sts = STS_INVALID_DATA;
            break;
        };
    };

    /*> Record the load status if not successful */

    if( sts != STS_OK )
    {
        cmp->loadstatus = sts;
        RETURN_STATUS( sts );
    };

    cmp->loaded = BLN_TRUE;

    TRACE_SHIFT(("Component model %s loaded",cmp->description));

    return sts;
}



/*************************************************************************
** Function name: calc_component_model
**//**
**    Routine to calculate the model value for the component at a specified
**    location
**
**  \param cmp                   The component to evaluate
**  \param x                   The x coordinate
**  \param y                   The y coordinate
**  \param result              Array to store the results
**
**  \return                    The return status
**
**************************************************************************
*/

static StatusType calc_component_model( hShiftCmp cmp, double x, double y, double *result )
{
    StatusType sts = STS_OK;

    /*> Load the model if it is not already loaded */

    if( ! cmp->loaded )
    {
        sts = load_component_model( cmp );
    }


    /*> Evaluate the model with utlCalcGridLinear or utlCalcTrig, depending
        upon the model type */

    if( sts == STS_OK )
    {
        switch (cmp->type)
        {
        case defModelGrid:
            sts = utlCalcGridLinear( cmp->model.grid, x, y, result );
            break;
        case defModelTrig:
            sts = utlCalcTrig( cmp->model.trig, x, y, result );
            break;
        default:
            sts = STS_INVALID_DATA;
            break;
        };
    };

    if( sts != STS_OK ) RETURN_STATUS( sts );
    return sts;
}

/*************************************************************************
** Function name: check_range
**//**
**    Function to check whether a specified point is inside or outside
**    a coordinate range.
**
**  \param range               The coordinate range
**  \param x                   The x coordinate
**  \param y                   The y coordinate
**
**  \return                    The return status
**
**************************************************************************
*/

static StatusType check_range( hCrdRange range, double x, double y )
{
    if( x < range->xmin || x > range->xmax ||
            y < range->ymin || y > range->ymax )
    {
        TRACE_SHIFT2(("shift_model range error: %.2lf %.2lf (%.2lf %.2lf %.2lf %.2lf)",x,y,range->xmin,range->xmax,range->ymin,range->ymax));
        RETURN_STATUS( STS_CRD_RANGE_ERR );
    }
    return STS_OK;
}


/*************************************************************************
** Function name: add_component
**//**
**    Adds the shift from a component to that for the total model
**
**  \param cmp                   The component
**  \param x                   The x coordinate
**  \param y                   The y coordinate
**  \param value               The array to which the displacement is
**                             to be added.
**
**  \return                    The return status
**
**************************************************************************
*/

static StatusType add_component( hShiftCmp cmp, double x,
                                 double y, double* value )
{
    StatusType sts;
    double cmpval[3];

    TRACE_SHIFT2(("add_component: Adding offset from component %d",cmp->id));

    cmpval[0] = 0.0;
    cmpval[1] = 0.0;
    cmpval[2] = 0.0;

    /*> Check that the coordinates are within the range of the component
        with calc_range.

        If not then nothing to do
        */

    sts = check_range( &(cmp->range), x, y );

    if( sts != STS_OK ) return STS_OK;

    sts = calc_component_model( cmp, x, y, cmpval );

    if( sts == STS_CRD_RANGE_ERR ) return STS_OK;

    /*> Add the displacement components according to the dimension of the
        component (either height only, horizontal only, or 3d) */

    TRACE_SHIFT2(("add_component: id = %d, dimension = %d, value = %.4lf %.4lf %.4lf",
                  cmp->id, (int)(cmp->dimension), cmpval[0], cmpval[1], cmpval[2] ));

    if( cmp->negative )
    {
        cmpval[0] = -cmpval[0];
        cmpval[1] = -cmpval[1];
        cmpval[2] = -cmpval[2];
    }

    if( cmp->dimension == 1 )
    {
        value[2] += cmpval[0];
    }
    else
    {
        value[0] += cmpval[0];
        value[1] += cmpval[1];
        if( cmp->dimension == 3 ) value[2] += cmpval[2];
    }

    return sts;
}


/*************************************************************************
** Function name: create_shift_model
**//**
**   Function to load the data defining a LINZ shift model
**
**  \param pdef                  The grid object to load
**  \param binsrc              The binary data source to load the
**                             model from
**
**  \return                    The return status
**
**************************************************************************
*/

static StatusType create_shift_model( hShiftMod* pdef, hBinSrc binsrc)
{
    hShiftMod def;
    int version;
    INT4 indexloc;
    StatusType sts;
    int idcomp;
    INT2 ncomp=0;

    double a, a2, b2, f, slat, clat, b, bsac, lat0, ltrange, lnc[3], ltc[3];
    int i;

    (*pdef) = NULL;

    TRACE_SHIFT(("create_shift_model"));
    version = check_header(binsrc, &indexloc);
    TRACE_SHIFT(("create_shift_model: version %d  indexloc %ld",version,indexloc));
    if( ! version )
    {
        RETURN_STATUS(STS_INVALID_DATA);
    }
    def = (hShiftMod) utlAlloc( sizeof( ShiftMod ) );
    if( ! def ) RETURN_STATUS(STS_ALLOC_FAILED);

    def->name = NULL;
    def->version = NULL;
    def->crdsyscode = NULL;
    def->description = NULL;
    def->firstcmp = NULL;
    def->lastcmp = NULL;
    def->binsrc = binsrc;

    TRACE_SHIFT(("Loading shift model"));

    sts = STS_OK;
    if( sts == STS_OK ) sts = utlBinSrcLoadString( binsrc, indexloc, &(def->name) );
    if( sts == STS_OK) TRACE_SHIFT(("Shift model header loaded: %s",def->name));
    if( sts == STS_OK ) sts = utlBinSrcLoadString( binsrc, BINSRC_CONTINUE, &(def->version) );
    if( sts == STS_OK ) TRACE_SHIFT(("Version: %s",def->version));
    if( sts == STS_OK ) sts = utlBinSrcLoadString( binsrc, BINSRC_CONTINUE, &(def->crdsyscode) );
    if( sts == STS_OK ) TRACE_SHIFT(("CrdSys: %s",def->crdsyscode));
    if( sts == STS_OK ) sts = utlBinSrcLoadString( binsrc, BINSRC_CONTINUE, &(def->description) );
    if( sts == STS_OK ) TRACE_SHIFT(("Description: %s",def->description));

    if( sts == STS_OK ) sts = utlBinSrcLoad8( binsrc, BINSRC_CONTINUE, 1, &(def->range.ymin) );
    if( sts == STS_OK ) sts = utlBinSrcLoad8( binsrc, BINSRC_CONTINUE, 1, &(def->range.ymax) );
    if( sts == STS_OK ) sts = utlBinSrcLoad8( binsrc, BINSRC_CONTINUE, 1, &(def->range.xmin) );
    if( sts == STS_OK ) sts = utlBinSrcLoad8( binsrc, BINSRC_CONTINUE, 1, &(def->range.xmax) );
    if( sts == STS_OK ) TRACE_SHIFT(("Range: (%.4lf %.4lf %.4lf %.4lf)",def->range.xmin,def->range.xmax,def->range.ymin,def->range.ymax));

    if( sts == STS_OK ) sts = utlBinSrcLoad2( binsrc, BINSRC_CONTINUE, 1, &ncomp );

    /*> Load the each of the shift model components with load_component */

    idcomp = 0;
    while( sts == STS_OK && ncomp-- )
    {
        hShiftCmp cmp;
        sts = load_component( binsrc, &cmp );
        if( sts == STS_OK )
        {
            idcomp++;
            cmp->id = idcomp;
            if( ! def->firstcmp ) def->firstcmp = cmp;
            if( def->lastcmp ) def->lastcmp->nextcmp = cmp;
            def->lastcmp = cmp;
        }
    }

    if( sts != STS_OK )
    {
        delete_def_mod( def );
        RETURN_STATUS( sts );
    }

    /*> Calculate midpoint conversions from metres to lat/lon degrees */

    a = 6378137.0;
    f = 1.0/298.257222101;
    b = a*(1.0-f);
    a2 = a*a;
    b2 = b*b;

    lat0 = (def->range.ymin+def->range.ymax)/2.0;
    ltrange = def->range.ymax - def->range.ymin;

    for( i = 0; i < 3; i++ )
    {
        double lat = DTOR*(lat0+(i-1)*ltrange/2.0);
        slat = sin(lat);
        clat = cos(lat);
        bsac = sqrt( b2*slat*slat + a2*clat*clat);
        lnc[i] = bsac/(a2*clat*DTOR);
        ltc[i] = (bsac*bsac*bsac)/(a2*b2*DTOR);
    }

    def->lat0 = lat0;

    def->latcnv0 = (ltc[0]+ltc[2]+2*ltc[1])/4;
    def->latcnv1 = (ltc[2]-ltc[0])/ltrange;

    def->loncnv0 = lnc[1];
    def->loncnv1 = (lnc[2]-lnc[0])/ltrange;
    def->loncnv2 = 2*(lnc[0]+lnc[2]-2*lnc[1])/(ltrange*ltrange);

    TRACE_SHIFT(("Conversion reference latitude: %.5lf",def->lat0));
    TRACE_SHIFT(("Lat conv: %.12lf + %.12lf*lat",def->latcnv0,def->latcnv1));
    TRACE_SHIFT(("Lon conv: %.12lf + %.12lf*lat + %.12lf*lat*lat",
                 def->loncnv0, def->loncnv1, def->loncnv2));

    TRACE_SHIFT(("Shift model loaded: %s",def->name));

    *pdef = def;
    return STS_OK;
}


/*************************************************************************
** Function name: calc_def_mod
**//**
**    Calculate the displacement for a shift model at a specific time
**    and location.
**
**  \param def                   The shift model
**  \param x                   The x coordinate
**  \param y                   The y coordinate
**  \param value               Array in which to return the
**                             displacment.
**
**  \return                    The return status
**
**************************************************************************
*/

static StatusType calc_def_mod( hShiftMod def,
                                double x, double y, double* value )
{
    hShiftCmp cmp;
    double calcval[3];
    StatusType sts;

    value[0] = value[1] = value[2] = 0;
    calcval[0] = calcval[1] = calcval[2] = 0;

    /*> Check that the coordinates are within range of the model.
        If not there's nothing to do.. */

    sts = check_range( &(def->range), x, y );
    if( sts != STS_OK ) RETURN_STATUS( STS_OK );

    /*> Sum the effects of each shift model component using add_component */

    TRACE_SHIFT2(("calc_def_mod: Calculating model shift position %.8f %.8f",
                  x,y));

    for( cmp = def->firstcmp; cmp; cmp = cmp->nextcmp )
    {
        sts = add_component( cmp, x, y, calcval );
        if( sts != STS_OK ) RETURN_STATUS( sts );
    }

    TRACE_SHIFT2(("calc_def_mod: Shift calculated as %.4f %.4f %.4f",
                  calcval[0],calcval[1],calcval[2]));

    /*> Store the resulting displacement into the result array */

    value[0] = calcval[0];
    value[1] = calcval[1];
    value[2] = calcval[2];

    return STS_OK;
}


/*************************************************************************
** Function name: utlCreateShiftModel
**//**
**    Create and load a ShiftModel object from a binary data source.
**    This is used to evaluate shift model at a given time and place.
**    The routine is just a wrapper around create_shift_model.
**
**  \param blob                The data source
**  \param pdef                 * The created model
**
**  \return                    The return status
**
**************************************************************************
*/

StatusType utlCreateShiftModel( hBinSrc blob, hPointShiftModel *pdef )
{
    StatusType sts;
    hShiftMod def;

    (*pdef) = NULL;
    sts = create_shift_model( &def, blob );
    if( sts == STS_OK ) (*pdef) = def;

    RETURN_STATUS( sts );
}


/*************************************************************************
** Function name: utlReleaseShiftModel
**//**
**    Release the resources allocated to a ShiftModel object.
**
**  \param def                    The model to release
**
**  \return                    The return status
**
**************************************************************************
*/

StatusType utlReleaseShiftModel( hPointShiftModel def )
{
    if( def )
    {
        delete_def_mod( (hShiftMod) def );
    }
    return STS_OK;
}


/*************************************************************************
** Function name: utlShiftModelCoordSysDef
**//**
**    Get the coordinate system code for the model.
**
**  \param def                    The shift model
**  \param crdsys              Receives a pointer to a string defining
**                             the coordinate system code.
**
**  \return                    The return status
**
**************************************************************************
*/

StatusType utlShiftModelCoordSysDef( hPointShiftModel def, char ** crdsys )
{
    (*crdsys) = NULL;
    if( ! def ) RETURN_STATUS( STS_INVALID_DATA );
    (*crdsys) = ((hShiftMod) def)->crdsyscode;
    return STS_OK;
}


/*************************************************************************
** Function name: utlShiftModelTitle
**//**
**    Returns text description information from a ShiftModel shift model
**
**  \param def                    The model
**  \param nTitle              Identifies what is to be returned.
**                               1 is the name
**                               2 is the description
**                               3 is the version number
**  \param title               Returns a pointer to the text
**
**  \return                    The return status
**
**************************************************************************
*/

StatusType utlShiftModelTitle( hPointShiftModel def, int nTitle, char ** title )
{
    (*title) = NULL;
    if( ! def ) RETURN_STATUS( STS_INVALID_DATA );
    if( nTitle < 1 || nTitle > 3 ) RETURN_STATUS( STS_INVALID_DATA );
    switch( nTitle )
    {
    case 1:
        (*title) = ((hShiftMod) def)->name;
        break;
    case 2:
        (*title) = ((hShiftMod) def)->description;
        break;
    case 3:
        (*title) = ((hShiftMod) def)->version;
        break;
    }
    return STS_OK;
}


/*************************************************************************
** Function name: utlShiftModelRange
**//**
**    Returns the extents affected by the model
**
**  \param def                    The model
**  \param xmin                The minimum x value affected by the model
**  \param xmax                The maximum x value affected by the model
**  \param ymin                The minimum y value affected by the model
**  \param ymax                The maximum y value affected by the model
**
**  \return                    The return status
**
**************************************************************************
*/

StatusType utlShiftModelRange( hPointShiftModel def,
                               double *xmin, double *xmax, double *ymin, double *ymax )
{
    hShiftMod mod;
    if( ! def ) RETURN_STATUS( STS_INVALID_DATA );
    mod = (hShiftMod) def;
    *xmin = mod->range.xmin;
    *xmax = mod->range.xmax;
    *ymin = mod->range.ymin;
    *ymax = mod->range.ymax;
    return STS_OK;
}


/*************************************************************************
** Function name: utlCalcShiftModel
**//**
**    Routine to calculate the shift defined by the model
**    at a coordinate.  This is a wrapper around ::calc_def_mod.
**
**  \param def                    The shift model
**  \param x                   The x coordinate
**  \param y                   The y coordinate
**  \param value               Array to receive the calculated
**                             displacement.
**
**  \return                    The return status
**
**************************************************************************
*/

StatusType utlCalcShiftModel( hPointShiftModel def, double x, double y,
                              double * value)
{

    if( ! def ) RETURN_STATUS( STS_INVALID_DATA );
    return calc_def_mod( (hShiftMod) def, x, y, value );
}


/*************************************************************************
** Function name: utlCalcShiftModel
**//**
**    Routine to apply the shift to a coordinate.  This assumes that the
**    coordinate is geographic (lon, lat, height) and that the
**    shift model defines changes in metres.  The conversion
**    to lon/lat shifts uses an approximation to lat/lon conversion.
**    For a model covering the extents of NZ this is good to about
**    0.6% of actual value in east direction, and 0.1% in north
**    direction (see code in create_shift_model).
**
**  \param def                 The shift model
**  \param crd                 Array of coordinates that will be
**                             updated.
**
**  \return                    The return status
**
**************************************************************************
*/

StatusType utlApplyShiftModel( hPointShiftModel def, double * crd)
{
    StatusType sts;
    double shift[3];
    hShiftMod model = (hShiftMod) def;


    if( ! def ) RETURN_STATUS( STS_INVALID_DATA );

    sts = calc_def_mod( model, crd[0], crd[1], shift );
    if( sts == STS_OK )
    {
        double lat = crd[1] - model->lat0;
        double lonfactor = model->loncnv0 + (model->loncnv1 + model->loncnv2*lat)*lat;
        double latfactor = model->latcnv0 + model->latcnv1*lat;
        crd[0] += shift[0]*lonfactor;
        crd[1] += shift[1]*latfactor;
        crd[2] += shift[2];
    }
    else if( sts != STS_CRD_RANGE_ERR )
    {
        RETURN_STATUS(sts);
    }
    return STS_OK;
}
