#ifndef _LOADDATA_H
#define _LOADDATA_H

/*
   $Log: loaddata.h,v $
   Revision 1.1  1995/12/22 18:48:04  CHRIS
   Initial revision

*/

#ifndef LOADDATA_H_RCSID
#define LOADDATA_H_RCSID "$Id: loaddata.h,v 1.1 1995/12/22 18:48:04 CHRIS Exp $"
#endif

#ifndef _DATATYPE_H
#include "snapdata/datatype.h"
#endif
#ifndef _SURVDATA_H
#include "snapdata/survdata.h"
#endif

/* Init load data takes the addresses of three callback functions which
   act as translators between names (of stations, refraction coeffs,
   etc, and codes.

   The functions take an ID type, and for some types of id a group
   identifier, and return a long id for the code (or a code for the
   long id).  For most data types the long value returned is converted
   to a int.

   The type must be one of the following:

   type         int?  Group_id      Description

   ID_STATION     Y        -          Station code
   ID_PROJCTN     Y        -          Reference frame
   ID_CLASSTYPE   Y        -          Classification type
   ID_CLASSNAME   Y      class_id     Name of a classification within a type
   ID_COEF        Y      coef_id      Name of a coefficient type
   ID_COEFCLASS   Y      coef_id      Returns id of coef class
   ID_SYSERROR    Y        -          Systematic error type
   ID_NOTE        N     continuation  Note attached to data

   Return values of 0 are taken as missing or undefined

   The third function is for calculations, currently calculating the
   distance between two stations.  It takes three parameters and returns
   an double value.  The parameters are a type and two ids

   type             id1              id2              Value
   CALC_DISTANCE    From station id  To station id    Slope distance
   CALC_HDIST       From station id  To station id    Horizonal distance

*/

enum { ID_STATION, ID_PROJCTN, ID_CLASSTYPE, ID_CLASSNAME, ID_COEF,
       ID_COEFCLASS, ID_SYSERR, ID_NOTE, N_ID_TYPE
     };

/* These must match the coef_classes array in loaddata.c */

enum { COEF_CLASS_DISTSF, COEF_CLASS_BRNGREF, COEF_CLASS_REFCOEF, COEF_CLASS_REFFRM, N_COEF_CLASSES };

/* Calculation types */

enum { CALC_DISTANCE, CALC_HDIST };

/*  */

struct coef_class_info
{
    const char *default_classname;
    bool datatypedef::*useclass;
};

coef_class_info *coef_class( int coeftype );
void set_coef_class( int coeftype, const char *name );

/* The following two routines are called by the "main" program
   before and after loading a data file */

void init_load_data( void (*usedata_func)( survdata *sd ),
                     long (*idfunc)( int type, int group_id, const char *code ),
                     const char * (*namefunc)( int type, int group_id, long id ),
                     double (*calcfunc)( int type, long id1, long id2 ));
void term_load_data( void );


/* Station recode function */

#define RECODE_IGNORE_CODE "*"

void set_stn_recode_func( 
        const char *(*recode)( void *recodedata, const char *code, double date ), 
        void *recodedata);

/* The following routine can be called to enable handling of GPS covariances.
   Otherwise the request for a covariance array (ldt_covariance)
   returns NULL, and input covariance information should be ignored */


void set_gpscvr_func( void (*func)( survdata *vd, int cvrtype,
                                    double *mmerr ));


/* The following routines are called by the data file reader.  The load
   data routines assemble these to convert the data to a standard internal
   format */

long ldt_get_id( int type, int group_id, const char *code );
const char *ldt_get_code( int type, int group_id, long id );
double ldt_calc_value( int calc_type, long id1, long id2 );

/* Set state - can happen at any time */

void ldt_file( int file );

/* Starts an observation set... */

void ldt_inststn( int stn_id, double ihgt );

/* Observation set state info */

void ldt_date( double date );
void ldt_time( double time );

void ldt_projection( int proj_id );
void ldt_tgtstn( int stn_id, double ihgt );

/* Initiallizes an observation */
void ldt_nextdata( int type );
void ldt_value( double *value );
void ldt_error( double *error );

void ldt_unused( void );

void ldt_lineno( long lineno );
void ldt_obs_id( int id );
void ldt_classification( int class_id, int name_id );
void ldt_syserr( int syserr_id, double influence );
void ldt_vecsyserr( int syserr_id, double influence[] );

void ldt_prefix_note( const char *note );
void ldt_note( const char *note );

/* Once all observations in a set are created */

ltmat ldt_covariance( int cvrtype, double *mmerr );

void ldt_cancel_data( void );
void ldt_cancel_trgt( void );
void ldt_cancel_inst( void );
int ldt_data_cancelled( void );

/* At the end of a set of data */

void ldt_end_data( void );

/* Covariance types for vecdata_covariance */

#define CVR_DEFAULT      0
#define CVR_TOPOCENTRIC  1
#define CVR_DIAGONAL     2
#define CVR_FULL         3
#define CVR_CORRELATION  4
#define CVR_ENU_CORRELATION 5

/* Default reference frame names */

#define DEFAULT_REFFRM_NAME   "GPS"
#define DEFAULT_REFCOEF_NAME  "DEFAULT"
#define DEFAULT_PROJECTION    "NONE"

#endif

