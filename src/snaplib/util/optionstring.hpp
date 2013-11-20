#pragma once
#ifndef _LINZ_OPTIONSTRING_HPP
#define _LINZ_OPTIONSTRING_HPP

#include <string>
#include <map>
#include <vector>

namespace LINZ
{

// Parses a string formatted as "key=value key=value ..."
// Keys must be alphanumeric (including _) and start with alpha or _
// Values can be quoted to include spaces, quotes may be included by repeating
// Keys are case insensitive

class OptionString
{
public:
    OptionString( const std::string &options );
    bool defines( const std::string &option );
    const std::string &valueOf( const std::string &key, const std::string &dflt="" );
    bool isValid() { return _invalid.size() > 0; }
    const std::vector<const std::string> errors() { return _invalid; }
private:
    void _load( const std::string &options );
    std::map<const std::string, const std::string> _options;
    std::vector<const std::string> _invalid;
};

}  // End of namespace LINZ


#endif
