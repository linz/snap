#include "snapconfig.h"

#include <algorithm>
#include <fstream>
#include <string>

#include "util/fileutil.h"
#include "util/getversion.h"

inline constexpr std::size_t MAX_VERSION_LENGTH = 20;
inline constexpr std::size_t MAX_VERSION_ID_LENGTH = 40;

/// Reads the first whitespace delimited word of a file, truncated to maxLength characters.
/// Returns an empty string if the file does not exist or has no text.
static std::string read_first_word( const std::string &filename, const std::size_t maxLength )
{
    std::string word;
    if( path_exists(filename) )
    {
        std::ifstream file(filename);
        file >> word;
    }
    word.resize(std::min(word.size(),maxLength));
    return word;
}

std::string getProgramName()
{
    return image_name();
}

const std::string &getProgramVersion( const char *const version )
{
    static std::string programVersion;
    if( ! programVersion.empty() ) return programVersion;

    programVersion=read_first_word(build_filespec(image_dir(),"VERSION",""),MAX_VERSION_LENGTH);
    if( programVersion.empty() )
    {
        programVersion=std::string(version).substr(0,MAX_VERSION_LENGTH);
    }
    const std::string versionId=read_first_word(build_filespec(image_dir(),"VERSIONID",""),MAX_VERSION_ID_LENGTH);
    if( ! versionId.empty() )
    {
        programVersion += "-"+versionId;
    }
    return programVersion;
}
