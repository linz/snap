#ifndef ERRORDLG_H
#define ERRORDLG_H

/*
   $Log: errordlg.h,v $
   Revision 1.1  1996/01/03 22:16:50  CHRIS
   Initial revision

*/

#ifndef ERRORDLG_H_RCSID
#define ERRORDLG_H_RCSID "$Id: errordlg.h,v 1.1 1996/01/03 22:16:50 CHRIS Exp $"
#endif

typedef struct
{
    char aPost;
    char stdErr;
    double confLim;
    double hor_factor;
    double vrt_factor;
    char hor_auto;
    char vrt_auto;
} ErrorScaleDef;

#ifdef __cplusplus
//extern "C"
#endif
int EditErrorScale( ErrorScaleDef *def );

#endif
