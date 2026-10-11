#ifndef INFOWIN_H
#define INFOWIN_H

/*
   $Log: infowin.h,v $
   Revision 1.1  1996/01/03 22:21:12  CHRIS
   Initial revision

*/

#include <string>

/* Definition of a function used to put text into an information window. */

enum { ptfNone, ptfTitleBlock, ptfStation, ptfLine, ptfObs, ptfStnList, ptfSres };

struct PutTextInfo
{
    char type;    /* Defined by enum above */
    int from;
    int to;
    int obs_id;
};


typedef void (*PutTextFunc)( void *object, PutTextInfo *jump, const std::string &text );

/* Calls a PutTextFunc, passing no PutTextInfo */

inline void put_text( void *object, PutTextFunc f, const std::string &text )
{
    (*f)( object, nullptr, text );
}

#endif
