#ifndef STNSZDLG_H
#define STNSZDLG_H

/*
   $Log: stnszdlg.h,v $
   Revision 1.1  1996/01/03 22:32:25  CHRIS
   Initial revision

*/

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
