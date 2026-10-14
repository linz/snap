#include "deformation/ggrxf_date.h"

#include <stdexcept>

#include "util/dateutil.h"

namespace deformation {

double ggrxfDateToDayNumber(const std::string& dateString) {
    const double dayNumber = snap_datetime_parse(dateString.c_str(), nullptr);
    if (dayNumber == 0.0) {
        throw std::invalid_argument("ggrxfDateToDayNumber: not a valid ISO-8601 date/date-time: " + dateString);
    }
    return dayNumber;
}

double ggrxfDaysBetween(const std::string& startDate, const std::string& endDate) {
    return ggrxfDateToDayNumber(endDate) - ggrxfDateToDayNumber(startDate);
}

}  // namespace deformation
