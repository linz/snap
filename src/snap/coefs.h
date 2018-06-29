#ifndef _COEFS_H
#define _COEFS_H

/*
   $Log: coefs.h,v $
   Revision 1.1  1996/01/03 21:57:56  CHRIS
   Initial revision

*/

#ifndef _NETWORK_H
#include "network/network.h"
#endif

#define COEFLEN 20
#define DEFAULT_REFCOEF 0.075

/* Note: the numbers defined for PRM_ correspond to indexes in the
   coefprefix and coefpfxlen arrays in coefs.c */

#define PRM_REFCOEF 0
#define PRM_DISTSF  1
#define PRM_BRNGREF 2
#define PRM_SYSERR  3

void define_coef( int type, const char *refcoef, double value, int adjust );
void define_coef_match( int type, const char *coef1, const char *coef2 );

void set_default_refcoef( double rc );
int refcoef_prm( const char *refcoef );
const char *refcoef_name( int rc );
double zd_ref_correction( int rc, station *st1, station *st2, void *hA, int irow );

int distsf_prm( const char *distsf );
const char *distsf_name( int ds );
double distsf_correction( int ds, double dist, void *hA, int irow );

int brngref_prm( const char *brngref );
const char *brngref_name( int br );
double brngref_correction( int br, void *hA, int irow );

int syserr_prm( const char *syserr );
const char *syserr_name( int se );
double syserr_correction( int se, double influence, void *hA, int irow );

#endif
