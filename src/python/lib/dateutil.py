"""Parses `D-Mon-YYYY[ HH:MM:SS]` date strings, and converts them to decimal years, for LINZ
deformation model files.
"""

from __future__ import annotations

from datetime import datetime

# strptime's %b/%B month directives depend on the process's current locale, so they aren't
# guaranteed to recognize English abbreviations like "mar" - this format is a fixed data
# convention, not user-facing text, and always uses English abbreviations regardless of locale.
# Substituting the month number here lets the rest of the string go through strptime with purely
# numeric directives (%d/%m/%Y/%H/%M/%S), which never consult locale data.
_MONTH_NUMBERS = {
    "jan": 1,
    "feb": 2,
    "mar": 3,
    "apr": 4,
    "may": 5,
    "jun": 6,
    "jul": 7,
    "aug": 8,
    "sep": 9,
    "oct": 10,
    "nov": 11,
    "dec": 12,
}


def parse_date(text: str) -> datetime:
    """Parses `D-Mon-YYYY` or `D-Mon-YYYY HH:MM:SS` (month name case-insensitive), raising
    `ValueError` if `text` doesn't match."""
    parts = text.strip().split("-", 2)
    if len(parts) != 3:
        raise ValueError(f"Invalid date {text!r}")
    day, month_name, rest = parts
    month = _MONTH_NUMBERS.get(month_name.lower())
    if month is None:
        raise ValueError(f"Invalid date {text!r}")
    normalized = f"{day}-{month:02d}-{rest}"
    try:
        return datetime.strptime(normalized, "%d-%m-%Y %H:%M:%S")
    except ValueError:
        return datetime.strptime(normalized, "%d-%m-%Y")


def date_to_year(text: str) -> float:
    """Converts a date string to a decimal year: the calendar year plus the exact fraction of it
    elapsed by that date and time, correctly distinguishing leap from non-leap years."""
    parsed = parse_date(text)
    year_start = datetime(parsed.year, 1, 1)
    next_year_start = datetime(parsed.year + 1, 1, 1)
    elapsed = (parsed - year_start).total_seconds()
    whole_year = (next_year_start - year_start).total_seconds()
    return parsed.year + elapsed / whole_year
