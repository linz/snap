#ifndef WILDCARD_H
#define WILDCARD_H

#include <string_view>

bool has_wildcard( std::string_view pattern );
bool wildcard_match( std::string_view pattern, std::string_view s );
bool filename_wildcard_match( std::string_view pattern, std::string_view filename );

#endif
