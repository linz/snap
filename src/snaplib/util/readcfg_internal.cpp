#include "snapconfig.h"

#include "util/readcfg_internal.h"
#include "util/snapctype.h"

void filter_line( std::string &line, const CommentRule &comment )
{
    std::optional<char> cmnt = comment.comment_char;
    bool iscmt = false;
    std::size_t writePos = 0;

    for( char c : line )
    {
        if( cmnt && c == *cmnt ) iscmt = true;

        if( !iscmt && c != '\r' && c != '\x1A' )
        {
            line[writePos++] = ISSPACE(c) ? ' ' : c;
        }

        if( comment.whole_line_only ) cmnt = std::nullopt;
    }
    line.resize( writePos );
}

int cap_line( std::string &line, const std::size_t max_len )
{
    if( line.size() <= max_len ) return 0;

    std::size_t i = max_len;
    while( i < line.size() && ISSPACE(line[i]) ) ++i;
    const int overrun = static_cast<int>( line.size() - i );

    line.resize( max_len );
    return overrun;
}
