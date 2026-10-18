#include "deformation/ggrxf_date.h"

#include <optional>
#include <variant>

#include "util/datetimeparser.h"
#include "util/dateutil.h"
#include "util/rangedint.h"

namespace deformation {

namespace {

constexpr double kSecondsPerMinute = 60.0;
constexpr double kSecondsPerHour = 3600.0;
constexpr double kSecondsPerDay = 86400.0;

}  // namespace

double ggrxfDateToDayNumber(const std::string& dateString) {
    const datetimeparser::DateTimeParts parts = datetimeparser::parseIso8601(dateString);

    // The date part of an ISO 8601 string that parseIso8601 reads is always yyyy-mm-dd, so the day is a month and
    // day, never a day of the year.
    const auto& monthAndDay = std::get<datetimeparser::MonthAndDay>(parts.day);
    const double secondsIntoDay = parts.hour.value * kSecondsPerHour + parts.minute.value * kSecondsPerMinute +
                                  parts.second.value + parts.fractionOfSecond;

    // A time with an offset is converted to UTC. A time with none is local time in a zone that the text does not give,
    // so it is taken as written.
    const double offsetSeconds = parts.offset ? parts.offset->value * kSecondsPerMinute : 0.0;

    return snap_date(parts.year.value, monthAndDay.month.value, monthAndDay.day.value) +
           (secondsIntoDay - offsetSeconds) / kSecondsPerDay;
}

double ggrxfDaysBetween(const std::string& startDate, const std::string& endDate) {
    return ggrxfDateToDayNumber(endDate) - ggrxfDateToDayNumber(startDate);
}

}  // namespace deformation
