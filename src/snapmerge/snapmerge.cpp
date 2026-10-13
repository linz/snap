#include "snapconfig.h"
/*
   $Log: $
*/

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include "util/errdef.h"
#include <string>
#include <vector>
#include "network/network.h"
#include "util/fieldscanner.hpp"
#include "util/fileutil.h"
#include "util/dateutil.h"
#include "util/dstring.h"
#include "util/getversion.h"
#include "snap/filenames.h"

std::vector<std::string> codes;
network* base = nullptr;
char listonly = 0;

int select_station(station* st) {
    if (!listonly) {
        return 1;
    }
    for (const std::string& code : codes) {
        if (compare_ignoring_case(st->Code, code) == 0) {
            return 1;
        }
    }
    return 0;
}

int main(int argc, char* argv[]) {
    char* stnlistfile = nullptr;
    char* basefile = nullptr;
    char* datafile = nullptr;
    char* newfile = nullptr;
    network* data;
    int quiet = 0;
    int overwrite = 0;
    int syntaxerror = 0;
    int mergeopt = 0;
    double mergedate = UNDEFINED_DATE;
    int updatecls = 0;
    int updatecrd = 0;
    int addstn = 1;
    int addclass = 1;
    int clearbaseorders = 0;
    int cleardataorders = 0;
    int sts;

    CONFIGURE_RUNTIME();

    while (!syntaxerror && argc > 1 && argv[1][0] == '-') {
        switch (argv[1][1]) {
            case 'q':
            case 'Q':
                quiet = 1;
                break;

            case 'o':
            case 'O':
                switch (argv[1][2]) {
                    case 0:
                        overwrite = 1;
                        break;
                    case 'c':
                    case 'C':
                        updatecrd = 1;
                        break;
                    case 'a':
                    case 'A':
                        updatecls = 1;
                        break;
                    default:
                        syntaxerror = 1;
                        break;
                };
                break;

            case 'u':
            case 'U':
                switch (argv[1][2]) {
                    case 0:
                        addstn = 0;
                        break;
                    case 'a':
                    case 'A':
                        addclass = 0;
                        break;
                    default:
                        syntaxerror = 1;
                        break;
                };
                break;

            case 'c':
            case 'C':
                switch (argv[1][2]) {
                    case 0:
                    case 'b':
                    case 'B':
                        clearbaseorders = 1;
                        break;
                    case 'd':
                    case 'D':
                        cleardataorders = 1;
                        break;
                    default:
                        syntaxerror = 1;
                        break;
                };
                break;

            case 'l':
            case 'L':
                if (argc > 2) {
                    stnlistfile = argv[2];
                    argc--;
                    argv++;
                } else {
                    syntaxerror = 1;
                }
                break;

            case 'y':
            case 'Y':
                if (argc > 2) {
                    if (!parse_crdsys_epoch(argv[2], mergedate)) {
                        syntaxerror = 1;
                    }
                    argc--;
                    argv++;
                } else {
                    syntaxerror = 1;
                }
                break;
            default:
                syntaxerror = 1;
                break;
        }
        argc--;
        argv++;
    }

    if (argc != 3 && argc != 4) {
        syntaxerror = 1;
    }

    if (!quiet) {
        std::printf("\n%s %s: Merges two SNAP coordinate files\n\n", PROGRAM_NAME, PROGRAM_VERSION);
    }

    if (syntaxerror) {
        std::printf("Syntax: snapmerge [options] basefile datafile [newfile]\n\n");
        std::printf("where\n");
        std::printf("  basefile    is the input coordinate file\n");
        std::printf("  datafile    is the coordinate file to merge\n");
        std::printf("  newfile     is the name of the output coordinate file\n");
        std::printf("              (defaults to the same as basefile)\n");
        std::printf("and options are:\n");
        std::printf("  -o          replace existing stations with new versions\n");
        std::printf("  -oc         overwrite coordinates of stations\n");
        std::printf("  -oa         overwrite classification attributes of stations\n");
        std::printf("  -u          update only .. don't add any new stations\n");
        std::printf("  -ua         update classes only .. don't add new classes\n");
        std::printf("  -c or -cb   Remove coordinate orders from basefile\n");
        std::printf("  -cd         Remove coordinate orders from datafile\n");
        std::printf("  -y yyyymmdd Date for coordinate conversion\n");
        std::printf("  -l listfile defines a file with a list of station codes to include\n");
        std::printf("              from data file - only these codes will be added\n");
        std::printf("  -q          quiet mode - minimal output\n");

        return 2;
    }

    basefile = argv[1];
    datafile = argv[2];
    newfile = argc > 3 ? argv[3] : argv[1];

    install_default_crdsys_file();

    base = new_network();
    if (read_network(base, basefile, 0) != OK) {
        std::printf("Cannot open base coordinate file %s\n", basefile);
        return 2;
    }
    if (clearbaseorders && base->orderclsid > 0) {
        const int nstn = number_of_stations(base);
        const int clsid = base->orderclsid;
        for (int istn = 1; istn <= nstn; istn++) {
            station* st = station_ptr(base, istn);
            st->set_class(clsid, 0);
        }
    }

    data = new_network();
    if (read_network(data, datafile, 0) != OK) {
        std::printf("Cannot open data coordinate file %s\n", datafile);
        return 2;
    }
    if (cleardataorders && data->orderclsid > 0) {
        const int nstn = number_of_stations(data);
        const int clsid = data->orderclsid;
        for (int istn = 1; istn <= nstn; istn++) {
            station* st = station_ptr(data, istn);
            st->set_class(clsid, 0);
        }
    }

    if (stnlistfile) {
        char code[81];
        std::FILE* list;
        list = std::fopen(stnlistfile, "r");
        if (!list) {
            std::printf("Cannot open station list file %s\n", stnlistfile);
            return 2;
        }
        skip_utf8_bom(list);
        while (std::fscanf(list, "%80s", code) == 1) {
            codes.emplace_back(code);
        }
        std::fclose(list);
        listonly = 1;
    }

    if (!addstn && !(updatecrd | updatecls)) {
        overwrite = 1;
    }
    mergeopt = 0;
    if (addstn) {
        mergeopt |= NW_MERGEOPT_ADDNEW;
    }
    if (addclass) {
        mergeopt |= NW_MERGEOPT_ADDCLASSES;
    }
    if (overwrite) {
        mergeopt |= NW_MERGEOPT_OVERWRITE;
    }
    if (updatecrd) {
        mergeopt |= NW_MERGEOPT_COORDS;
    }
    if (updatecls) {
        mergeopt |= NW_MERGEOPT_CLASSES;
    }
    sts = merge_network(base, data, mergeopt, mergedate, &select_station);

    if (sts != OK || write_network(base, newfile, std::string_view(), 0, nullptr) != OK) {
        return 2;
    }

    return 0;
}
