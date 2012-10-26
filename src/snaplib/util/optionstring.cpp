#include <string>
#include <map>
#include <regex>
#include <boost/algorithm/string.hpp>

#include <optionstring.hpp>

namespace LINZ
{

OptionString::OptionString( const std::string &options )
{
    _load(options);
}

void OptionString::_load( const std::string &options )
{
    _options.clear();
    _invalid.clear();

    std::regex re(
        "\\s*"   // Optional initial whitespace
        "(?:"
        "\\-([a-z_]\\w*)"  // negated option
        "|"
        "([a-z_]\\w+)\\=" // start of option
        "(?:"
        "([^\\s\\\"]+)"                       // unquoted value
        "|"
        "(?:\\\"((?:[^\\\"]|\\\"\\\")*)\\\")" // or quoted value
        ")"                 // end of option, space or end of string
        "|"
        "(\\S+)"              // Catchall for invalid data
        ")(?:\\s|$)",          // Terminator
        std::regex_constants::icase
    );
    std::sregex_iterator match(options.begin(), options.end(), re);
    std::sregex_iterator end;
    for( ; match != end; match++ )
    {
        bool valid = true;
        std::string key;
        std::string value;

        // Negated option
        if( (*match)[1].length() > 0 )
        {
            key = (*match)[1];
            value = "N";
        }
        // Normal option
        else if( (*match)[2].length() > 0 )
        {
            key = (*match)[2];
            // Unquoted
            if( (*match)[3].length() > 0 )
            {
                value = (*match)[3];
            }
            // Quoted
            else
            {
                value = std::regex_replace( (*match)[4].str(),std::regex("\\\"\\\""),std::string("\""));
            }
        }
        else
        {
            valid = false;
        }
        if( valid )
        {
            boost::to_lower( key );
            valid = _options.find(key) == _options.end();
            if( valid ) _options.insert( std::pair<const std::string, const std::string>(key,value));
        }
        if( ! valid )
        {
            _invalid.push_back( (*match)[0] );
        }
    }
}

bool OptionString::defines( const std::string &option )
{
   std::string key = option;
   boost::to_lower( key );
   return _options.find(key) != _options.end();
}

const std::string &OptionString::valueOf( const std::string &option, const std::string &dflt )
{
    std::string key = option;
    boost::to_lower( key );
    auto match = _options.find(key);
    if( match != _options.end())
    {
        return match->second;
    }
    else
    {
        return dflt;
    }
}

}  // End of namespace LINZ
