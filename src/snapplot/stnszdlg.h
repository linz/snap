#ifndef STNSZDLG_H
#define STNSZDLG_H

/*
   $Log: stnszdlg.h,v $
   Revision 1.1  1996/01/03 22:32:25  CHRIS
   Initial revision

*/

#ifndef STNSZDLG_H_RCSID
#define STNSZDLG_H_RCSID "$Id: stnszdlg.h,v 1.1 1996/01/03 22:32:25 CHRIS Exp $"
#endif

typedef struct
{
    double symSize;
    double txtSize;
    char symAuto;
    char txtAuto;
    char dfltFont;
    char ignoreOffset;
} StationSizeDef;

#ifdef __cplusplus
//extern "C"
#endif
int EditStationSize( StationSizeDef *def );

#endif
