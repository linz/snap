// #pragma once
#ifndef _UTIL_PARSEANGLE_HPP
#define _UTIL_PARSEANGLE_HPP

#include <stdexcept>
#include <string>

namespace LINZ
{
    namespace ParseAngle
    {

        // Parse angles in DMS format.

        class ParseAngleException : public std::runtime_error
        {
        public:
            ParseAngleException(const std::string &message) : std::runtime_error(message) {}
        };

        enum AngleFormat
        {
            AF_DEGREES,
            AF_DMS,
            AF_HPFORMAT
        };

        AngleFormat parseAngleFormat(const std::string &format);
        double parseAngle(AngleFormat format, const std::string &value, const std::string &sign = "");

    } // namespace ParseAngle

} // End of namespace LINZ

#endif
