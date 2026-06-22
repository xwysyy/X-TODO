#pragma once

#include <string>

// Win32-free Gregorian date arithmetic for calendar views. Date math here is
// deterministic and timezone-free; only the active day comes from the platform.
namespace CalendarDate {

struct Date {
    int year = 0;
    int month = 0; // 1..12
    int day = 0;   // 1..days-in-month
};

bool IsLeapYear(int year);
int DaysInMonth(int year, int month);

// A real Gregorian date: month 1..12 and day within that month's length.
bool IsValid(const Date& d);

// Parses "YYYY-MM-DD" and rejects impossible dates such as 2026-02-31.
bool Parse(const std::string& key, Date& out);
std::string Format(const Date& d);

// Offsets by whole days using proleptic Gregorian day math.
Date AddDays(const Date& d, int delta);

// Adds months, clamping the day to the target month's length
// (2026-01-31 plus one month becomes 2026-02-28).
Date AddMonths(const Date& d, int delta);

// Weekday with Monday as 0 through Sunday as 6.
int WeekdayMondayBased(const Date& d);

// The Monday on or before d.
Date StartOfWeek(const Date& d);

// The Monday that begins the 42-cell grid for the given month, that is the
// Monday on or before the first day of the month.
Date MonthGridStart(int year, int month);

} // namespace CalendarDate
