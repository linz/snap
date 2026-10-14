#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <iostream>
#include <string>
#include <string_view>
#include "snapconfig.h"
#include "dbl4_utl_error.h"
#include "dbl4_utl_yield.h"
#include "dbl4_utl_progress.h"
#include "dbl4_utl_blob.h"
#include "util/errdef.h"
#include "dbl4/snap_dbl4_interface.h"
#include "dbl4_types.h"
#include "mi.h"

static int lastpercent = 0;
static std::string laststate;
static int tracing = -1;

StatusType utlCheckAbort() {
    return STS_OK;
}

StatusType utlShowProgress(const std::string_view state, int percent) {
    /*
        if( strcmp(state,laststate) != 0 || percent != lastpercent ) {
           printf("%s",state);
           if( percent >= 0 && percent <= 100 ) {
              printf(": %d%%\n",percent);
              }
           else {
              printf("\n");
              }
           fflush(stdout);
           }
    */
    if (state != laststate) {
        std::printf("%s\n", std::string(state).c_str());
    }
    laststate = state;
    lastpercent = percent;

    return utlCheckAbort();
}

void utlAbort(const std::string_view message) {
    std::cerr << message << '\n';
    std::exit(1);
}

// #pragma warning (disable : 4100)

void start_trace(const char* c, long l) {
    if (tracing < 0) {
        tracing = std::getenv("XATRACE") ? 1 : 0;
    }
    if (tracing) {
        std::printf("Trace %s %ld: ", c, l);
    }
}

void add_trace(const char* fmt, ...) {
    if (tracing) {
        std::va_list argptr;
        va_start(argptr, fmt);
        std::vprintf(fmt, argptr);
        va_end(argptr);
    }
}

void end_trace(void) {
    if (tracing) {
        std::printf("\n");
    }
}

StatusType utlReadBlobDB(void* blob, long lngOffset, long lngBufSize, void* pvBuffer) {
    std::FILE* f;

    f = (std::FILE*)blob;
    if (std::fseek(f, lngOffset, SEEK_SET) != 0)
        RETURN_STATUS(STS_INVALID_DATA);
    if (std::fread(pvBuffer, lngBufSize, 1, f) != 1)
        RETURN_STATUS(STS_INVALID_DATA);
    return STS_OK;
}

StatusType utlWriteBlobDB(void* blob, long lngBufSize, void* pvBuffer) {
    if (!std::fwrite(pvBuffer, (std::size_t)lngBufSize, 1, (std::FILE*)blob))
        RETURN_STATUS(STS_INVALID_DATA);
    return STS_OK;
}

StatusType utlSeekBlobDB(void* blob, long lngOffset, int whence) {
    int w;
    switch (whence) {
        case BLOB_SEEK_CUR:
            w = SEEK_CUR;
            break;
        case BLOB_SEEK_END:
            w = SEEK_END;
            break;
        default:
            w = SEEK_SET;
            break;
    }
    if (std::fseek((std::FILE*)blob, lngOffset, w) != 0)
        RETURN_STATUS(STS_INVALID_DATA);
    return STS_OK;
}

StatusType utlTellBlobDB(void* blob, long* lngOffset) {
    (*lngOffset) = (long)std::ftell((std::FILE*)blob);
    return STS_OK;
}

void utlReleaseBlobDB(void* blob) {
    std::fclose((std::FILE*)blob);
}

int utlCreateReadonlyFileBlob(const std::string& filename, hBlob* blob) {
    std::FILE* f;
    StatusType sts;
    *blob = nullptr;
    f = std::fopen(filename.c_str(), "rb");
    if (!f) {
        return FILE_OPEN_ERROR;
    }
    sts = utlCreateBlobHandle(nullptr, blob);
    if (*blob) {
        (*blob)->pvBlob = f;
    }
    return sts;
}
