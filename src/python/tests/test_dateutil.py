"""Tests lib.dateutil.parse_date/date_to_year."""

from __future__ import annotations

from datetime import datetime

import pytest

from lib.dateutil import date_to_year, parse_date


class TestParseDate:
    """parse_date across the supported date-string forms."""

    def test_date_only(self) -> None:
        """A date with no time component parses with hour/minute/second all 0."""
        assert parse_date("1-jan-2020") == datetime(2020, 1, 1, 0, 0, 0)

    def test_date_and_time(self) -> None:
        """A date with a time component parses every field, including seconds."""
        assert parse_date("15-mar-2023 06:30:45") == datetime(2023, 3, 15, 6, 30, 45)

    def test_end_of_year_with_time(self) -> None:
        """A late-December date at 23:59:59 parses correctly, with no year rollover."""
        assert parse_date("31-dec-2021 23:59:59") == datetime(2021, 12, 31, 23, 59, 59)

    def test_month_name_is_case_insensitive(self) -> None:
        """The month abbreviation matches regardless of case."""
        assert parse_date("15-MAR-2023 06:30:45") == parse_date("15-mar-2023 06:30:45")
        assert parse_date("15-Mar-2023") == parse_date("15-mar-2023")

    def test_single_digit_day(self) -> None:
        """A single-digit day (no leading zero) parses correctly."""
        assert parse_date("1-jan-2020") == parse_date("01-jan-2020")

    def test_rejects_invalid_calendar_date(self) -> None:
        """A day that doesn't exist in the given month (29 Feb on a non-leap year) is rejected."""
        with pytest.raises(ValueError, match="day"):
            parse_date("29-feb-2021")

    def test_rejects_unrecognized_month_name(self) -> None:
        """A month abbreviation outside the fixed 12 raises `ValueError`."""
        with pytest.raises(ValueError, match="Invalid date"):
            parse_date("1-xyz-2020")

    def test_rejects_malformed_input(self) -> None:
        """A string with no recognizable date structure raises `ValueError`."""
        with pytest.raises(ValueError, match="Invalid date"):
            parse_date("not a date")


class TestDateToYear:
    """date_to_year's boundary and leap-year behaviour."""

    def test_start_of_year_is_exact(self) -> None:
        """1 January at midnight is exactly the whole year, no fractional part."""
        assert date_to_year("1-jan-2020") == 2020.0
        assert date_to_year("1-jan-2021") == 2021.0

    def test_leap_year_july_first_differs_from_non_leap(self) -> None:
        """1 July lands at a different fraction of a leap year than a non-leap year."""
        leap = date_to_year("1-jul-2020")  # 2020 is a leap year: 182/366 elapsed
        non_leap = date_to_year("1-jul-2021")  # 2021 is not: 181/365 elapsed
        assert leap == pytest.approx(2020 + 182 / 366)
        assert non_leap == pytest.approx(2021 + 181 / 365)
        assert leap != non_leap

    def test_century_leap_year_rules(self) -> None:
        """2000 (divisible by 400) is a leap year; 1900 (divisible by 100, not 400) is not."""
        assert date_to_year("29-feb-2000") == pytest.approx(2000 + 59 / 366)
        with pytest.raises(ValueError, match="day"):
            parse_date("29-feb-1900")

    def test_time_of_day_contributes_a_sub_day_fraction(self) -> None:
        """A time-of-day component adds a fraction smaller than a single day's worth."""
        midnight = date_to_year("2-jan-2021")
        noon = date_to_year("2-jan-2021 12:00:00")
        assert noon > midnight
        assert noon == pytest.approx(midnight + 0.5 / 365)

    def test_end_of_year_approaches_next_year(self) -> None:
        """23:59:59 on 31 December is just short of rolling over to the next year."""
        assert date_to_year("31-dec-2021 23:59:59") < 2022.0
        assert date_to_year("31-dec-2021 23:59:59") == pytest.approx(2022.0, abs=1e-4)
