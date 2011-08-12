#ifndef INFOWIN_H
#define INFOWIN_H

/*
   $Log: infowin.h,v $
   Revision 1.1  1996/01/03 22:21:12  CHRIS
   Initial revision

*/

#ifndef INFOWIN_H_RCSID
#define INFOWIN_H_RCSID "$Id: infowin.h,v 1.1 1996/01/03 22:21:12 CHRIS Exp $"
#endif

/* Definition of a function used to put text into an information window. */

enum { ptfNone, ptfTitleBlock, ptfStation, ptfLine, ptfObs, ptfStnList, ptfSres };

typedef struct
{
    char type;    /* Defined by enum above */
    int from;
    int to;
    int obs_id;
} PutTextInfo;


typedef void (*PutTextFunc)( void *object, PutTextInfo *jump, const char *text );

#endif
