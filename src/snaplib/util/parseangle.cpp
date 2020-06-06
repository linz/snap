#include "snapconfig.hpp"

#include "util/parseangle.hpp"
#include "util/snapregex.hpp"
#include <boost/algorithm/string.hpp>
#include <exception>

using namespace std;
using namespace LINZ;
using namespace ParseAngle;

static RGX::regex dms_regex("^\\s*([-nsNSewEW]?)\\s*((?:3[0-5]|[0-2]?\\d?)\\d)\\s+([0-5]?\\d)\\s+([0-5]?\\d)(?:\\.(\\d*))?\\s*([nsNSewEW]?)\\s*$");
static RGX::regex hp_regex("^\\s*(\\-?)((?:3[0-5]|[0-2]?\\d?)\\d)\\.([0-5]\\d)([0-5]\\d)(\\d*)()\\s*$");

ParseAngle::AngleFormat ParseAngle::parseAngleFormat(const string &formatdef)
{
    AngleFormat format = AF_DEGREES;

    if (boost::iequals(formatdef, "deg_angles") || boost::iequals(formatdef, "degrees") || boost::iequals(formatdef, "deg"))
    {
        format = AF_DEGREES;
    }
    else if (boost::iequals(formatdef, "dms_angles") || boost::iequals(formatdef, "dms"))
    {
        format = AF_DMS;
    }
    else if (boost::iequals(formatdef, "hp_angles") || boost::iequals(formatdef, "hp"))
    {
        format = AF_HPFORMAT;
    }
    else
    {
        throw ParseAngleException(string("Invalid angle format ") + formatdef);
    }
    return format;
}

double ParseAngle::parseAngle(AngleFormat format, const string &value, const string &sign)
{
    double angle = 0.0;
    RGX::smatch match;
    string tvalue = value;
    if (format == AF_DEGREES)
    {
        angle = atof(value.c_str());
        return angle;
    }
    if (format == AF_HPFORMAT)
    {
        if (tvalue.find('.') == string::npos) tvalue += '.';
        tvalue += "0000";
    }
    const RGX::regex &formatre = (format == AF_HPFORMAT) ? hp_regex : dms_regex;

    if (!RGX::regex_match(tvalue, match, formatre))
    {
        throw ParseAngleException(string("Invalid angle degrees/minutes/seconds: ") + value);
    }
    else
    {
        int deg = atoi(match[2].str().c_str());
        int min = atoi(match[3].str().c_str());
        double sec = atof((match[4].str() + "." + match[5].str()).c_str());
        angle = deg + min / 60.0 + sec / 3600.0;
        string hem = match[1].str() + match[6].str();
        if (hem.length() > 0)
        {
            bool ok = false;
            if (sign.length() == 2 && hem.length() == 1)
            {
                ok = true;
                boost::to_upper(hem);
                if (hem[0] == '-' || hem[0] == sign[1])
                {
                    angle = -angle;
                }
                else if (hem[0] != sign[0])
                {
                    ok = false;
                }
            }
            if (!ok)
            {
                throw ParseAngleException(string("Badly formatted angle: " + value));
            }
        }
    }
    return angle;
}
