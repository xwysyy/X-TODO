#include "CalendarDate.h"
#include "test_framework.h"

#include <algorithm>
#include <set>
#include <string>

using namespace xtodo_test;

namespace {

using CalendarDate::Date;

Date ParseOk(const std::string& key) {
  Date d;
  EXPECT_TRUE(CalendarDate::Parse(key, d));
  return d;
}

void ExpectDateEq(const Date& actual, const Date& expected) {
  EXPECT_EQ(actual.year, expected.year);
  EXPECT_EQ(actual.month, expected.month);
  EXPECT_EQ(actual.day, expected.day);
}

void LeapYearAndMonthLengthRulesHoldAcrossFullGregorianCycle() {
  for (int year = 1600; year < 2000; ++year) {
    const bool expectedLeap = (year % 4 == 0) && ((year % 100 != 0) || (year % 400 == 0));
    EXPECT_EQ(CalendarDate::IsLeapYear(year), expectedLeap);
    EXPECT_EQ(CalendarDate::DaysInMonth(year, 1), 31);
    EXPECT_EQ(CalendarDate::DaysInMonth(year, 2), expectedLeap ? 29 : 28);
    EXPECT_EQ(CalendarDate::DaysInMonth(year, 3), 31);
    EXPECT_EQ(CalendarDate::DaysInMonth(year, 4), 30);
    EXPECT_EQ(CalendarDate::DaysInMonth(year, 5), 31);
    EXPECT_EQ(CalendarDate::DaysInMonth(year, 6), 30);
    EXPECT_EQ(CalendarDate::DaysInMonth(year, 7), 31);
    EXPECT_EQ(CalendarDate::DaysInMonth(year, 8), 31);
    EXPECT_EQ(CalendarDate::DaysInMonth(year, 9), 30);
    EXPECT_EQ(CalendarDate::DaysInMonth(year, 10), 31);
    EXPECT_EQ(CalendarDate::DaysInMonth(year, 11), 30);
    EXPECT_EQ(CalendarDate::DaysInMonth(year, 12), 31);
  }
}

void ParseFormatRoundTripsEveryValidDayAcrossFullGregorianCycle() {
  for (int year = 1600; year < 2000; ++year) {
    for (int month = 1; month <= 12; ++month) {
      const int days = CalendarDate::DaysInMonth(year, month);
      EXPECT_TRUE(days >= 28);
      EXPECT_TRUE(days <= 31);
      for (int day = 1; day <= days; ++day) {
        Date d{year, month, day};
        Date parsed;
        const std::string key = CalendarDate::Format(d);
        EXPECT_TRUE(CalendarDate::Parse(key, parsed));
        ExpectDateEq(parsed, d);
        EXPECT_EQ(CalendarDate::Format(parsed), key);
      }
      Date invalidLow{year, month, 0};
      Date invalidHigh{year, month, days + 1};
      EXPECT_FALSE(CalendarDate::IsValid(invalidLow));
      EXPECT_FALSE(CalendarDate::IsValid(invalidHigh));
      EXPECT_FALSE(CalendarDate::Parse(std::to_string(year) + "-" + (month < 10 ? "0" : "") + std::to_string(month) + "-00", invalidLow));
    }
  }
}

void AddDaysIsReversibleAcrossLeapCenturyBoundaries() {
  const Date anchors[] = {
      ParseOk("1899-12-31"),
      ParseOk("1900-02-28"), // 1900 is not leap.
      ParseOk("1999-12-31"),
      ParseOk("2000-02-29"), // 2000 is leap.
      ParseOk("2024-02-29"),
      ParseOk("2100-02-28"), // 2100 is not leap.
  };

  for (const Date& anchor : anchors) {
    for (int delta = -800; delta <= 800; delta += 37) {
      const Date shifted = CalendarDate::AddDays(anchor, delta);
      EXPECT_TRUE(CalendarDate::IsValid(shifted));
      ExpectDateEq(CalendarDate::AddDays(shifted, -delta), anchor);
      const Date weekStart = CalendarDate::StartOfWeek(shifted);
      EXPECT_EQ(CalendarDate::WeekdayMondayBased(weekStart), 0);
      const long long daysFromWeekStart = (CalendarDate::WeekdayMondayBased(shifted) + 7) % 7;
      ExpectDateEq(CalendarDate::AddDays(weekStart, static_cast<int>(daysFromWeekStart)), shifted);
    }
  }
}

void AddMonthsClampsToTargetMonthForEverySourceMonth() {
  for (int year = 2023; year <= 2025; ++year) {
    for (int month = 1; month <= 12; ++month) {
      const int sourceDays = CalendarDate::DaysInMonth(year, month);
      Date source{year, month, sourceDays};
      for (int delta = -24; delta <= 24; ++delta) {
        const Date shifted = CalendarDate::AddMonths(source, delta);
        EXPECT_TRUE(CalendarDate::IsValid(shifted));
        const long long sourceIndex = static_cast<long long>(year) * 12 + (month - 1) + delta;
        int expectedYear = static_cast<int>(sourceIndex / 12);
        int expectedMonth0 = static_cast<int>(sourceIndex % 12);
        if (expectedMonth0 < 0) {
          expectedMonth0 += 12;
          --expectedYear;
        }
        const int expectedMonth = expectedMonth0 + 1;
        const int expectedDay = std::min(sourceDays, CalendarDate::DaysInMonth(expectedYear, expectedMonth));
        ExpectDateEq(shifted, Date{expectedYear, expectedMonth, expectedDay});
      }
    }
  }
}

void MonthGridAlwaysStartsMondayAndCoversTheWholeMonthInFortyTwoCells() {
  for (int year = 2024; year <= 2027; ++year) {
    for (int month = 1; month <= 12; ++month) {
      const Date start = CalendarDate::MonthGridStart(year, month);
      EXPECT_EQ(CalendarDate::WeekdayMondayBased(start), 0);
      std::set<int> daysSeen;
      for (int cell = 0; cell < 42; ++cell) {
        const Date d = CalendarDate::AddDays(start, cell);
        EXPECT_TRUE(CalendarDate::IsValid(d));
        if (d.year == year && d.month == month) daysSeen.insert(d.day);
      }
      EXPECT_EQ(daysSeen.size(), static_cast<size_t>(CalendarDate::DaysInMonth(year, month)));
      EXPECT_TRUE(daysSeen.count(1) == 1);
      EXPECT_TRUE(daysSeen.count(CalendarDate::DaysInMonth(year, month)) == 1);
    }
  }
}

void StrictParserRejectsAlmostCorrectMalformedKeysWithoutChangingOutput() {
  Date out = ParseOk("2026-06-22");
  const Date before = out;
  const std::string bad[] = {
      "2026-6-22", "2026-06-2", "2026/06/22", "2026-06-22 ", " 2026-06-22", "2026-02-29", "2024-02-30", "999-12-31"};
  for (const std::string& key : bad) {
    EXPECT_FALSE(CalendarDate::Parse(key, out));
    ExpectDateEq(out, before);
  }
}

const TestCase kTests[] = {
    {"LeapYearAndMonthLengthRulesHoldAcrossFullGregorianCycle", LeapYearAndMonthLengthRulesHoldAcrossFullGregorianCycle},
    {"ParseFormatRoundTripsEveryValidDayAcrossFullGregorianCycle", ParseFormatRoundTripsEveryValidDayAcrossFullGregorianCycle},
    {"AddDaysIsReversibleAcrossLeapCenturyBoundaries", AddDaysIsReversibleAcrossLeapCenturyBoundaries},
    {"AddMonthsClampsToTargetMonthForEverySourceMonth", AddMonthsClampsToTargetMonthForEverySourceMonth},
    {"MonthGridAlwaysStartsMondayAndCoversTheWholeMonthInFortyTwoCells", MonthGridAlwaysStartsMondayAndCoversTheWholeMonthInFortyTwoCells},
    {"StrictParserRejectsAlmostCorrectMalformedKeysWithoutChangingOutput", StrictParserRejectsAlmostCorrectMalformedKeysWithoutChangingOutput},
};

} // namespace

int main() { return RunTests("calendar_date_property", kTests); }
