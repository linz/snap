#include "snapconfig.h"

#include <stdio.h>
#include <string.h>

#include "util/fileutil.h"
#include "util/getversion.h"

#define MAXVER 20
#define MAXID  40
#define MAXNAME  20

static char progversion[MAXVER+MAXID+2]={0};

std::string getProgramName()
{
    return image_name();
}

const char *getProgramVersion( const char *version )
{
    if( progversion[0] ) return progversion;

    char format[20];
    std::string versionfile=build_filespec(image_dir(),"VERSION","");
    if( path_exists(versionfile))
    {
        sprintf(format,"%%%ds",MAXVER);
        FILE *vf=fopen(versionfile.c_str(),"r");
        if( vf )
        {
            fscanf(vf,format,progversion);
        }
        fclose(vf);
    }
    if( ! progversion[0] )
    {
        strncpy(progversion,version,MAXVER);
    }
    versionfile=build_filespec(image_dir(),"VERSIONID","");
    if( path_exists(versionfile))
    {
        char *pv=progversion+strlen(progversion);
        FILE *vf=fopen(versionfile.c_str(),"r");
        if( vf )
        {
            sprintf(format,"%%%ds",MAXID);
            *pv='-';
            fscanf(vf,format,pv+1);
        }
        fclose(vf);
    }
    return progversion;
}
