#include "snapconfig.h"

/*
   $Log: plotbin.c,v $
   Revision 1.2  1997/04/28 10:58:55  CHRIS
   Added function to access notes associated with data.  Not properly done -
   does not share common code with snap\notedata.c

   Revision 1.1  1996/01/03 22:23:34  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <string.h>
#include <filesystem>
#include <optional>
#include <string>
#include <stdlib.h>
#include <math.h>

#include "util/errdef.h"
#include "util/fileutil.h"

#include "plotbin.h"
#include "snap/genparam.h"
#include "util/binfile.h"
#include "snapdata/survdata.h"
#include "snap/survfile.h"
#include "snap/bindata.h"
#include "snap/stnadj.h"
#include "snap/rftrndmp.h"
#include "snap/snapglob.h"
#include "snap/snapglob_bin.h"
#include "util/classify.h"
#include "snap/survfile.h"
#include "plotconn.h"
#include "plotstns.h"
#include "snapplot_util.h"


static int reload_observations( BINARY_FILE *b );
static int reload_relative_covariances( BINARY_FILE *b );

/* Need a binary file to store data.  Use either the input binary file if
   it exists and is OK, or create a temporary file if it does not (or is not)
*/

static BINARY_FILE *b = NULL;
static FILE *f = NULL;
static std::optional<bindata> bd;

int reload_binary_data( )
{
    int sts;

    const std::string bfn = std::filesystem::path( native_path(command_file->root) ).replace_extension().string() + BINFILE_EXT;

    auto [file, result] = open_binary_file( bfn, BINFILE_SIGNATURE );
    b = file;

    if( !b )
    {
        if( result == BinFileOpenResult::InvalidVersion )
        {
            handle_error( WARNING_ERROR | SHOW_DIALOG,
                          "Cannot reload data - binary file version is not compatible with this version of SNAP",
                          bfn );
        }
        return NO_MORE_DATA;
    }

    print_log("     Reloading data from binary file...\n");

    if( reload_snap_globals( b ) != OK ||
            reload_stations( b ) != OK ||
            reload_filenames( b ) != OK ||
            reload_rftransformations( b ) != OK ||
            reload_parameters( b ) != OK )
    {

        handle_error( FILE_OPEN_ERROR | SHOW_DIALOG, "Cannot reload data from binary file",
                      bfn);
        sts = FILE_READ_ERROR;
    }
    else
    {
        sts = OK;
        set_binary_data( 1 );
        reload_obs_classes( b );
        reload_covariances( b );
        reload_relative_covariances( b );
        reload_observations( b );
    }

    return sts;
}

void load_observations_from_binary( void )
{
    if( ! bd ) bd.emplace();
    init_get_bindata( 0L );

    while( get_bindata( SURVDATA, *bd ) == OK )
    {
        add_survdata_connections( bd->survey_data(), bd->loc );
    }
}


/* Doesn't sit comfortably here and doesn't use a library for definition
   of note functions but.. */

inline constexpr long NOTEWIDTH = 90;

void display_note_text( void *dest, PutTextFunc f, int64_t loc )
{
    PutTextInfo jmp;
    long size;
    int type;
    int firstline, c;
    if( loc < 0 ) return;
    const int64_t curloc = ftell64( b->f );
    fseek64( b->f, loc, SEEK_SET );

    jmp.type = ptfNone;
    firstline = 1;

    std::string prefix = "Note: ";
    while( read_bindata_header( &size, &type ) && type == NOTEDATA )
    {
        c = fgetc( b->f );
        size--;
        if( firstline && c != '\n' ) break;
        firstline = 0;
        size -= 2;  /* To account for the tail of the note */
        while ( size )
        {
            const long block = size > NOTEWIDTH ? NOTEWIDTH : size;
            std::string text( block, '\0' );
            fread( &text[0], 1, block, b->f );
            text.resize( strlen( text.c_str() ) );  /* The text ends at the first NUL, as it did as a C string */
            (*f)( dest, &jmp, prefix + text );
            size -= block;
        }
        fgetc(b->f); fgetc(b->f);  /* Tail of the note */
        prefix = "      ";
    }

    fseek64( b->f, curloc, SEEK_SET );
}


static int reload_observations( BINARY_FILE *bf )
{

    if( find_section( bf, "OBSERVATIONS" ) != OK ) return MISSING_DATA;

    print_log("     Reloading observations\n");

    init_bindata( bf->f );

    load_observations_from_binary();

    return OK;
}

survdata *get_survdata_from_binary( int64_t loc )
{
    if( !bd ) bd.emplace();
    init_get_bindata( loc );
    if( get_bindata(ANYDATATYPE,*bd) != OK )  return nullptr;
    if( bd->bintype != SURVDATA ) return nullptr;
    return bd->survey_data();
}

static int reload_relative_covariances( BINARY_FILE *b )
{
    int from, to;
    double cvr[6];

    if( find_section( b, "STATION_RELATIVE_COVARIANCES" ) != OK ) return MISSING_DATA;

    for(;;)
    {
        if( fread( &from, sizeof(from), 1, b->f ) != 1 ) break;
        if( from < 0 ) break;
        fread( &to, sizeof(to), 1, b->f );
        fread( cvr, sizeof(cvr), 1, b->f );
        add_relative_covariance( from, to, cvr );
    }

    return check_end_section( b );
}

void open_data_source( void )
{
    if( ! b  && ! f )
    {
        f = snaptmpfile();
        if( f ) fputs("SNAPPLOT TEMP FILE\r\n\x1A",f);
        init_bindata( f );
    }
}

void close_data_source( void )
{
    bd.reset();
    if( b ) {close_binary_file(b); b = NULL; }
    if( f ) {fclose(f); f = NULL;}
}
