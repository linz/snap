#include "snapconfig.h"
#include <stdio.h>

#include "snapdata/datatype.h"
#include "snapdata/survdata.h"
#include "util/classify.h"

snap_data_type snap_type[] =
{
    /*
        O True if the type cannot be joined with others (eg vectors)
        R True if uses a refraction coefficient
        D True if uses a distance scale factor
        B True if uses a bearing orientation error

       Type Error   Obsclass    O */
    {  HD, DS_ERR,  SD_OBSDATA, 0, 0 },
    {  SD, DS_ERR,  SD_OBSDATA, 0, 0 },
    {  ED, DS_ERR,  SD_OBSDATA, 0, 0 },
    {  MD, DS_ERR,  SD_OBSDATA, 0, 0 },
    {  DR, DS_ERR,  SD_OBSDATA, 0, 0 },
    {  HA, HA_ERR,  SD_OBSDATA, 0, 0 },
    {  AZ, AZ_ERR,  SD_OBSDATA, 0, 0 },
    {  ZD, ZD_ERR,  SD_OBSDATA, 0, 0 },
    {  LV, LV_ERR,  SD_OBSDATA, 0, 0 },
    {  GB, GB_ERR,  SD_VECDATA, 1, 0 },
    {  LT, LT_ERR,  SD_PNTDATA, 0, 0 },
    {  LN, LN_ERR,  SD_PNTDATA, 0, 0 },
    {  PB, AZ_ERR,  SD_OBSDATA, 0, 0 },
    {  OH, OH_ERR,  SD_PNTDATA, 0, 0 },
    {  EH, EH_ERR,  SD_PNTDATA, 0, 0 },
    {  GX, GX_ERR,  SD_VECDATA, 1, 0 },
    {  -1 }
};


snap_data_type *snap_data_type_from_id( int idtype )
{
    static bool init = false;
    static snap_data_type *snaptypebyid[NOBSTYPE];

    if( ! init )
    {
        init = true;
        for( int i = 0; i < NOBSTYPE; i++ ) snaptypebyid[i] = 0;

        for( snap_data_type *sd = snap_type; sd->type != -1; sd++ )
        {
            sd->datatype = datatypedef_from_id( sd->type );
            snaptypebyid[sd->type] = sd;
        }
    }
    return snaptypebyid[idtype];
}

int get_obs_classification_id( survdata *sd, trgtdata *t, int class_id )
{
    int ic;
    for( ic = 0; ic < t->nclass; ic++ )
    {
        classdata *cd;
        cd = sd->clsf + ic + t->iclass;
        if( cd->class_id == class_id )
        {
            return cd->name_id;
        }
    }
    return CLASS_VALUE_NOT_DEFINED;
}
