#include "snapconfig.h"
/*
   $Log: notedata.c,v $
   Revision 1.2  1999/06/14 09:20:31  ccrook
   Fixed minor bug with corrupting retrieval of comments > 80 characters long.

   Revision 1.1  1996/01/03 22:00:48  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <string.h>

#include "output.h"
#include "snapdata/survdata.h"
#include "snap/bindata.h"
#include "notedata.h"


/*===================================================*/


/* Routines to handle notes */
/* If the note is continued, the first character is set to blank,
   otherwise it is a line-feed character */

long save_note( const char *text, int continued )
{
    int nch;
    int64_t noteloc;
    long size, loc;
    nch = strlen( text );
    size = nch+3;

    // For the moment we don't want to propogate int64_t to 
    // the note id (which is a file location), as this would then
    // impact on snap_id, and thence to all ids.  This can be 
    // sorted as/when the snap code base gets a proper overhaul.
    //
    // In practice long will be big enough as binary files are 
    // unlikely to exceed 2Gb until the covariance data is written to
    // them, and this is after the notes have been written (which
    // is during the data load phase).

    noteloc = write_bindata_header( size, NOTEDATA );
    loc = (long) noteloc;
    if (loc != noteloc) loc = 0;

    fputc( continued ? ' ' : '\n', bindata_file );
    fwrite( text, nch, 1, bindata_file );
    fputc( '\n', bindata_file );
    fputc( 0, bindata_file );

    return loc;
}



void list_note( FILE *out, long loc )
{
    int64_t curloc;
    char note[81];
    long size;
    int block, type;
    int firstline, c;
    if( loc < 0 || !output_notes ) return;
    curloc = ftell64( bindata_file );
    fseek( bindata_file, loc, SEEK_SET );

    firstline = 1;
    while( read_bindata_header( &size, &type ) && type == NOTEDATA )
    {
        c = fgetc( bindata_file );
        if( firstline )
        {
            fputc( c, out );
            firstline = 0;
        }
        else
        {
            if( c != ' ' ) break;
        }
        size--;
        note[80] = 0;
        while ( size )
        {
            block = size > 80 ? 80 : size;
            fread( note, 1, block, bindata_file );
            fputs( note, out );
            size -= block;
        }
    }

    fseek64( bindata_file, curloc, SEEK_SET );
}
