#include "snapconfig.h"

#include <optional>
#include <string_view>
#include "util/fieldscanner.hpp"
#include "util/wildcard.h"


bool has_wildcard( std::string_view pattern )
{
    return pattern.find_first_of( "*?" ) != std::string_view::npos;
}

/// Matches \p s against \p pattern, where '*' matches any run of characters and
/// '?' matches any one character. Characters listed in \p notwild are not
/// matched by '?' or by a single '*'.
static bool wildcard_match_imp(
    std::string_view pattern,                   ///< the pattern to match
    std::string_view s,                         ///< the text to match against the pattern
    std::optional<std::string_view> notwild )   ///< characters the wildcards may not match
{
    while( ! pattern.empty() && ! s.empty() &&
            pattern.front() != '*' &&
            (  compare_ignoring_case(pattern.substr(0,1),s.substr(0,1)) == 0 ||
               (pattern.front() == '?' && ( ! notwild || notwild->find(s.front()) == std::string_view::npos))
            )
         )
    {
        pattern.remove_prefix(1);
        s.remove_prefix(1);
    }
    if( pattern.empty() && s.empty() ) return true;
    if( ! pattern.empty() && pattern.front() == '*' )
    {
        std::optional<std::string_view> nw = notwild;
        while( true )
        {
            pattern.remove_prefix(1);
            if( pattern.empty() || pattern.front() != '*' ) break;
            nw = std::nullopt;
        }
        while( ! s.empty() )
        {
            if( ! pattern.empty() && wildcard_match_imp(pattern,s,nw) ) return true;
            if( nw && nw->find(s.front()) != std::string_view::npos ) return false;
            s.remove_prefix(1);
        }
        return pattern.empty();
    }
    return false;
}

bool wildcard_match( std::string_view pattern, std::string_view s )
{
    return wildcard_match_imp( pattern, s, std::nullopt );
}

bool filename_wildcard_match( std::string_view pattern, std::string_view filename )
{
    static constexpr std::string_view pathdelim = "/\\";
    std::string_view s = filename;
    while( ! s.empty() )
    {
        if( wildcard_match_imp(pattern,s,pathdelim) ) return true;
        while( ! s.empty() )
        {
            const char ch = s.front();
            s.remove_prefix(1);
            if( pathdelim.find(ch) != std::string_view::npos ) break;
        }
    }
    return false;
}
