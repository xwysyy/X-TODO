#include "CalendarDate.h"
#include "test_framework.h"

#include <string>

using namespace xtodo_test;

namespace {

using CalendarDate::Date;

std::string Fmt(const Date& d) { return CalendarDate::Format(d); }

void ParseRejectsImpossibleDatesAndAcceptsRealOnes() {
    Date d;
    EXPECT_FALSE(CalendarDate::Parse("2026-02-31", d));
    EXPECT_FALSE(CalendarDate::Parse("2026-04-31", d)); // April has 30 days
    EXPECT_FALSE(CalendarDate::Parse("2026-00-10", d));
    EXPECT_FALSE(CalendarDate::Parse("2026-13-01", d));
    EXPECT_FALSE(CalendarDate::Parse("2025-02-29", d)); // non-leap year
    EXPECT_FALSE(CalendarDate::Parse("2026-1-1", d));   // malformed shape
    EXPECT_FALSE(CalendarDate::Parse("", d));
    EXPECT_FALSE(CalendarDate::Parse("hello worl", d));

    EXPECT_TRUE(CalendarDate::Parse("2024-02-29", d)); // leap day
    EXPECT_EQ(Fmt(d), std::string("2024-02-29"));

    EXPECT_TRUE(CalendarDate::Parse("2026-06-22", d));
    EXPECT_EQ(d.year, 2026);
    EXPECT_EQ(d.month, 6);
    EXPECT_EQ(d.day, 22);
    EXPECT_EQ(Fmt(d), std::string("2026-06-22"));
}

void LeapYearAndMonthLengthRules() {
    EXPECT_TRUE(CalendarDate::IsLeapYear(2000));
    EXPECT_FALSE(CalendarDate::IsLeapYear(1900));
    EXPECT_TRUE(CalendarDate::IsLeapYear(2024));
    EXPECT_FALSE(CalendarDate::IsLeapYear(2025));

    EXPECT_EQ(CalendarDate::DaysInMonth(2024, 2), 29);
    EXPECT_EQ(CalendarDate::DaysInMonth(2025, 2), 28);
    EXPECT_EQ(CalendarDate::DaysInMonth(2026, 4), 30);
    EXPECT_EQ(CalendarDate::DaysInMonth(2026, 1), 31);
}

void AddDaysCrossesMonthAndYearBoundaries() {
    Date d;
    EXPECT_TRUE(CalendarDate::Parse("2026-02-28", d));
    EXPECT_EQ(Fmt(CalendarDate::AddDays(d, 1)), std::string("2026-03-01"));

    EXPECT_TRUE(CalendarDate::Parse("2024-02-28", d));
    EXPECT_EQ(Fmt(CalendarDate::AddDays(d, 1)), std::string("2024-02-29"));

    EXPECT_TRUE(CalendarDate::Parse("2025-12-31", d));
    EXPECT_EQ(Fmt(CalendarDate::AddDays(d, 1)), std::string("2026-01-01"));

    EXPECT_TRUE(CalendarDate::Parse("2026-01-01", d));
    EXPECT_EQ(Fmt(CalendarDate::AddDays(d, -1)), std::string("2025-12-31"));
    EXPECT_EQ(Fmt(CalendarDate::AddDays(d, 7)), std::string("2026-01-08"));
}

void AddMonthsClampsDayToTargetMonthLength() {
    Date d;
    EXPECT_TRUE(CalendarDate::Parse("2026-01-31", d));
    EXPECT_EQ(Fmt(CalendarDate::AddMonths(d, 1)), std::string("2026-02-28"));

    EXPECT_TRUE(CalendarDate::Parse("2024-01-31", d));
    EXPECT_EQ(Fmt(CalendarDate::AddMonths(d, 1)), std::string("2024-02-29"));

    EXPECT_TRUE(CalendarDate::Parse("2026-03-31", d));
    EXPECT_EQ(Fmt(CalendarDate::AddMonths(d, -1)), std::string("2026-02-28"));

    EXPECT_TRUE(CalendarDate::Parse("2026-06-15", d));
    EXPECT_EQ(Fmt(CalendarDate::AddMonths(d, 1)), std::string("2026-07-15"));

    EXPECT_TRUE(CalendarDate::Parse("2026-12-15", d));
    EXPECT_EQ(Fmt(CalendarDate::AddMonths(d, 1)), std::string("2027-01-15"));

    EXPECT_TRUE(CalendarDate::Parse("2026-01-15", d));
    EXPECT_EQ(Fmt(CalendarDate::AddMonths(d, -1)), std::string("2025-12-15"));
}

Date ParseOk(const char* key) {
    Date d;
    EXPECT_TRUE(CalendarDate::Parse(key, d));
    return d;
}

void WeekdayUsesMondayAsZero() {
    EXPECT_EQ(CalendarDate::WeekdayMondayBased(ParseOk("2024-01-01")), 0); // Monday
    EXPECT_EQ(CalendarDate::WeekdayMondayBased(ParseOk("2024-01-06")), 5); // Saturday
    EXPECT_EQ(CalendarDate::WeekdayMondayBased(ParseOk("2024-01-07")), 6); // Sunday
    EXPECT_EQ(CalendarDate::WeekdayMondayBased(ParseOk("2026-06-22")), 0); // Monday
}

void StartOfWeekSnapsBackToMonday() {
    EXPECT_EQ(Fmt(CalendarDate::StartOfWeek(ParseOk("2024-01-03"))), std::string("2024-01-01"));
    EXPECT_EQ(Fmt(CalendarDate::StartOfWeek(ParseOk("2024-01-07"))), std::string("2024-01-01"));
    EXPECT_EQ(Fmt(CalendarDate::StartOfWeek(ParseOk("2024-01-08"))), std::string("2024-01-08"));
}

void MonthGridStartIsMondayOnOrBeforeFirstOfMonth() {
    EXPECT_EQ(Fmt(CalendarDate::MonthGridStart(2026, 2)), std::string("2026-01-26"));
    EXPECT_EQ(Fmt(CalendarDate::MonthGridStart(2026, 6)), std::string("2026-06-01"));
}

const TestCase kTests[] = {
    {"ParseRejectsImpossibleDatesAndAcceptsRealOnes", ParseRejectsImpossibleDatesAndAcceptsRealOnes},
    {"LeapYearAndMonthLengthRules", LeapYearAndMonthLengthRules},
    {"AddDaysCrossesMonthAndYearBoundaries", AddDaysCrossesMonthAndYearBoundaries},
    {"AddMonthsClampsDayToTargetMonthLength", AddMonthsClampsDayToTargetMonthLength},
    {"WeekdayUsesMondayAsZero", WeekdayUsesMondayAsZero},
    {"StartOfWeekSnapsBackToMonday", StartOfWeekSnapsBackToMonday},
    {"MonthGridStartIsMondayOnOrBeforeFirstOfMonth", MonthGridStartIsMondayOnOrBeforeFirstOfMonth},
};

} // namespace

int main() {
    return RunTests("calendar_date", kTests);
}
