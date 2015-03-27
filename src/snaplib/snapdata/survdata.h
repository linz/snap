#ifndef _SURVDATA_H
#define _SURVDATA_H

/*
   $Log: survdata.h,v $
   Revision 1.1  1995/12/22 18:49:01  CHRIS
   Initial revision

*/

#ifndef SURVDATA_H_RCSID
#define SURVDATA_H_RCSID "$Id: survdata.h,v 1.1 1995/12/22 18:49:01 CHRIS Exp $"
#endif

#ifndef _SYMMATRX_H
#include "util/symmatrx.h"
#endif

#ifndef _DATATYPE_H
#include "snapdata/datatype.h"
#endif

/* Data formats used by loaddata routines */

enum { SD_OBSDATA, SD_VECDATA, SD_PNTDATA };

/* Structure defining observational data types in the SNAP file */

enum { DS_ERR, HA_ERR, AZ_ERR, ZD_ERR, LV_ERR, LT_ERR, LN_ERR, GB_ERR, GX_ERR, OH_ERR, EH_ERR, NERRTYPE };

typedef struct
{
    int type;         /* As defined in DATATYPE.H */
    int errortype;    /* Index to type of default error */
    int obsclass;     /* SD_PNTDATA, SD_OBSDATA, SD_VECDATA */
    int oneonly;     /*  True if the type cannot be joined with others (eg vectors) */
    datatypedef *datatype; /* The generic type definition */
} snap_data_type;

typedef struct
{
    int  class_id;  /* The index of the classication */
    int  name_id;   /* The index of the obs within the classification */
} classdata;

typedef struct
{
    int  prm_id;    /* Id of the systematic error parameter */
    double influence; /* Influence of the parameter on the observation */
} syserrdata;

typedef struct
{
    int  to;        /* The index of the target station */
    double tohgt;     /* The instrument height at the target station */
    int  type;      /* The type of data */
    int  id;        /* An optional id for the observation */
    int  lineno;    /* The line number in the data file - used to pinpoint problems */
    int  nclass;    /* Number of classifications for the obs */
    int  iclass;    /* Index of first class in array of classes */
    int  nsyserr;   /* Number of systematic errors */
    int  isyserr;   /* Index of first systematic error in array of syserrs */
    char   unused;    /* Flags whether the observation has been rejected */
    long   noteloc;   /* Identifies a note associated with the obs */
} trgtdata;

/* NOTE: each variant structure used within typedef survdata, defined
   below, must have a trgtdata as its first element */

typedef struct         /* The data relating to the target station and obs */
{
    trgtdata tgt;     /* The target data */
    double value;     /* The observation value */
    double error;     /* The associated error */
    double calc;      /* The calculated value */
    double calcerr;   /* The error of the calculated value */
    double residual;  /* The residual */
    double reserr;    /* The error of the residual */
    double sres;      /* The standardised residual */
    int  refcoef;   /* Refraction coefficient id */
    int  prm_id;    /* A parameter which may be one of
                          bearing orientation error  for azimuth data
                          distance scale factor      for any type of distance data
                          */
} obsdata;

typedef struct          /* The data relating to the target station and obs */
{
    trgtdata tgt;      /* The target data */
    double vector[3];  /* The observed vector */
    double calc[3];    /* The calculated vector */
    double residual[3];/* The residuals */
    double vsres;      /* The vector standardised residual */
    int  rank;       /* The rank of vsres */
} vecdata;

typedef struct          /* The data relating to the target station and obs */
{
    trgtdata tgt;      /* The target data (even though there isn't a target!) */
    double value;     /* The observation value */
    double error;     /* The associated error */
    double calc;      /* The calculated value */
    double calcerr;   /* The error of the calculated value */
    double residual;  /* The residual */
    double reserr;    /* The error of the residual */
    double sres;      /* The standardised residual */
} pntdata;

typedef struct           /* Data relating to an observation or set of obs */
{
    int  from;        /* The instrument station */
    double fromhgt;     /* The instrument station height */
    double date;        /* The date of the data */
    int  reffrm;      /* Id of the reference frame */
    int  file;        /* The number of the data file from which it is taken */
    double schval;      /* The value and variance of an implicit parameter */
    double schvar;      /* calculated using Schreibers equations */
    int  format;      /* One of the SD_ values above, defines the obs type */
    int  nobs;        /* The number of observations defined */
    int  obssize;     /* The size of each element in the array of observations */
    int  ncvr;        /* The size of the covariance matrix */
    int  nclass;      /* The number of classifications stored */
    int  nsyserr;     /* The number of systematic errors stored */

    /* The following information is not stored in the binary file */
    /* Though the pointed-to objects are... */
    union               /* The array of observations */
    {
        obsdata *odata;
        vecdata *vdata;
        pntdata *pdata;
    } obs;
    long obsalloc;      /* Space allocated for the observations */
    int maxcvr;       /* Allocated size of covariance matrices */
    int maxclass;     /* Allocated size of classification array */
    int maxsyserr;    /* Allocated size of systematic error array */
    ltmat  cvr;         /* The covariance of the observations (vector data only) */
    ltmat  calccvr;     /* The covariance of the calculate values (vector data only) */
    ltmat  rescvr;      /* The covariance matrix of the residuals (vector data only */
    classdata *clsf;    /* Array of classifications - observations index into this */
    syserrdata *syserr; /* The list of systematic errors */
} survdata;

/* Flag bits set in the unused flag */

#define REJECT_OBS_BIT   0x01
#define IGNORE_OBS_BIT   0x02
#define UNUSED_OBS_BIT   0x04   /* Unused for another reason...*/

/* Get the i'th target for an observations */

#define get_trgtdata(o,i) \
    ((trgtdata *)(void *)( (unsigned char *)(void *)(o->obs.odata) + o->obssize * i ))

snap_data_type *snap_data_type_from_id( int idtype );

#endif

