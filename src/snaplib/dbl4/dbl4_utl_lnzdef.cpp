/*************************************************************************
**
**  Filename:    %M%
**
**  Version:     %I%
**
**  What string: %W%
**
** $Id: dbl4_utl_lnzdef.c,v 1.3 2005/04/04 23:57:58 ccrook Exp $
**//**
** \file
**      Functions for evaluating the complete LINZ deformation model
**      accessed via a binary source object.
**
**      The deformation model is defined by one or more sequences each
**      comprising one or more components.  The sequences define deformation
**      for a specific period and geographical extent.  Each sequence can
**      either define deformation (displacement) or velocities, and can define
**      it in one, two, or three dimensions.  The components are either grid
**      or triangulation definitions of displacement, which are embedded into
**      the binary file.  For velocity models each component is added
**      independently.  For displacement models the deformation is interpolated
**      between the two models that apply (one before, one after), or
**      extrapolated from the first or last two components.  Alternatively
**      the displacement may be defined as fixed or zero before or after
**      each component.
**
**      The main components used in this code are:
**         DefMod      The deformation model
**         DefSeq      A deformation sequence in the model
**         DefCmp      A component in the sequence
**
**      The binary format is portable between SUN and Intel DOS/Windows
**      environments which differ only in endianness
**
*************************************************************************
*/

#include "dbl4_common.h"

#include <stdlib.h>
#include <string.h>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <boost/numeric/conversion/cast.hpp>

#include "dbl4_utl_lnzdef.h"
#include "dbl4_utl_trig.h"
#include "dbl4_utl_grid.h"
#include "dbl4_utl_date.h"

#include "dbl4_utl_error.h"
#include "util/stringlimited.hpp"

/* Note: code below assumes headers are all the same length */

#define LNZDEF_FILE_HEADER_1L "LINZ deformation model v1.0L\r\n\x1A"
#define LNZDEF_FILE_HEADER_1B "LINZ deformation model v1.0B\r\n\x1A"
#define LNZDEF_FILE_HEADER_2L "LINZ deformation model v2.0L\r\n\x1A"
#define LNZDEF_FILE_HEADER_2B "LINZ deformation model v2.0B\r\n\x1A"
#define LNZDEF_FILE_HEADER_3L "LINZ deformation model v3.0L\r\n\x1A"
#define LNZDEF_FILE_HEADER_3B "LINZ deformation model v3.0B\r\n\x1A"

typedef enum { defModelGrid, defModelTrig } DefModelType;
typedef enum { defEvalZero, defEvalFixed, defEvalInterp } DefEvalMode;
typedef enum { defValueDeformation, defValueVelocity } DefValueType;

#define VERSIONLEN 8
using Version = StringLimited<VERSIONLEN>;
constexpr std::string_view defaultStartVer = "00000000";
constexpr std::string_view defaultEndVer = "99999999";

struct CrdRange
{
    double xmin;
    double ymin;
    double xmax;
    double ymax;
};

struct TimeModelPoint
{
    double year;
    double factor;
};

struct DefCmp
{
    int id = 0;
    std::string description;
    DateTimeType refdate = {};
    CrdRange range = {};
    int dimension = 0;
    DefEvalMode beforemode = defEvalZero; /* LINZDEF version 1 parameter */
    DefEvalMode aftermode = defEvalZero; /* LINZDEF version 1 parameter */
    double factor0 = 0.0;
    std::vector<TimeModelPoint> timeModel;
    DefModelType type = defModelGrid;
    INT4 offset = 0;
    hBinSrc const refbinsrc;
    hBinSrc binsrc = nullptr;
    union
    {
        hGrid grid;
        hTrig trig;
    } model = {};
    bool loaded = false;
    StatusType loadstatus = STS_OK;
    DefCmp *nextcmp = nullptr;

    /// Creates a component that will load its model from the binary source
    explicit DefCmp( const hBinSrc source ) : refbinsrc( source ) {}  ///< The source of the deformation model

    /// The number of points in the time model
    INT2 nTimeModel() const { return boost::numeric_cast<INT2>( timeModel.size() ); }
};
typedef DefCmp *hDefCmp;

struct DefSeq
{
    int id = 0;
    std::string name;
    std::string description;
    DateTimeType startdate = {};
    DateTimeType enddate = {};
    CrdRange range = {};
    int dimension = 0;
    DefValueType valtype = defValueDeformation; /* LINZDEF version 1 parameter */
    bool zerobeyond = false;
    bool nested = false;
    hDefCmp firstcmp = nullptr;
    hDefCmp lastcmp = nullptr;
    Version startver{ defaultStartVer };
    Version endver{ defaultEndVer };
    bool enabled = false;
    DefSeq *nextseq = nullptr;
};
typedef DefSeq *hDefSeq;

struct DefVer
{
    Version version{ defaultStartVer };
    DateTimeType versiondate = {};
    std::optional<std::string> description;  ///< Absent if not loaded, which differs from an empty description
    DefVer *nextver = nullptr;
};
typedef DefVer *hDefVer;

struct DefMod
{
    std::string name;
    std::string crdsyscode;
    DateTimeType startdate = {};
    DateTimeType enddate = {};
    CrdRange range = {};
    bool isgeographical = false;
    int nsequences = 0;
    hDefSeq firstseq = nullptr;
    hDefSeq lastseq = nullptr;
    hDefVer firstver = nullptr;
    hDefVer currver = nullptr;
    hBinSrc const binsrc;

    /// Creates a model with no sequences or versions
    explicit DefMod( const hBinSrc source ) : binsrc( source ) {}  ///< The source the model is read from
};
typedef DefMod *hDefMod;



/*************************************************************************
** Function name: check_header
**//**
**    Reads the binary source header and checks that it is compatible with the
**    one of the defined valid headers, each defining a version of the linz
**    deformation model format.  Also sets the binary source endian-ness to
**    match the model and retrieves the location of the deformation model
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
    Endian endian = Endian::Little;
    version = 0;
    len = strlen( LNZDEF_FILE_HEADER_1L );
    if( utlBinSrcLoad1( binsrc, 0, len, buf ) != STS_OK ) return 0;

    if(  memcmp( buf, LNZDEF_FILE_HEADER_1L, len ) == 0 )
    {
        version = 1;
        endian = Endian::Little;
    }
    else if(  memcmp( buf, LNZDEF_FILE_HEADER_1B, len ) == 0 )
    {
        version = 1;
        endian = Endian::Big;
    }
    else if(  memcmp( buf, LNZDEF_FILE_HEADER_2B, len ) == 0 )
    {
        version = 2;
        endian = Endian::Big;
    }
    else if(  memcmp( buf, LNZDEF_FILE_HEADER_2L, len ) == 0 )
    {
        version = 2;
        endian = Endian::Little;
    }
    else if(  memcmp( buf, LNZDEF_FILE_HEADER_3B, len ) == 0 )
    {
        version = 3;
        endian = Endian::Big;
    }
    else if(  memcmp( buf, LNZDEF_FILE_HEADER_3L, len ) == 0 )
    {
        version = 3;
        endian = Endian::Little;
    }
    utlBinSrcSetEndian( binsrc, endian );

    if( utlBinSrcLoad4( binsrc, BINSRC_CONTINUE, 1, indexloc ) != STS_OK ) return
            0;
    if( ! *indexloc ) version = 0;
    return version;
}


/*************************************************************************
** Function name: delete_def_comp
**//**
**    Deletes a deformation component, releasing any resources that
**    it uses.
**
**  \param cmp                 The deformation component to delete
**
**  \return
**
**************************************************************************
*/

static void delete_def_comp( hDefCmp cmp )
{
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
    delete cmp;
}


/*************************************************************************
** Function name: delete_def_seq
**//**
**    Deletes a deformation sequence and releases any resources allocated
**    to it.  In particular this also deletes any components that it
**    includes
**
**  \param seq                 The deformation sequence to delete.
**
**  \return
**
**************************************************************************
*/

static void delete_def_seq( hDefSeq seq )
{
    hDefCmp cmp;
    hDefCmp nextcmp;

    /*> Delete all deformation components with delete_def_comp */
    cmp  = seq->firstcmp;
    while( cmp )
    {
        nextcmp = cmp->nextcmp;
        delete_def_comp( cmp );
        cmp = nextcmp;
    }
    seq->firstcmp = NULL;
    seq->lastcmp = NULL;

    /*> Release memory allocated to the sequence object */
    delete seq;
}

/*************************************************************************
** Function name: delete_def_mod
**//**
**    Deletes a deformation model and releases any resources it uses.
**    In particular it deletes any deformation sequences defined for the
**    model.
**
**  \param mod                 The deformation model to delete
**
**  \return
**
**************************************************************************
*/

static void delete_def_mod( hDefMod mod )
{
    hDefSeq seq, nextseq;

    /*> Delete each deformation sequence with delete_def_seq */

    seq = mod->firstseq;
    while( seq )
    {
        nextseq = seq->nextseq;
        delete_def_seq( seq );
        seq = nextseq;
    }
    mod->firstseq = NULL;
    mod->lastseq = NULL;

    while( mod->firstver )
    {
        hDefVer ver=mod->firstver;
        mod->firstver=ver->nextver;
        delete ver;
    }

    /*> Release the model itself */

    delete mod;
}


/*************************************************************************
** Function name: load_date
**//**
**    Function to load a date from the blob.  The date is stored as
**    2 byte year, month (Jan=1), day, hour, minute, second.
**
**  \param binsrc              The binary source object
**  \param offset              The location from which the date
**                             is to be read
**  \param date                The date to be returned
**
**  \return                    The return status
**
**************************************************************************
*/

static StatusType load_date( hBinSrc binsrc, long offset, DateTimeType *date )
{
    INT2 datecomp[6];
    StatusType sts;
    sts = utlBinSrcLoad2( binsrc, offset, 6, (void *) (datecomp) );

    if( sts != STS_OK ) RETURN_STATUS( sts );

    utlSetDateTime( date, datecomp[0], datecomp[1], datecomp[2],
                    datecomp[3], datecomp[4], datecomp[5] );
    return STS_OK;
}


/// Loads the description of a version, which stays absent if the load fails.
/// Returns the return status.
static StatusType load_version_description(
    const hBinSrc binsrc,  ///< The binary source object
    DefVer &ver )          ///< The version whose description is set
{
    std::string text;
    const StatusType sts = utlBinSrcLoadString( binsrc, BINSRC_CONTINUE, text );
    if( sts != STS_OK ) {
        RETURN_STATUS( sts );
    }
    ver.description = std::move( text );
    return STS_OK;
}


/// Loads a version identifier (VERSION, VERSION_NUMBER, VERSION_START, or
/// VERSION_END) into a version of at most VERSIONLEN characters.
/// VERSION_NUMBER (version 1/2) and VERSION (version 3) have always accepted
/// strings longer than VERSIONLEN by silently truncating them, so
/// rejectOverlength must be false for those two to preserve that behaviour.
/// Only VERSION_START/VERSION_END have always rejected them.
/// Returns the return status.
static StatusType load_version_field(
    const hBinSrc binsrc,          ///< The binary source object
    Version &dest,                 ///< The version to set
    const bool rejectOverlength )  ///< If true, reject strings longer than VERSIONLEN instead of truncating them
{
    std::string data;
    const StatusType sts = utlBinSrcLoadString( binsrc, BINSRC_CONTINUE, data );
    if( sts != STS_OK ) {
        RETURN_STATUS( sts );
    }
    if( rejectOverlength && data.size() > VERSIONLEN ) {
        RETURN_STATUS( STS_INVALID_DATA );
    }
    dest.assign( data );
    return STS_OK;
}


/// Orders two versions as strcmp would: negative if the first is earlier,
/// zero if they are equal and positive if it is later.
static int compare_versions(
    const Version &version1,   ///< The first version
    const Version &version2 )  ///< The second version
{
    return std::string_view( version1 ).compare( std::string_view( version2 ) );
}


/*************************************************************************
** Function name: load_component
**//**
**    Creates and loads a deformation component
**
**    Note: this doesn't read the grid or trig model itself, just the
**    definition of it.  The actual model is loaded only when it is first
**    required.
**
**  \param version             The model format version
**  \param seq                 The sequence for which the component is being loaded
**  \param binsrc              The binary source from which to load
**                             the component
**  \param pcmp                The component object that is created.
**
**  \return                    The return status
**
**************************************************************************
*/


static StatusType load_component( int version, hDefSeq seq, hBinSrc binsrc, hDefCmp *pcmp )
{
    hDefCmp cmp;
    StatusType sts;

    INT2 usebefore=0;
    INT2 useafter=0;
    INT2 istrig=0;
    INT2 tmtype=0;

    /*> Allocate a DefCmp object */

    (*pcmp) = NULL;
    cmp = new DefCmp( binsrc );

    TRACE_LNZDEF(("Loading component"));

    /*> Load the object from the binary source file */

    sts = STS_OK;
    if( sts == STS_OK ) sts = utlBinSrcLoadString( binsrc, BINSRC_CONTINUE, cmp->description );
    if( sts == STS_OK ) sts = load_date( binsrc, BINSRC_CONTINUE, &(cmp->refdate) );

    if( sts == STS_OK ) sts = utlBinSrcLoad8( binsrc, BINSRC_CONTINUE, 1, &(cmp->range.ymin) );
    if( sts == STS_OK ) sts = utlBinSrcLoad8( binsrc, BINSRC_CONTINUE, 1, &(cmp->range.ymax) );
    if( sts == STS_OK ) sts = utlBinSrcLoad8( binsrc, BINSRC_CONTINUE, 1, &(cmp->range.xmin) );
    if( sts == STS_OK ) sts = utlBinSrcLoad8( binsrc, BINSRC_CONTINUE, 1, &(cmp->range.xmax) );


    if( version == 1 )
    {
        int nTimeModel=1;
        int iref=0;
        int i;
        double refyear=utlDateAsYear(&(cmp->refdate));
        int isvel = seq->valtype == defValueVelocity;

        if( sts == STS_OK && version == 1 ) sts = utlBinSrcLoad2( binsrc, BINSRC_CONTINUE, 1, &usebefore );
        if( usebefore == 1 ) cmp->beforemode = defEvalFixed;
        if( usebefore == 2 ) { cmp->beforemode = defEvalInterp; iref++; nTimeModel++; }

        if( sts == STS_OK && version == 1 ) sts = utlBinSrcLoad2( binsrc, BINSRC_CONTINUE, 1, &useafter );
        if( useafter == 1 ) cmp->aftermode = defEvalFixed;
        if( useafter == 2 ) { cmp->aftermode = defEvalInterp; nTimeModel++; }

        if( isvel )
        {
            cmp->timeModel.resize(3);
            cmp->timeModel[0].year=utlDateAsYear(&(seq->startdate));
            cmp->timeModel[0].factor=usebefore ? cmp->timeModel[0].year-refyear : 0.0;
            cmp->timeModel[1].year=refyear;
            cmp->timeModel[1].factor=0.0;
            cmp->timeModel[2].year=utlDateAsYear(&(seq->enddate));
            cmp->timeModel[2].factor=useafter ? cmp->timeModel[2].year-refyear : 0.0;
        }
        else 
        {
            cmp->factor0 = cmp->beforemode == defEvalFixed ? 1.0 : 0.0;
            cmp->timeModel.resize( boost::numeric_cast<size_t>( nTimeModel ) );
            for( i = 0; i < nTimeModel; i++ )
            {
                TimeModelPoint *tmi = &(cmp->timeModel[i]);
                tmi->year=refyear;
                if( i < iref )
                {
                    tmi->factor = cmp->beforemode == defEvalFixed ? 1.0 : 0.0;
                }
                else if ( i == iref )
                {
                    tmi->factor = cmp->aftermode != defEvalZero ? 1.0 : 0.0;
                }
                else
                {
                    tmi->factor = 0.0;
                }
            }
        }
    }
    else
    {
        if( sts == STS_OK ) sts = utlBinSrcLoad2( binsrc, BINSRC_CONTINUE, 1, &tmtype );
        /* Can only handle type 1 (piecewise linear) time models */
        if( tmtype != 1 ) SET_STATUS(sts, STS_INVALID_DATA);
        INT2 nTimeModel = 0;
        if( sts == STS_OK ) sts = utlBinSrcLoad2( binsrc, BINSRC_CONTINUE, 1, &nTimeModel );
        if( sts == STS_OK ) sts = utlBinSrcLoad8( binsrc, BINSRC_CONTINUE, 1, &(cmp->factor0) );
        if( sts == STS_OK && nTimeModel > 0 )
        {
            INT2 i;
            cmp->timeModel.resize( boost::numeric_cast<size_t>( nTimeModel ) );
            for( i = 0; i < nTimeModel; i++ )
            {
                DateTimeType eventDate;
                TimeModelPoint *tmi = &(cmp->timeModel[i]);
                if( sts == STS_OK ) sts = load_date( binsrc, BINSRC_CONTINUE, &eventDate );
                if( sts == STS_OK ) tmi->year = utlDateAsYear( &eventDate );
                if( sts == STS_OK ) sts = utlBinSrcLoad8( binsrc, BINSRC_CONTINUE, 1, &(tmi->factor) );
            }
        }
    }

    if( sts == STS_OK ) sts = utlBinSrcLoad2( binsrc, BINSRC_CONTINUE, 1, &istrig );
    if( istrig ) 
    {
        /* Current implementation does not support triangulated data in 
         * nested model, as does not properly check if point lies within a 
         * triangulation in calc_seq_def */
        if( seq->nested ) SET_STATUS( sts, STS_INVALID_DATA );
        cmp->type = defModelTrig;
    }

    if( sts == STS_OK ) sts = utlBinSrcLoad4( binsrc, BINSRC_CONTINUE, 1, &(cmp->offset) );

    /*> If the load was not successful, delete the partially loaded object with
        delete_def_comp */

    if( sts != STS_OK )
    {
        delete_def_comp( cmp );
        RETURN_STATUS( sts );
    }

    TRACE_LNZDEF(("Component %s loaded",cmp->description.c_str()));
    TRACE_LNZDEF(("Component built with  %d time steps",cmp->nTimeModel()));

    (*pcmp) = cmp;

    return sts;
}


/*************************************************************************
** Function name: load_component_model
**//**
**    Loads the actual deformation/velocity  model from the component
**
**  \param cmp                 The definition of the deformation
**                             component.
**
**  \return                    The return status
**
**************************************************************************
*/

static StatusType load_component_model( hDefCmp cmp )
{
    StatusType sts;

    /*> If the model has already been loaded then return success */

    if( cmp->loaded ) return STS_OK;

    /*> If the model has already tried to load and failed, then return failure */

    if( cmp->loadstatus != STS_OK ) RETURN_STATUS( cmp->loadstatus );

    TRACE_LNZDEF(("Loading component model %s at offset %d",cmp->description.c_str(),cmp->offset));

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

    cmp->loaded = true;

    TRACE_LNZDEF(("Component model %s loaded",cmp->description.c_str()));

    return sts;
}



/*************************************************************************
** Function name: calc_component_model
**//**
**    Routine to calculate the model value for the component at a specified
**    location
**
**  \param cmp                 The component to evaluate
**  \param x                   The x coordinate
**  \param y                   The y coordinate
**  \param result              Array to store the results
**
**  \return                    The return status
**
**************************************************************************
*/

static StatusType calc_component_model( hDefCmp cmp, double x, double y, double *result )
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
** Function name: load_sequence
**//**
**    Creates and loads the deformation sequence object from a binary stream.
**
**  \param version             The model format version
**  \param binsrc              The binary source object
**  \param pseq                Returns the deformation sequence
**                             object that is created.
**
**  \return                    The return status
**
**************************************************************************
*/

static StatusType load_sequence( int version, hBinSrc binsrc, hDefSeq *pseq )
{
    hDefSeq seq;
    StatusType sts;
    INT2 isvel=0;
    INT2 dimension=0;
    INT2 zerobeyond=0;
    INT2 nested=0;
    INT2 ncomponents=0;
    int idcomp;

    TRACE_LNZDEF(("Loading deformation component sequence"));

    /*> Allocate the DefSeq object */

    (*pseq) = NULL;
    seq = new DefSeq();

    sts = STS_OK;

    /*> Load the sequence data from the binary source.  Load name, description,
        start and end dates, coordinate range, and dimension and model value
        type */

    if( sts == STS_OK ) sts = utlBinSrcLoadString( binsrc, BINSRC_CONTINUE, seq->name );
    if( sts == STS_OK ) sts = utlBinSrcLoadString( binsrc, BINSRC_CONTINUE, seq->description );
    if( sts == STS_OK ) sts = load_date( binsrc, BINSRC_CONTINUE, &(seq->startdate) );
    if( sts == STS_OK ) sts = load_date( binsrc, BINSRC_CONTINUE, &(seq->enddate) );

    if( sts == STS_OK ) sts = utlBinSrcLoad8( binsrc, BINSRC_CONTINUE, 1, &(seq->range.ymin) );
    if( sts == STS_OK ) sts = utlBinSrcLoad8( binsrc, BINSRC_CONTINUE, 1, &(seq->range.ymax) );
    if( sts == STS_OK ) sts = utlBinSrcLoad8( binsrc, BINSRC_CONTINUE, 1, &(seq->range.xmin) );
    if( sts == STS_OK ) sts = utlBinSrcLoad8( binsrc, BINSRC_CONTINUE, 1, &(seq->range.xmax) );

    if( sts == STS_OK && version == 1 ) sts = utlBinSrcLoad2( binsrc, BINSRC_CONTINUE, 1, &isvel );
    seq->valtype = isvel ? defValueVelocity : defValueDeformation;

    if( sts == STS_OK ) sts = utlBinSrcLoad2( binsrc, BINSRC_CONTINUE, 1, &dimension );
    seq->dimension = dimension;

    if( sts == STS_OK ) sts = utlBinSrcLoad2( binsrc, BINSRC_CONTINUE, 1, &zerobeyond );
    seq->zerobeyond = zerobeyond != 0;

    if( sts == STS_OK && version >= 2 ) sts = utlBinSrcLoad2( binsrc, BINSRC_CONTINUE, 1, &nested );
    seq->nested = nested != 0;

    if( sts == STS_OK && version >= 3 )
    {
        sts = load_version_field( binsrc, seq->startver, true );
        if( sts == STS_OK ) {
            sts = load_version_field( binsrc, seq->endver, true );
        }
    }

    if( sts == STS_OK ) sts = utlBinSrcLoad2( binsrc, BINSRC_CONTINUE, 1, &ncomponents );

    TRACE_LNZDEF(("Deformation component sequence header loaded: %s",seq->name.c_str()));

    /*> Load the each of the sequence components with load_component */

    idcomp = 0;
    while( sts == STS_OK && ncomponents-- )
    {
        hDefCmp cmp;
        sts = load_component( version, seq, binsrc, &cmp );
        if( sts == STS_OK )
        {
            idcomp++;
            cmp->id = idcomp;
            cmp->dimension = dimension;
            if( ! seq->firstcmp ) seq->firstcmp = cmp;
            if( seq->lastcmp ) seq->lastcmp->nextcmp = cmp;
            seq->lastcmp = cmp;
        }
    }

    /* If version is 1 then set up time model for version */

    if( version == 1 && seq->firstcmp )
    {
        hDefCmp cmp=seq->firstcmp;
        hDefCmp lastcmp=NULL;
        double fixfirst=0.0;
        double fixlast=0.0;
        for( ; cmp; lastcmp=cmp, cmp=cmp->nextcmp )
        {
            hDefCmp nextcmp=cmp->nextcmp;
            if( cmp->beforemode == defEvalInterp )
            {
                TimeModelPoint *tmi=&(cmp->timeModel[0]);
                if( lastcmp )
                {
                    tmi->year = utlDateAsYear(&(lastcmp->refdate));
                }
                else if( nextcmp )
                {
                    double y0=utlDateAsYear(&(cmp->refdate));
                    double y1=utlDateAsYear(&(nextcmp->refdate));
                    if( y1 > y0 ) 
                    {
                        tmi->year=utlDateAsYear(&(seq->startdate));
                        tmi->factor=(tmi->year-y1)/(y0-y1);
                        fixfirst=1.0-tmi->factor;
                    }
                }
            }
            if( cmp->aftermode == defEvalInterp )
            {
                TimeModelPoint *tmi=&(cmp->timeModel[cmp->nTimeModel()-1]);
                if( nextcmp )
                {
                    tmi->year = utlDateAsYear(&(nextcmp->refdate));
                }
                else if( lastcmp )
                {
                    double y0=utlDateAsYear(&(cmp->refdate));
                    double y1=utlDateAsYear(&(lastcmp->refdate));
                    if( y1 < y0 ) 
                    {
                        tmi->year=utlDateAsYear(&(seq->enddate));
                        tmi->factor=(tmi->year-y1)/(y0-y1);
                        fixlast=1.0-tmi->factor;
                    }
                }
            }
        }
        if( fixfirst != 0.0 )
        {
            cmp=seq->firstcmp->nextcmp;
            if( cmp->nTimeModel() > 1 )
            {
                cmp->timeModel[0].year=utlDateAsYear(&(seq->startdate));
                cmp->timeModel[0].factor=fixfirst;
            }
        }
        if( fixlast != 0.0 )
        {
            int ntm;
            cmp=seq->firstcmp;
            while( cmp->nextcmp->nextcmp ) cmp=cmp->nextcmp;
            ntm=cmp->nTimeModel()-1;
            if( ntm > 0 )
            {
                cmp->timeModel[ntm].year=utlDateAsYear(&(seq->enddate));
                cmp->timeModel[ntm].factor=fixlast;
            }
        }
    }

    /* If the sequence is not successfully loaded then delete the sequence
       resources that have been allocated with delete_def_seq */

    if( sts != STS_OK )
    {
        delete_def_seq( seq );
        RETURN_STATUS(sts);
    }

    TRACE_LNZDEF(("Deformation sequence components loaded: %s",seq->name.c_str()));

    (*pseq) = seq;
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

static StatusType check_range( const CrdRange *range, double x, double y )
{
    if( x < range->xmin || x > range->xmax ||
            y < range->ymin || y > range->ymax ) RETURN_STATUS( STS_CRD_RANGE_ERR );
    return STS_OK;
}



/*************************************************************************
** Function name: calc_seq_def
**//**
**    Calculates the displacement for a deformation sequence at a specific
**    location and time.  This is used where the sequence is a deformation
**    model (ie not a velocity model)
**
**  \param seq                 The sequence object
**  \param date                The date (in years) at which to
**                             evaluate the displacement
**  \param x                   The x coordinate
**  \param y                   The y coordinate
**  \param value               Array returning the calculated values
**
**  \return                    The return status
**
**************************************************************************
*/

static StatusType calc_seq_def( hDefSeq seq, double date, double x, double y, double *value )
{

    hDefCmp cmp;
    double factor;
    double cval[3];
    StatusType sts;
    StatusType stscalc;
    int i;

    cval[0] = cval[1] = cval[2] = 0;

    for( i = 0; i < seq->dimension; i++ )
    {
        value[i] = 0.0;
    }

    /*> Process the components in turn.  For each test if the evaluation point
        lies within the range of the component.  If so then determine the 
        scale factor, and add to the total deformation.  

        If this is a nested sequence then stop after the first one within
        range.
        */

    sts=STS_CRD_RANGE_ERR;
    stscalc=STS_OK;

    for( cmp = seq->firstcmp; cmp; cmp=cmp->nextcmp )
    {
        int ntm;
        StatusType stscmp;

        /*>> First check that the point is within the range defined for the component.
             If it is not then continue on to the next component. 
             */

        stscmp = check_range( &(cmp->range), x, y );
        if( stscmp != STS_OK ) continue;
        sts=STS_OK;

        /*>> Evaluate the scale factor to apply to the component based on the time
             model */

        ntm = cmp->nTimeModel()-1;
        factor=cmp->factor0;
        if( cmp->nTimeModel() > 0 )
        {
            if( date <= cmp->timeModel[0].year )
            {
                factor=cmp->factor0;
            }
            else if( date > cmp->timeModel[ntm].year )
            {
                factor=cmp->timeModel[ntm].factor;
            }
            else
            {
                TimeModelPoint *tm0, *tm1;
                for( i=1; i <= ntm; i++ )
                {
                    if( date <= cmp->timeModel[i].year )
                    {
                        break;
                    }
                }
                tm0=&(cmp->timeModel[i-1]);
                tm1=&(cmp->timeModel[i]);
                factor=((date-tm0->year)*tm1->factor+(tm1->year-date)*tm0->factor)/(tm1->year-tm0->year);
            }
        }

        /*>> If the factor is greater than 0 then calculate the model and add the 
             scaled model deformation to the total.
             */

        if( factor != 0.0 )
        {
            stscmp = calc_component_model( cmp, x, y, cval );
            if( stscmp == STS_OK )
            {
                for( i = 0; i < seq->dimension; i++ )
                {
                    value[i] += factor * cval[i];
                }
                TRACE_LNZDEF(("calc_seq_def: Adding component %d, factor %.4lf, value %.4lf %.4lf %.4lf",
                              cmp->id,factor,cval[0],cval[1],cval[2]));
            }
            else
            {
                if( stscalc == STS_OK ) stscalc = stscmp;
            }
        }

        /*>> If the sequence is nested then do not process any more components.  

             NOTE: This is not technically correct, as for triangulated components we have only
             tested whether the model is within bounds, not actually within the triangulation.
             */
        if( seq->nested ) break;
    }

    if( sts == STS_OK ) sts=stscalc;

    if( sts != STS_OK ) RETURN_STATUS( sts );
    return STS_OK;
}


/*************************************************************************
** Function name: add_sequence
**//**
**    Adds the displacement from a deformation sequence to that for the total
**    model
**
**  \param seq                 The deformation sequence
**  \param date                The evaluation date
**  \param x                   The x coordinate
**  \param y                   The y coordinate
**  \param value               The array to which the displacement is
**                             to be added.
**
**  \return                    The return status
**
**************************************************************************
*/

static StatusType add_sequence( hDefSeq seq, double date, double x,
                                double y, double* value )
{
    StatusType sts;
    double seqval[3];

    TRACE_LNZDEF(("add_sequence: Adding offset from sequence %d",seq->id));

    seqval[0] = 0.0;
    seqval[1] = 0.0;
    seqval[2] = 0.0;

    /*> If the date is not within the range specified by the model then
        return without doing anything */

    if( date < utlDateAsYear( &(seq->startdate) ) ||
            date > utlDateAsYear( &(seq->enddate) ) ) return STS_OK;

    /*> Check that the coordinates are within the range of the sequence
        with calc_range.

        If they are then evaluate the displacement for the deformation
        sequence using calc_seq_def (for deformation sequences)
        */

    sts = check_range( &(seq->range), x, y );

    if( sts == STS_OK )
    {
        sts = calc_seq_def( seq, date, x, y, seqval );
    }

    /*> If either the check or the evaluation gives a coordinate range error,
        then check whether the sequence is defined as having a zero displacement
        outside the coordinate range.  If it is then return without adding the
        displacement.  If it is not, then return an error */

    if( sts == STS_CRD_RANGE_ERR )
    {
        if( seq->zerobeyond ) sts = STS_OK;
        RETURN_STATUS( sts );
    }

    /*> Add the displacement components according to the dimension of the
        sequence (either height only, horizontal only, or 3d) */

    TRACE_LNZDEF(("add_sequence: id = %d, dimension = %d, value = %.4lf %.4lf %.4lf",
                  seq->id, (int)(seq->dimension), seqval[0], seqval[1], seqval[2] ));

    if( seq->dimension == 1 )
    {
        value[2] += seqval[0];
    }
    else
    {
        value[0] += seqval[0];
        value[1] += seqval[1];
        if( seq->dimension == 3 ) value[2] += seqval[2];
    }

    return sts;
}



/*************************************************************************
** Function name: create_def_ver
**//**
**   Function to load the data defining a LINZ deformation model
**
**  \param phver               The DefVer object to load
**
**  \return                    The return status
**
**************************************************************************
*/

static StatusType create_def_ver( hDefVer *phver )
{
    *phver=new DefVer();
    return STS_OK;
}


/*************************************************************************
** Function name: create_def_mod
**//**
**   Function to load the data defining a LINZ deformation model
**
**  \param pdef                The grid object to load
**  \param binsrc              The binary data source to load the
**                             model from
**
**  \return                    The return status
**
**************************************************************************
*/

static StatusType create_def_mod( hDefMod* pdef, hBinSrc binsrc)
{
    hDefMod def;
    int version;
    INT4 indexloc;
    StatusType sts;
    INT2 isgeog=0;
    INT2 nseq=0;
    INT2 nver=0;
    int idseq;
    hDefVer ver=0;

    (*pdef) = NULL;

    TRACE_LNZDEF(("create_def_mod"));
    version = check_header(binsrc, &indexloc);
    TRACE_LNZDEF(("create_def_mod: version %d  indexloc %ld",version,indexloc));
    if( ! version )
    {
        RETURN_STATUS(STS_INVALID_DATA);
    }
    def = new DefMod( binsrc );

    TRACE_LNZDEF(("Loading deformation model"));

    sts = STS_OK;
    ver=0;
    if( version < 3 ) { sts=create_def_ver( &ver ); def->firstver=ver; }
    if( sts == STS_OK ) sts = utlBinSrcLoadString( binsrc, indexloc, def->name );
    if( sts == STS_OK && version < 3 )
    {
        Version tmpver;
        sts = load_version_field( binsrc, tmpver, false );
        if( sts == STS_OK && ! tmpver.empty() ) {
            ver->version = tmpver;
        }
    }
    if( sts == STS_OK ) sts = utlBinSrcLoadString( binsrc, BINSRC_CONTINUE, def->crdsyscode );
    if( sts == STS_OK && version < 3 ) sts = load_version_description( binsrc, *ver );
    if( sts == STS_OK  && version < 3 ) sts = load_date( binsrc, BINSRC_CONTINUE, &(ver->versiondate) );
    if( sts == STS_OK ) sts = load_date( binsrc, BINSRC_CONTINUE, &(def->startdate) );
    if( sts == STS_OK ) sts = load_date( binsrc, BINSRC_CONTINUE, &(def->enddate) );

    if( sts == STS_OK ) sts = utlBinSrcLoad8( binsrc, BINSRC_CONTINUE, 1, &(def->range.ymin) );
    if( sts == STS_OK ) sts = utlBinSrcLoad8( binsrc, BINSRC_CONTINUE, 1, &(def->range.ymax) );
    if( sts == STS_OK ) sts = utlBinSrcLoad8( binsrc, BINSRC_CONTINUE, 1, &(def->range.xmin) );
    if( sts == STS_OK ) sts = utlBinSrcLoad8( binsrc, BINSRC_CONTINUE, 1, &(def->range.xmax) );

    if( sts == STS_OK ) sts = utlBinSrcLoad2( binsrc, BINSRC_CONTINUE, 1, &isgeog );
    def->isgeographical = isgeog != 0;

    if( version >= 3 )
    {
        hDefVer *pver=&(def->firstver);
        if( sts == STS_OK ) sts = utlBinSrcLoad2( binsrc, BINSRC_CONTINUE, 1, &nver );
        while( sts == STS_OK && nver-- )
        {
            sts=create_def_ver( &ver );
            if( sts != STS_OK ) break;
            *pver=ver;
            pver=&(ver->nextver);

            sts = load_version_field( binsrc, ver->version, false );
            if( sts == STS_OK ) sts = load_date( binsrc, BINSRC_CONTINUE, &(ver->versiondate) );
            if( sts == STS_OK ) sts = load_version_description( binsrc, *ver );
        }
    }

    if( sts == STS_OK ) sts = utlBinSrcLoad2( binsrc, BINSRC_CONTINUE, 1, &nseq );

    TRACE_LNZDEF(("Deformation model header loaded: %s",def->name.c_str()));

    idseq = 0;
    while( sts == STS_OK && nseq-- )
    {
        hDefSeq seq;
        sts = load_sequence( version, binsrc, &seq );
        if( sts == STS_OK )
        {
            idseq++;
            seq->id = idseq;
            if( ! def->firstseq ) def->firstseq = seq;
            if( def->lastseq ) def->lastseq->nextseq = seq;
            def->lastseq = seq;
        }
    }

    if( sts == STS_OK && ! (def->firstver) )
    {
        TRACE_LNZDEF(("Deformation model %s has no version information",def->name.c_str()));
        SET_STATUS(sts, STS_INVALID_DATA );
    }

    /* Define the current version */

    if( sts == STS_OK )
    {
        def->currver=def->firstver;
        ver=def->firstver;
        while( ver )
        {
            if( compare_versions( ver->version, def->currver->version ) > 0 )
            {
                def->currver=ver;
            }
            ver=ver->nextver;
        }
        sts=utlSetLinzDefVersion(def,def->currver->version);
    }

    if( sts != STS_OK )
    {
        delete_def_mod( def );
        RETURN_STATUS( sts );
    }

    TRACE_LNZDEF(("Deformation model loaded: %s",def->name.c_str()));


    *pdef = def;
    return STS_OK;
}


/*************************************************************************
** Function name: calc_def_mod
**//**
**    Calculate the displacement for a deformation model at a specific time
**    and location.
**
**  \param def                 The deformation model
**  \param date                The date (in years)
**  \param x                   The x coordinate
**  \param y                   The y coordinate
**  \param value               Array in which to return the
**                             displacement.
**
**  \return                    The return status
**
**************************************************************************
*/

static StatusType calc_def_mod( hDefMod def, double date,
                                double x, double y, double* value )
{
    hDefSeq seq;
    double calcval[3];
    StatusType sts;

    value[0] = value[1] = value[2] = 0;
    calcval[0] = calcval[1] = calcval[2] = 0;

    /*> If the coordinate are geographical then try to bring the x coordinate
        (longitude) into the range of the model by adding multiples of 360.
        */

    if( def->isgeographical )
    {
        /* Don't attempt to fix stupid values ... */
        if( x > -1000 && x < 1000 )
        {
            while( x < def->range.xmin ) x += 360;
            while( x > def->range.xmax ) x -= 360;
        }
    }

    /*> Check that the coordinates are within range of the model, and if
        not return an error */

    sts = check_range( &(def->range), x, y );
    if( sts != STS_OK ) RETURN_STATUS( sts );

    /*> Check that the date is within range of the model, and if not return an
        error */

    if( date < utlDateAsYear(&(def->startdate)) ||
            date > utlDateAsYear(&(def->enddate)) ) RETURN_STATUS( STS_INVALID_DATA );

    /*> Sum the effects of each deformation sequence using add_sequence */

    TRACE_LNZDEF(("calc_def_mod: Calculating deformation position %.8f %.8f date %.2f",
                  x,y,date));
    for( seq = def->firstseq; seq; seq = seq->nextseq )
    {
        if( ! seq->enabled ) continue;
        sts = add_sequence( seq, date, x, y, calcval );
        if( sts != STS_OK ) RETURN_STATUS( sts );
    }

    TRACE_LNZDEF(("calc_def_mod: Value calculated as %.4f %.4f %.4f",
                  calcval[0],calcval[1],calcval[2]));

    /*> Store the resulting displacement into the result array */

    value[0] = calcval[0];
    value[1] = calcval[1];
    value[2] = calcval[2];

    return STS_OK;
}


/*************************************************************************
** Function name: utlCreateLinzDef
**//**
**    Create and load a LinzDefModel object from a binary data source.
**    This is used to evaluate deformation at a given time and place.
**    The routine is just a wrapper around ::create_def_mod.
**
**  \param blob                The data source
**  \param pdef                The created model
**
**  \return                    The return status
**
**************************************************************************
*/

StatusType utlCreateLinzDef( hBinSrc blob, hLinzDefModel *pdef )
{
    StatusType sts;
    hDefMod def;

    (*pdef) = NULL;
    sts = create_def_mod( &def, blob );
    if( sts == STS_OK ) (*pdef) = def;

    RETURN_STATUS( sts );
}


/*************************************************************************
** Function name: utlReleaseLinzDef
**//**
**    Release the resources allocated to a LinzDefModel object.
**
**  \param def                 The model to release
**
**  \return                    The return status
**
**************************************************************************
*/

StatusType utlReleaseLinzDef( hLinzDefModel def )
{
    if( def )
    {
        delete_def_mod( (hDefMod) def );
    }
    return STS_OK;
}

/// Sets the version of the model to use, and enables the sequences that apply
/// to it.  Returns the return status.
StatusType utlSetLinzDefVersion(
    hLinzDefModel pdef,               ///< The model
    const std::string_view version )  ///< The version to configure for
{
    hDefMod def = static_cast<hDefMod>( pdef );
    hDefVer ver;
    hDefSeq seq;
    if( ! def ) RETURN_STATUS( STS_INVALID_DATA );
    TRACE_LNZDEF(("Setting deformation model to version %s",std::string(version).c_str()));

    ver=def->firstver;
    while( ver )
    {
        if( std::string_view( ver->version ) == version ) break;
        ver=ver->nextver;
    }
    if( ! ver )
    {
        TRACE_LNZDEF(("Cannot set model to version %s - not a valid version",
                    std::string(version).c_str()));
        RETURN_STATUS( STS_INVALID_DATA );
    }
    def->currver=ver;
    for( seq=def->firstseq; seq; seq=seq->nextseq )
    {
        int ok=1;
        if( compare_versions(ver->version,seq->startver) < 0 ) ok=0;
        if( compare_versions(ver->version,seq->endver) >= 0 ) ok=0;
        seq->enabled = ok != 0;
        TRACE_LNZDEF(("%s sequence %d %s",ok ? "Enabling" : "Disabling",
                    seq->id,seq->name.c_str()));
    }
    return STS_OK;
}


/// Gets the coordinate system code for the model, which stays valid while the
/// model does.  Returns the return status.
StatusType utlLinzDefCoordSysDef(
    hLinzDefModel def,          ///< The deformation model
    std::string_view &crdsys )  ///< Receives the coordinate system code, empty if the call fails
{
    crdsys = std::string_view();
    if( ! def ) RETURN_STATUS( STS_INVALID_DATA );
    crdsys = static_cast<hDefMod>( def )->crdsyscode;
    return STS_OK;
}


/// Returns text description information from a LinzDefModel deformation
/// model.  Returns the return status.
StatusType utlLinzDefTitle(
    hLinzDefModel def,                        ///< The model
    int nTitle,                               ///< What to return: 1 is the name, 2 is the description, 3 is the version number
    std::optional<std::string_view> &title )  ///< Receives the text while the model is valid, or nothing if there is none
{
    title = std::nullopt;
    if( ! def ) RETURN_STATUS( STS_INVALID_DATA );
    if( nTitle < 1 || nTitle > 3 ) RETURN_STATUS( STS_INVALID_DATA );
    const hDefMod model = static_cast<hDefMod>( def );
    switch( nTitle )
    {
    case 1:
        title = std::string_view( model->name );
        break;
    case 2:
        if( model->currver->description ) title = std::string_view( *model->currver->description );
        break;
    case 3:
        {
        std::string_view version = model->currver->version;
        if( version == "00000000" ){ version = std::string_view(); }
        title = version;
        }
        break;
    }
    return STS_OK;
}


/*************************************************************************
** Function name: utlCalcLinzDef
**//**
**    Routine to calculate the displacement due to the deformation model
**    at a specific time and place.  This is a wrapper around ::calc_def_mod.
**
**  \param def                 The deformation model
**  \param date                The date at which to evaluate the
**                             model (in years)
**  \param x                   The x coordinate
**  \param y                   The y coordinate
**  \param value               Array to receive the calculated
**                             displacement.
**
**  \return                    The return status
**
**************************************************************************
*/

StatusType utlCalcLinzDef( hLinzDefModel def, double date, double x, double y,
                           double * value)
{

    if( ! def ) RETURN_STATUS( STS_INVALID_DATA );
    return calc_def_mod( (hDefMod) def, date, x, y, value );
}

