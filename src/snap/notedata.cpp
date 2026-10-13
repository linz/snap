#include "snapconfig.h"
/*
   $Log: notedata.c,v $
   Revision 1.2  1999/06/14 09:20:31  ccrook
   Fixed minor bug with corrupting retrieval of comments > 80 characters long.

   Revision 1.1  1996/01/03 22:00:48  CHRIS
   Initial revision

*/

#include <cstdio>
#include <cstring>
#include <boost/numeric/conversion/cast.hpp>
using boost::numeric_cast;

#include "output.h"
#include "snapdata/survdata.h"
#include "snap/bindata.h"
#include "notedata.h"

/*===================================================*/

/* Routines to handle notes */
/* If the note is continued, the first character is set to blank,
   otherwise it is a line-feed character */

std::int64_t save_note(std::string_view text, int continued) {
    const long size = numeric_cast<long>(text.size()) + 3;

    const std::int64_t loc = write_bindata_header(size, NOTEDATA);

    std::fputc(continued ? ' ' : '\n', bindata_file);
    std::fwrite(text.data(), text.size(), 1, bindata_file);
    std::fputc('\n', bindata_file);
    std::fputc(0, bindata_file);

    return loc;
}

void list_note(std::FILE* out, std::int64_t loc) {
    char note[81];
    long size;
    int block;
    int type;
    int firstline;
    int c;
    if (loc < 0 || !output_notes) {
        return;
    }
    const std::int64_t curloc = ftell64(bindata_file);
    fseek64(bindata_file, loc, SEEK_SET);

    firstline = 1;
    while (read_bindata_header(&size, &type) && type == NOTEDATA) {
        c = std::fgetc(bindata_file);
        if (firstline) {
            std::fputc(c, out);
            firstline = 0;
        } else {
            if (c != ' ') {
                break;
            }
        }
        size--;
        note[80] = 0;
        while (size) {
            block = size > 80 ? 80 : size;
            std::fread(note, 1, block, bindata_file);
            std::fputs(note, out);
            size -= block;
        }
    }

    fseek64(bindata_file, curloc, SEEK_SET);
}
