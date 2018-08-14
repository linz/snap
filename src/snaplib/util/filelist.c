#include "snapconfig.h"
#include <stdio.h>
#include <string.h>
#include "util/chkalloc.h"
#include "util/filelist.h"
#include "util/dstring.h"
#include "util/strarray.h"

#define INIT_FILENAME_COUNT 100

typedef struct 
{
    const char *filename;
    const char *filetype;
} recfilename;

static int recording=0;
static strarray filetypes;
static recfilename *filenames=0;
static int nfilenames=0;
static int maxfilenames=0;

int set_record_filenames( int record )
{
    int wasrecording=recording;
    if( record && ! filenames )
    {
        strarray_init(&filetypes);
        filenames= (recfilename *) check_malloc(sizeof(recfilename)*INIT_FILENAME_COUNT);
        nfilenames=0;
        maxfilenames=INIT_FILENAME_COUNT;
    }
    recording=record;
    return wasrecording;
}

int record_filename( const char *filename, const char *filetype )
{
    if( ! recording ) return NO_FILENAME_ID;
    if( ! filename || ! filetype ) return NO_FILENAME_ID;
    for( int i=0; i<nfilenames; i++ )
    {
        if( strcmp(filename,filenames[i].filename) == 0 )
        {
            return i;
        }
    }
    int ftypeid=strarray_find(&filetypes,filetype);
    if( ftypeid == STRARRAY_NOT_FOUND ) ftypeid=strarray_add(&filetypes,filetype);
    filetype=strarray_get(&filetypes,ftypeid);
    filename=copy_string(filename);
    while( nfilenames >= maxfilenames )
    {
        maxfilenames *= 2;
        filenames=(recfilename *) check_realloc(filenames,sizeof(recfilename)*maxfilenames);
    }
    filenames[nfilenames].filetype=filetype;
    filenames[nfilenames].filename=filename;
    nfilenames++;
    return nfilenames-1;
}

int recorded_filename_count()
{
    return nfilenames;
}

const char *recorded_filename( int i, const char **pfiletype )
{
    const char *filename=0;
    const char *filetype=0;
    if( filenames && i >= 0 && i < nfilenames )
    {
        filename=filenames[i].filename;
        filetype=filenames[i].filetype;
    }
    if( pfiletype ) *pfiletype=filetype;
    return filename;
}

void delete_recorded_filenames()
{
    for( int i = 0; i < nfilenames; i++ )
    {
        check_free( (void *) filenames[i].filename);
    }
    check_free( filenames );
    strarray_delete( &filetypes );
    nfilenames=0;
    maxfilenames=0;
    filenames=0;
}


