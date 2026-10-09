#include "snapconfig.h"
#include <algorithm>
#include <string>
#include <vector>
#include <boost/numeric/conversion/cast.hpp>
#include "util/fieldscanner.hpp"
#include "util/filelist.h"

using boost::numeric_cast;

struct recfilename
{
    std::string filename;
    std::string filetype;
};

static int recording=0;
static std::vector<recfilename> filenames;

int set_record_filenames( int record )
{
    const int wasrecording=recording;
    recording=record;
    return wasrecording;
}

int record_filename( std::string_view filename, std::string_view filetype )
{
    if( ! recording ) return NO_FILENAME_ID;
    const auto known=std::find_if( filenames.begin(), filenames.end(),
        [filename]( const recfilename &rec ){ return rec.filename == filename; } );
    if( known != filenames.end() ) return numeric_cast<int>( known - filenames.begin() );

    // A file type is stored with the spelling it was first recorded with
    const auto sametype=std::find_if( filenames.begin(), filenames.end(),
        [filetype]( const recfilename &rec ){ return compare_ignoring_case( filetype, rec.filetype ) == 0; } );
    const std::string type( sametype != filenames.end() ? sametype->filetype : std::string( filetype ) );
    filenames.push_back( { std::string( filename ), type } );
    return numeric_cast<int>( filenames.size() ) - 1;
}

int recorded_filename_count()
{
    return numeric_cast<int>( filenames.size() );
}

bool recorded_filename( int i, std::string &filename, std::string &filetype )
{
    if( i < 0 || i >= recorded_filename_count() ) return false;
    const recfilename &rec=filenames[i];
    filename=rec.filename;
    filetype=rec.filetype;
    return true;
}

void delete_recorded_filenames()
{
    filenames.clear();
}
