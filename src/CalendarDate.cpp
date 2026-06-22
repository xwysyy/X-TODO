#include "CalendarDate.h"

#include <cstdio>

namespace CalendarDate {
namespace {

// Howard Hinnant's civil<->days algorithms (proleptic Gregorian, epoch
// 1970-01-01). Pure integer math with no timezone dependence.
long long DaysFromCivil(int y, int m, int d) {
    y -= m <= 2;
    const long long era = (y >= 0 ? y : y - 399) / 400;
    const int yoe = static_cast<int>(y - era * 400);
    const int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

Date CivilFromDays(long long z) {
    z += 719468;
    const long long era = (z >= 0 ? z : z - 146096) / 146097;
    const int doe = static_cast<int>(z - era * 146097);
    const int yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const int y = yoe + static_cast<int>(era) * 400;
    const int doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const int mp = (5 * doy + 2) / 153;
    const int d = doy - (153 * mp + 2) / 5 + 1;
    const int m = mp + (mp < 10 ? 3 : -9);
    Date out;
    out.year = y + (m <= 2 ? 1 : 0);
    out.month = m;
    out.day = d;
    return out;
}

} // namespace

bool IsLeapYear(int year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

int DaysInMonth(int year, int month) {
    static const int kDays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12) return 0;
    if (month == 2 && IsLeapYear(year)) return 29;
    return kDays[month - 1];
}

bool IsValid(const Date& d) {
    if (d.month < 1 || d.month > 12) return false;
    return d.day >= 1 && d.day <= DaysInMonth(d.year, d.month);
}

bool Parse(const std::string& key, Date& out) {
    if (key.size() != 10) return false;
    for (int i = 0; i < 10; ++i) {
        if (i == 4 || i == 7) {
            if (key[(size_t)i] != '-') return false;
            continue;
        }
        if (key[(size_t)i] < '0' || key[(size_t)i] > '9') return false;
    }
    Date d;
    d.year = (key[0] - '0') * 1000 + (key[1] - '0') * 100 + (key[2] - '0') * 10 + (key[3] - '0');
    d.month = (key[5] - '0') * 10 + (key[6] - '0');
    d.day = (key[8] - '0') * 10 + (key[9] - '0');
    if (!IsValid(d)) return false;
    out = d;
    return true;
}

std::string Format(const Date& d) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", d.year, d.month, d.day);
    return std::string(buf);
}

Date AddDays(const Date& d, int delta) {
    return CivilFromDays(DaysFromCivil(d.year, d.month, d.day) + delta);
}

Date AddMonths(const Date& d, int delta) {
    const long long monthIndex = static_cast<long long>(d.year) * 12 + (d.month - 1) + delta;
    int year = static_cast<int>(monthIndex / 12);
    int month0 = static_cast<int>(monthIndex % 12);
    if (month0 < 0) {
        month0 += 12;
        year -= 1;
    }
    const int month = month0 + 1;
    int day = d.day;
    const int maxDay = DaysInMonth(year, month);
    if (day > maxDay) day = maxDay;
    Date out;
    out.year = year;
    out.month = month;
    out.day = day;
    return out;
}

int WeekdayMondayBased(const Date& d) {
    const long long z = DaysFromCivil(d.year, d.month, d.day);
    const int sunday = static_cast<int>(((z + 4) % 7 + 7) % 7); // 0=Sunday
    return (sunday + 6) % 7;                                     // 0=Monday
}

Date StartOfWeek(const Date& d) {
    return AddDays(d, -WeekdayMondayBased(d));
}

Date MonthGridStart(int year, int month) {
    Date first;
    first.year = year;
    first.month = month;
    first.day = 1;
    return StartOfWeek(first);
}

} // namespace CalendarDate
