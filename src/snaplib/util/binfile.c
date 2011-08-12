#include "snapconfig.h"

/*
   $Log: binfile.c,v $
   Revision 1.4  2004/04/22 02:35:23  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.3  1998/06/25 02:13:37  ccrook
   Fixing bug introduced when a file is opened for reading such that the end of section code
   is run in error when

   Revision 1.2  1998/06/03 22:56:44  ccrook
   Added support for binary file version and section versions to facilitate building upwardly
   compatible binary files.

   Revision 1.1  1995/12/22 18:52:38  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "util/chkalloc.h"
#include "util/binfile.h"
#include "util/errdef.h"

#ifdef _MSC_VER
#define MS_LOCKING
#endif

#ifdef MS_LOCKING
#include <io.h>
#include <fcntl.h>
#include <share.h>
#include <sys/stat.h>
#endif

#define SIG_TRAILER "\r\n\032"
#define ENDSECTION  "End_of_Section"
#define VERSION_SECTION "\x01_BF_VERSION\x02"
#define BF_VERSION 1

static char rcsid[]="$Id: binfile.c,v 1.4 2004/04/22 02:35:23 ccrook Exp $";

BINARY_FILE *create_binary_file( char *fname, char *signature )
{
    FILE *f = NULL;
    BINARY_FILE *b;
    char zero = 0;

#ifdef MS_LOCKING
    {
        int fd;
        int oflag;
        int shflag;
        int pflag;

        oflag = _O_RDWR | _O_CREAT | _O_TRUNC | _O_BINARY;
        shflag = _SH_DENYRW;
        pflag = _S_IREAD | _S_IWRITE;

        fd = _sopen( fname, oflag, shflag, pflag );

        if( fd != -1 )
        {

            f = _fdopen( fd, "w+b" );
        }
    }
#else
    f = fopen( fname, "w+b" );
#endif

    if( !f ) return NULL;

    b = (BINARY_FILE *) check_malloc( sizeof( BINARY_FILE ) );
    fwrite(signature, strlen(signature), 1, f );
    fwrite(SIG_TRAILER,strlen(SIG_TRAILER),1,f);      /* DOS eof character */

    b->f = f;
    b->start = ftell(f);

    /* Invalidate the first character of the signature, so that the file will
       only be valid if not closed properly */

    b->sigchar = signature[0];
    fseek(b->f,0,SEEK_SET);
    fwrite( &zero, sizeof(char), 1, b->f );
    fseek( b->f, b->start, SEEK_SET );
    b->section_start = 0L;
    b->section_version = 0L;
    b->bf_version = BF_VERSION;

    create_section_ex( b, VERSION_SECTION, BF_VERSION );
    end_section(b);

    return b;
}


BINARY_FILE *open_binary_file( char *fname, char *signature )
{
    FILE *f = NULL;
    BINARY_FILE *b;
    char *sig;
    int nsig;

    /* Note: this implementation has
       removed write permissions for opening existing file,
      could be reinstated, but would need to change lock option
      shflag depending upon whether needed RO or RW - ie another
      parameter in call. */

#ifdef MS_LOCKING
    {
        int fd;
        int oflag;
        int shflag;
        int pflag;

        oflag = _O_RDONLY | _O_BINARY;
        shflag = _SH_DENYWR;
        pflag = _S_IREAD | _S_IWRITE;

        fd = _sopen( fname, oflag, shflag, pflag );

        if( fd != -1 )
        {
            f = _fdopen( fd, "rb" );
        }
    }
#else
    f = fopen( fname, "rb" );
#endif
    if( !f ) return NULL;

    nsig = strlen(signature) + strlen(SIG_TRAILER);

    sig = (char *) check_malloc( nsig );
    if( fread( sig, nsig, 1, f ) != 1 ||
            memcmp( signature, sig, strlen(signature) ) != 0 ||
            memcmp( SIG_TRAILER, sig+strlen(signature), strlen(SIG_TRAILER)) != 0 )
    {
        check_free( sig );
        fclose(f);
        return NULL;
    }

    check_free( sig );

    b = (BINARY_FILE *) check_malloc( sizeof( BINARY_FILE ) );
    b->f = f;
    b->start = ftell(f);
    b->section_start = 0L;
    b->section_version = 0L;
    /* Try to find a version section, and if we do, use it to define the
       version of the file.  If we don't find one the section version will
       remain as 0. */
    b->bf_version = 1;
    b->sigchar = 0;
    find_section(b,VERSION_SECTION);
    b->bf_version = b->section_version;
    return b;
}


void end_section( BINARY_FILE *b )
{
    long end;
    if( b->section_start )
    {
        fseek( b->f, 0L, SEEK_END );
        fwrite( ENDSECTION, strlen(ENDSECTION)+1, 1, b->f );
        end = ftell( b->f );
        fseek( b->f, b->section_start, SEEK_SET );
        fwrite( &end, sizeof(end), 1, b->f );
        fseek( b->f, 0L, SEEK_END );
        b->section_start = 0L;
    }
}


void close_binary_file( BINARY_FILE *b )
{
    end_section( b );
    /* Complete the signature of the file if properly closed */
    if( b->sigchar )
    {
        fseek( b->f, 0, SEEK_SET );
        fwrite( &(b->sigchar), sizeof(char), 1, b->f );
    }
    fclose( b->f );
    check_free( b );
}


void create_section( BINARY_FILE *b, char *section )
{
    create_section_ex( b, section, 0L );
}


void create_section_ex( BINARY_FILE *b, char *section, long version )
{
    long end;
    end = 0L;
    end_section( b );
    fseek( b->f, 0L, SEEK_END );
    b->section_start = ftell( b->f );
    b->section_version = 0;
    fwrite( &end, sizeof(end), 1, b->f );
    fwrite( section, strlen(section)+1, 1, b->f );
    if( b->bf_version > 0 )
    {
        fwrite( &version, sizeof(version), 1, b->f );
        b->section_version = version;
    }
    return;
}


int find_section( BINARY_FILE *b, char *section )
{
    long next;
    char *match;
    int nch;
    int sts;

    nch = strlen(section)+1;
    match = (char *) check_malloc( nch );

    next = b->start;

    sts = MISSING_DATA;

    while( next )
    {
        fseek( b->f, next, SEEK_SET );
        if( fread( &next, sizeof(next), 1, b->f ) != 1 ) break;
        if( fread( match, nch, 1, b->f ) != 1 ) break;
        if( strcmp( match, section ) == 0 )
        {
            /* b->section_start = next; This cannot be added without modifying
                                        end_section, as it results in writing an
                                        end of section when a file has been opened
                                        for reading only */
            b->section_version = 0;
            if( b->bf_version > 0 )
            {
                fread( &(b->section_version), sizeof(b->section_version), 1, b->f );
            }
            sts = OK;

            break;
        }
    }

    check_free( match );
    return sts;
}


int check_end_section( BINARY_FILE *bin )
{
    char endsec[80];
    if( fread( endsec, strlen(ENDSECTION)+1, 1, bin->f ) != 1 ||
            strcmp(endsec,ENDSECTION) != 0 ) return INVALID_DATA;
    return OK;
}







