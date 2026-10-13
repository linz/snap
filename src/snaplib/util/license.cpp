#include "snapconfig.h"

/* The header - a random string of bytes.  Used to identify the
   license in the file in order to change its contents. */

/*
   $Log: license.c,v $
   Revision 1.2  2004/04/22 02:35:26  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.1  1995/12/22 19:52:14  CHRIS
   Initial revision

*/

#include "util/license.h"

#define LICENSE_HEADER "Azq\xF3_\xA0*%2=.\xC9"
#define LICENSE_HEADER_LEN 12
#define MAX_LICENSE_TEXT 80

char* decrypted_license(unsigned char* license_text) {
    int i;
    unsigned char ch1;
    unsigned char ch2;
    unsigned char mlt;
    unsigned char prm;
    unsigned char add;
    unsigned char* license = license_text + LICENSE_HEADER_LEN;
    if (license_text[0]) {
        mlt = 0x1B;
        prm = 0xA3;
        add = 0x45;
        for (i = 0; i < MAX_LICENSE_TEXT; i++) {
            prm = prm * mlt + add;
            ch1 = license[MAX_LICENSE_TEXT - i - 1];
            ch2 = license[i];
            ch1 = ch1 ^ prm;
            prm = prm * mlt + add;
            ch2 = ch2 ^ prm;
            license[i] = ch1;
            license[MAX_LICENSE_TEXT - i - 1] = ch2;
        }
    }

    license_text[0] = 0; /* To avoid repeating the decryption */
    return (char*)license;
}

#ifdef SET_LICENSE

#ifdef DEMO
#define LICENSE_TEXT "Demonstration copy only"
#else
#define LICENSE_TEXT "For internal use by DOSLI only"
#endif

static unsigned char license_text[MAX_LICENSE_TEXT + LICENSE_HEADER_LEN + 1];
static unsigned char* license = license_text + LICENSE_HEADER_LEN;

#include <cstdio>
#include "util/snapctype.h"
#include <cstring>
#include <stdlib.h>

int main(int argc, char* argv[]) {
    std::FILE* in;
    int c;
    long loc;
    unsigned char buf[LICENSE_HEADER_LEN];
    unsigned char save;
    in = NULL;

    if (argc < 2 || (_stricmp(argv[1], "-s") && !(in = std::fopen(argv[1], "r+b")))) {
        std::fprintf(stderr, "Missing or invalid file name in LICENSE command\n");
        return 0;
    }

    std::strcpy(license_text, LICENSE_HEADER);
    for (c = 0; c < MAX_LICENSE_TEXT; c++)
        license[c] = c;

    if (in) {
        for (;;) {
            while ((c = std::fgetc(in)) != license_text[0] && c != EOF)
                ;
            if (c == EOF)
                break;
            loc = std::ftell(in);
            if (std::fread(buf, LICENSE_HEADER_LEN - 1, 1, in) == 1 &&
                std::memcmp(buf, license_text + 1, LICENSE_HEADER_LEN - 1) == 0) {
                break;
            }
            std::fseek(in, loc, SEEK_SET);
        }
        if (c == EOF) {
            std::fprintf(stderr, "%s cannot be licensed\n", argv[1]);
            return 0;
        }
    }

    std::fprintf(stderr, "Enter licensee name: ");
    gets((char*)license);
    if (!in)
        std::printf("\n/* License string for: %s */\n", license);
    save = license_text[0];
    decrypted_license(license_text);
    license_text[0] = save;
    if (in) {
        std::fseek(in, loc - 1, SEEK_SET);
        if (std::fwrite(license_text, sizeof(license_text), 1, in) != 1) {
            std::printf("Unable to write license information to %s\n", argv[1]);
            return 0;
        }
        std::fprintf(stderr, "License written to %s\n", argv[1]);
        std::fclose(in);
    } else {
        char inhex = 0;
        int nc = 0;
        int i;
        std::printf("\nstatic unsigned char *license_text = \"");
        for (i = 0; i < MAX_LICENSE_TEXT + LICENSE_HEADER_LEN; i++) {
            c = license_text[i];
            if (nc > 50) {
                std::printf("\"\n                                  \"");
                nc = 0;
            }
            if (ISPRINT(c) && !ISSPACE(c) && (!inhex || !ISXDIGIT(c))) {
                if (c == '\"' || c == '\\') {
                    std::putchar('\\');
                    nc++;
                }
                std::putchar((int)(c));
                nc++;
                inhex = 0;
            } else {
                std::printf("\\x%02X", (int)(c));
                inhex = 1;
                nc += 4;
            }
        }
        std::printf("\";\n");
    }
    return 0;
}

#endif
