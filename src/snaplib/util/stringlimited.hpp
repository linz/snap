#ifndef _STRINGLIMITED_HPP
#define _STRINGLIMITED_HPP

#include <cstddef>
#include <string>
#include <string_view>

/// A string of at most MaxChars characters.  Constructing or assigning a
/// longer value truncates it.  The base is private so that the other
/// std::string members, which could break the limit, are not available.
template <size_t MaxChars>
class StringLimited : private std::string
{
public:
    StringLimited() = default;

    /// Creates a string, truncating it to MaxChars characters.
    explicit StringLimited( const std::string_view text ) ///< The text
        : std::string( text.substr( 0, MaxChars ) )
    {
    }

    /// Replaces the string, truncating it to MaxChars characters.
    void assign( const std::string_view text ) ///< The new text
    {
        std::string::assign( text.substr( 0, MaxChars ) );
    }

    using std::string::empty;
    using std::string::size;

    /// A copy of the text as a std::string
    std::string str() const
    {
        return *this;
    }

    /// The NUL terminated text, for the printf family and other C interfaces
    using std::string::c_str;

    using std::string::operator std::string_view;
};

#endif
