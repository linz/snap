#ifndef _COEFS_H
#define _COEFS_H

/*
   $Log: coefs.h,v $
   Revision 1.1  1996/01/03 21:57:56  CHRIS
   Initial revision

*/

#ifndef _NETWORK_H
#include "network/network.h"
#include "snap/genparam.h"
#endif

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
