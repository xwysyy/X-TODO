#include "CalendarLayout.h"

#include "CalendarModel.h"
#include "Theme.h"

#include <cmath>
#include <cwchar>

namespace GuiCalendar {
namespace {

float S(float value, float dpiScale) {
    return value * dpiScale;
}

int RoundToInt(float value) {
    return static_cast<int>(value >= 0.0f ? value + 0.5f : value - 0.5f);
}

int Digit(wchar_t ch) {
    return (ch >= L'0' && ch <= L'9') ? static_cast<int>(ch - L'0') : -1;
}

} // namespace

Frame ComputeFrame(float windowWidth, float viewportHeight, float dpiScale) {
    const float pad = S(Theme::kPadX, dpiScale);
    const float headerH = S(40.0f, dpiScale);
    const float timelineTop = headerH + S(8.0f, dpiScale);
    const float gutterW = S(54.0f, dpiScale);
    const float hourH = S(56.0f, dpiScale);
    const float navSize = S(26.0f, dpiScale);
    const float navTop = (headerH - navSize) * 0.5f;
    const float todayW = S(46.0f, dpiScale);
    const float gap = S(6.0f, dpiScale);

    Frame out;
    out.dateHeader = Gui::Rect{ pad, 0.0f, windowWidth - pad, headerH };
    out.prevDay = Gui::Rect{ pad, navTop, pad + navSize, navTop + navSize };
    out.nextDay = Gui::Rect{ windowWidth - pad - navSize, navTop,
                             windowWidth - pad, navTop + navSize };
    out.today = Gui::Rect{ out.nextDay.left - gap - todayW, navTop,
                           out.nextDay.left - gap, navTop + navSize };

    float timelineBottom = viewportHeight;
    if (timelineBottom < timelineTop) timelineBottom = timelineTop;
    out.timelineViewport = Gui::Rect{ 0.0f, timelineTop, windowWidth, timelineBottom };
    out.gutter = Gui::Rect{ 0.0f, timelineTop, gutterW, timelineBottom };
    out.lane = Gui::Rect{ gutterW, timelineTop, windowWidth - pad, timelineBottom };
    out.hourHeight = hourH;
    out.contentHeight = 24.0f * hourH;
    return out;
}

Gui::Rect ComputeBlockRect(const Frame& frame, int blockId, int startMinute, int endMinute) {
    (void)blockId;
    const float y1 = (static_cast<float>(ClampCalendarMinute(startMinute)) / 60.0f) * frame.hourHeight;
    const float y2 = (static_cast<float>(ClampCalendarMinute(endMinute)) / 60.0f) * frame.hourHeight;
    float bottom = y2;
    const float minHeight = frame.hourHeight * (34.0f / 56.0f);
    if (bottom < y1 + minHeight) bottom = y1 + minHeight;
    return Gui::Rect{ frame.lane.left + 4.0f, y1 + 2.0f, frame.lane.right - 4.0f, bottom - 2.0f };
}

HitResult HitTest(float x, float y, float scroll, float dpiScale, const Frame& frame,
                  const std::vector<BlockRect>& blocks) {
    if (frame.prevDay.Contains(x, y)) return HitResult{ HitKind::PrevDay, -1 };
    if (frame.nextDay.Contains(x, y)) return HitResult{ HitKind::NextDay, -1 };
    if (frame.today.Contains(x, y)) return HitResult{ HitKind::Today, -1 };
    if (!frame.timelineViewport.Contains(x, y)) return {};

    const float docY = y - frame.timelineViewport.top + scroll;
    const float handle = S(7.0f, dpiScale);
    for (const BlockRect& block : blocks) {
        const Gui::Rect& r = block.rect;
        if (x < r.left || x >= r.right || docY < r.top || docY >= r.bottom) continue;
        if (docY < r.top + handle) return HitResult{ HitKind::ResizeStart, block.blockId };
        if (docY >= r.bottom - handle) return HitResult{ HitKind::ResizeEnd, block.blockId };
        return HitResult{ HitKind::BlockBody, block.blockId };
    }

    if (frame.lane.Contains(x, y)) return HitResult{ HitKind::EmptyTimeline, -1 };
    return {};
}

EditLayout ComputeEditLayout(const Gui::Rect& blockRect, float dpiScale) {
    EditLayout out;
    out.block = blockRect;

    const float minH = S(76.0f, dpiScale);
    if (out.block.bottom < out.block.top + minH) out.block.bottom = out.block.top + minH;

    const float padX = S(8.0f, dpiScale);
    const float padTop = S(7.0f, dpiScale);
    const float frameInset = S(4.0f, dpiScale);
    const float titleH = S(26.0f, dpiScale);
    const float timeH = S(26.0f, dpiScale);
    const float gapY = S(8.0f, dpiScale);
    const float gapX = S(10.0f, dpiScale);
    const float minTimeW = S(44.0f, dpiScale);
    const float preferredTimeW = S(64.0f, dpiScale);

    const float contentLeft = out.block.left + padX;
    const float contentRight = out.block.right - padX;
    const float contentW = contentRight - contentLeft;
    float timeW = preferredTimeW;
    if (contentW < timeW * 2.0f + gapX) timeW = (contentW - gapX) * 0.5f;
    if (timeW < minTimeW) timeW = minTimeW;

    out.titleFrame = Gui::Rect{
        contentLeft,
        out.block.top + padTop,
        contentRight,
        out.block.top + padTop + titleH
    };
    out.startFrame = Gui::Rect{
        contentLeft,
        out.titleFrame.bottom + gapY,
        contentLeft + timeW,
        out.titleFrame.bottom + gapY + timeH
    };
    out.endFrame = Gui::Rect{
        out.startFrame.right + gapX,
        out.startFrame.top,
        out.startFrame.right + gapX + timeW,
        out.startFrame.bottom
    };

    auto editRect = [&](const Gui::Rect& frame) {
        return Gui::Rect{
            frame.left + frameInset,
            frame.top + frameInset,
            frame.right - frameInset,
            frame.bottom - frameInset
        };
    };
    out.titleEdit = editRect(out.titleFrame);
    out.startEdit = editRect(out.startFrame);
    out.endEdit = editRect(out.endFrame);
    return out;
}

EditField HitTestEditField(float x, float y, const EditLayout& layout) {
    if (layout.startFrame.Contains(x, y)) return EditField::StartTime;
    if (layout.endFrame.Contains(x, y)) return EditField::EndTime;
    if (layout.titleFrame.Contains(x, y)) return EditField::Title;
    if (layout.block.Contains(x, y)) {
        if (y >= layout.titleFrame.bottom) {
            const float split = (layout.startFrame.right + layout.endFrame.left) * 0.5f;
            return x < split ? EditField::StartTime : EditField::EndTime;
        }
        return EditField::Title;
    }
    return EditField::None;
}

bool EditFieldFrame(const EditLayout& layout, EditField field, Gui::Rect& frame) {
    switch (field) {
    case EditField::Title:
        frame = layout.titleFrame;
        return true;
    case EditField::StartTime:
        frame = layout.startFrame;
        return true;
    case EditField::EndTime:
        frame = layout.endFrame;
        return true;
    case EditField::None:
        break;
    }
    return false;
}

bool EditFieldControlRect(const EditLayout& layout, EditField field, Gui::Rect& rect) {
    switch (field) {
    case EditField::Title:
        rect = layout.titleEdit;
        return true;
    case EditField::StartTime:
        rect = layout.startEdit;
        return true;
    case EditField::EndTime:
        rect = layout.endEdit;
        return true;
    case EditField::None:
        break;
    }
    return false;
}

int MinuteFromPoint(float y, float scroll, const Frame& frame) {
    const float docY = y - frame.timelineViewport.top + scroll;
    if (frame.hourHeight <= 0.0f) return 0;
    const float minute = (docY / frame.hourHeight) * 60.0f;
    return ClampCalendarMinute(RoundToInt(minute));
}

int SnapMinute(int minute, int stepMinutes) {
    if (stepMinutes <= 0) stepMinutes = 15;
    minute = ClampCalendarMinute(minute);
    const int snapped = ((minute + stepMinutes / 2) / stepMinutes) * stepMinutes;
    return ClampCalendarMinute(snapped);
}

bool DragExceeded(float startX, float startY, float x, float y, float dpiScale) {
    const float threshold = S(8.0f, dpiScale);
    const float dx = x - startX;
    const float dy = y - startY;
    return dx * dx + dy * dy >= threshold * threshold;
}

TimeRange RangeFromDrag(int anchorMinute, int currentMinute, int minDuration) {
    if (minDuration <= 0) minDuration = 15;
    int a = SnapMinute(anchorMinute);
    int b = SnapMinute(currentMinute);
    if (a == b) b = a + minDuration;
    int start = a < b ? a : b;
    int end = a < b ? b : a;
    if (end - start < minDuration) end = start + minDuration;
    if (end > 1440) {
        end = 1440;
        start = end - minDuration;
    }
    if (start < 0) start = 0;
    return TimeRange{ start, end };
}

float ScrollForMinute(int minute, float viewportHeight, const Frame& frame) {
    minute = ClampCalendarMinute(minute);
    const float timelineH = frame.timelineViewport.Height();
    const float visible = viewportHeight < timelineH ? viewportHeight : timelineH;
    float scroll = (static_cast<float>(minute) / 60.0f) * frame.hourHeight - visible * 0.35f;
    if (scroll < 0.0f) scroll = 0.0f;
    const float maxScroll = frame.contentHeight - timelineH;
    if (maxScroll > 0.0f && scroll > maxScroll) scroll = maxScroll;
    return scroll;
}

bool ParseTimeText(const std::wstring& text, int& minute) {
    std::wstring s;
    for (wchar_t ch : text) {
        if (ch != L' ' && ch != L'\t' && ch != L'\r' && ch != L'\n') s.push_back(ch);
    }
    const size_t colon = s.find(L':');
    if (colon == std::wstring::npos || colon == 0 || colon > 2 || s.size() - colon - 1 != 2)
        return false;

    int hour = 0;
    for (size_t i = 0; i < colon; ++i) {
        const int d = Digit(s[i]);
        if (d < 0) return false;
        hour = hour * 10 + d;
    }
    const int m1 = Digit(s[colon + 1]);
    const int m2 = Digit(s[colon + 2]);
    if (m1 < 0 || m2 < 0) return false;
    const int mins = m1 * 10 + m2;
    if (hour < 0 || hour > 24 || mins < 0 || mins > 59) return false;
    if (hour == 24 && mins != 0) return false;
    minute = hour * 60 + mins;
    return true;
}

bool ParseTimeRangeText(const std::wstring& startText, const std::wstring& endText,
                        TimeRange& range) {
    int start = 0;
    int end = 0;
    if (!ParseTimeText(startText, start)) return false;
    if (!ParseTimeText(endText, end)) return false;
    if (start < 0 || end > 1440 || end <= start) return false;
    range = TimeRange{ start, end };
    return true;
}

std::wstring FormatTimeText(int minute) {
    minute = ClampCalendarMinute(minute);
    wchar_t buf[8];
#ifdef _WIN32
    swprintf_s(buf, L"%02d:%02d", minute / 60, minute % 60);
#else
    std::swprintf(buf, sizeof(buf) / sizeof(buf[0]), L"%02d:%02d", minute / 60, minute % 60);
#endif
    return std::wstring(buf);
}

namespace {

// Header band geometry shared by day, week, and month frames.
struct HeaderBand {
    float pad = 0.0f;
    float headerH = 0.0f;
    Gui::Rect header;
    Gui::Rect prev;
    Gui::Rect next;
    Gui::Rect today;
};

HeaderBand ComputeHeaderBand(float windowWidth, float dpiScale) {
    const float pad = S(Theme::kPadX, dpiScale);
    const float headerH = S(40.0f, dpiScale);
    const float navSize = S(26.0f, dpiScale);
    const float navTop = (headerH - navSize) * 0.5f;
    const float todayW = S(46.0f, dpiScale);
    const float gap = S(6.0f, dpiScale);

    HeaderBand b;
    b.pad = pad;
    b.headerH = headerH;
    b.header = Gui::Rect{ pad, 0.0f, windowWidth - pad, headerH };
    b.prev = Gui::Rect{ pad, navTop, pad + navSize, navTop + navSize };
    b.next = Gui::Rect{ windowWidth - pad - navSize, navTop, windowWidth - pad, navTop + navSize };
    b.today = Gui::Rect{ b.next.left - gap - todayW, navTop, b.next.left - gap, navTop + navSize };
    return b;
}

} // namespace

ModeControl ComputeModeControl(float windowWidth, float dpiScale) {
    const float headerH = S(40.0f, dpiScale);
    const float segW = S(44.0f, dpiScale);
    const float segH = S(24.0f, dpiScale);
    const float top = (headerH - segH) * 0.5f;
    const float left = windowWidth * 0.5f - segW * 1.5f;

    ModeControl mc;
    mc.day = Gui::Rect{ left, top, left + segW, top + segH };
    mc.week = Gui::Rect{ left + segW, top, left + segW * 2.0f, top + segH };
    mc.month = Gui::Rect{ left + segW * 2.0f, top, left + segW * 3.0f, top + segH };
    return mc;
}

ModeHit HitTestModeControl(float x, float y, const ModeControl& mc) {
    if (mc.day.Contains(x, y)) return ModeHit::Day;
    if (mc.week.Contains(x, y)) return ModeHit::Week;
    if (mc.month.Contains(x, y)) return ModeHit::Month;
    return ModeHit::None;
}

WeekFrame ComputeWeekFrame(float windowWidth, float viewportHeight, float dpiScale) {
    const HeaderBand band = ComputeHeaderBand(windowWidth, dpiScale);
    const float gutterW = S(54.0f, dpiScale);
    const float hourH = S(56.0f, dpiScale);
    const float dayHeaderTop = band.headerH + S(8.0f, dpiScale);
    const float dayHeaderH = S(22.0f, dpiScale);
    const float timelineTop = dayHeaderTop + dayHeaderH;

    WeekFrame out;
    out.header = band.header;
    out.prev = band.prev;
    out.next = band.next;
    out.today = band.today;
    out.mode = ComputeModeControl(windowWidth, dpiScale);

    float timelineBottom = viewportHeight;
    if (timelineBottom < timelineTop) timelineBottom = timelineTop;

    out.dayHeaderRow = Gui::Rect{ 0.0f, dayHeaderTop, windowWidth, dayHeaderTop + dayHeaderH };
    out.timelineViewport = Gui::Rect{ 0.0f, timelineTop, windowWidth, timelineBottom };
    out.gutter = Gui::Rect{ 0.0f, timelineTop, gutterW, timelineBottom };

    const float laneLeft = gutterW;
    const float laneRight = windowWidth - band.pad;
    const float colW = (laneRight - laneLeft) / 7.0f;
    for (int i = 0; i < 7; ++i) {
        const float left = laneLeft + colW * static_cast<float>(i);
        const float right = (i == 6) ? laneRight : laneLeft + colW * static_cast<float>(i + 1);
        out.columns[(size_t)i] = Gui::Rect{ left, timelineTop, right, timelineBottom };
        out.dayHeaders[(size_t)i] = Gui::Rect{ left, dayHeaderTop, right, dayHeaderTop + dayHeaderH };
    }

    out.hourHeight = hourH;
    out.contentHeight = 24.0f * hourH;
    return out;
}

std::vector<LaneSpan> PackDayLanes(const std::vector<TimeRange>& sortedSpans) {
    std::vector<LaneSpan> out(sortedSpans.size());
    std::vector<int> laneEnd; // end minute of the last block in each lane of the cluster
    size_t clusterStart = 0;
    int clusterMaxEnd = -1;

    auto closeCluster = [&](size_t endIndex) {
        const int count = laneEnd.empty() ? 1 : static_cast<int>(laneEnd.size());
        for (size_t k = clusterStart; k < endIndex; ++k) out[k].laneCount = count;
    };

    for (size_t i = 0; i < sortedSpans.size(); ++i) {
        const TimeRange& span = sortedSpans[i];
        if (clusterMaxEnd >= 0 && span.startMinute >= clusterMaxEnd) {
            closeCluster(i);
            laneEnd.clear();
            clusterStart = i;
            clusterMaxEnd = -1;
        }
        int lane = -1;
        for (size_t l = 0; l < laneEnd.size(); ++l) {
            if (laneEnd[l] <= span.startMinute) { lane = static_cast<int>(l); break; }
        }
        if (lane < 0) {
            lane = static_cast<int>(laneEnd.size());
            laneEnd.push_back(span.endMinute);
        } else {
            laneEnd[(size_t)lane] = span.endMinute;
        }
        out[i].lane = lane;
        if (span.endMinute > clusterMaxEnd) clusterMaxEnd = span.endMinute;
    }
    closeCluster(sortedSpans.size());
    return out;
}

Gui::Rect ComputeWeekBlockRect(const WeekFrame& frame, int dayIndex, const LaneSpan& lane,
                               int startMinute, int endMinute) {
    if (dayIndex < 0) dayIndex = 0;
    if (dayIndex > 6) dayIndex = 6;
    const Gui::Rect& col = frame.columns[(size_t)dayIndex];

    const float y1 = (static_cast<float>(ClampCalendarMinute(startMinute)) / 60.0f) * frame.hourHeight;
    const float y2 = (static_cast<float>(ClampCalendarMinute(endMinute)) / 60.0f) * frame.hourHeight;
    float bottom = y2;
    const float minHeight = frame.hourHeight * (34.0f / 56.0f);
    if (bottom < y1 + minHeight) bottom = y1 + minHeight;

    const int laneCount = lane.laneCount > 0 ? lane.laneCount : 1;
    const float w = col.Width() / static_cast<float>(laneCount);
    const float left = col.left + w * static_cast<float>(lane.lane);
    return Gui::Rect{ left + 1.0f, y1 + 1.0f, left + w - 1.0f, bottom - 1.0f };
}

WeekHitResult HitTestWeek(float x, float y, float scroll, float dpiScale,
                          const WeekFrame& frame, const std::vector<WeekBlockRect>& blocks) {
    (void)dpiScale;
    if (frame.prev.Contains(x, y)) return WeekHitResult{ WeekHitKind::Prev, -1, -1 };
    if (frame.next.Contains(x, y)) return WeekHitResult{ WeekHitKind::Next, -1, -1 };
    if (frame.today.Contains(x, y)) return WeekHitResult{ WeekHitKind::Today, -1, -1 };
    switch (HitTestModeControl(x, y, frame.mode)) {
    case ModeHit::Day:   return WeekHitResult{ WeekHitKind::ModeDay, -1, -1 };
    case ModeHit::Week:  return WeekHitResult{ WeekHitKind::ModeWeek, -1, -1 };
    case ModeHit::Month: return WeekHitResult{ WeekHitKind::ModeMonth, -1, -1 };
    case ModeHit::None:  break;
    }
    for (int i = 0; i < 7; ++i) {
        if (frame.dayHeaders[(size_t)i].Contains(x, y)) return WeekHitResult{ WeekHitKind::DayHeader, i, -1 };
    }
    if (!frame.timelineViewport.Contains(x, y)) return WeekHitResult{};

    const float docY = y - frame.timelineViewport.top + scroll;
    for (const WeekBlockRect& block : blocks) {
        const Gui::Rect& r = block.rect;
        if (x < r.left || x >= r.right || docY < r.top || docY >= r.bottom) continue;
        return WeekHitResult{ WeekHitKind::Block, block.dayIndex, block.blockId };
    }
    for (int i = 0; i < 7; ++i) {
        if (x >= frame.columns[(size_t)i].left && x < frame.columns[(size_t)i].right) {
            return WeekHitResult{ WeekHitKind::EmptyColumn, i, -1 };
        }
    }
    return WeekHitResult{};
}

MonthFrame ComputeMonthFrame(float windowWidth, float viewportHeight, float dpiScale) {
    const HeaderBand band = ComputeHeaderBand(windowWidth, dpiScale);
    const float weekdayTop = band.headerH + S(8.0f, dpiScale);
    const float weekdayH = S(20.0f, dpiScale);
    const float gridTop = weekdayTop + weekdayH;

    MonthFrame out;
    out.header = band.header;
    out.prev = band.prev;
    out.next = band.next;
    out.today = band.today;
    out.mode = ComputeModeControl(windowWidth, dpiScale);

    float gridBottom = viewportHeight;
    if (gridBottom < gridTop) gridBottom = gridTop;
    const float gridLeft = band.pad;
    const float gridRight = windowWidth - band.pad;

    out.weekdayRow = Gui::Rect{ gridLeft, weekdayTop, gridRight, weekdayTop + weekdayH };
    out.grid = Gui::Rect{ gridLeft, gridTop, gridRight, gridBottom };

    const float cellW = (gridRight - gridLeft) / 7.0f;
    const float cellH = (gridBottom - gridTop) / 6.0f;
    out.cellWidth = cellW;
    out.cellHeight = cellH;

    for (int c = 0; c < 7; ++c) {
        const float left = gridLeft + cellW * static_cast<float>(c);
        const float right = (c == 6) ? gridRight : gridLeft + cellW * static_cast<float>(c + 1);
        out.weekdayHeaders[(size_t)c] = Gui::Rect{ left, weekdayTop, right, weekdayTop + weekdayH };
    }
    for (int r = 0; r < 6; ++r) {
        for (int c = 0; c < 7; ++c) {
            const float left = gridLeft + cellW * static_cast<float>(c);
            const float right = (c == 6) ? gridRight : gridLeft + cellW * static_cast<float>(c + 1);
            const float top = gridTop + cellH * static_cast<float>(r);
            const float bottom = (r == 5) ? gridBottom : gridTop + cellH * static_cast<float>(r + 1);
            out.cells[(size_t)(r * 7 + c)] = Gui::Rect{ left, top, right, bottom };
        }
    }
    return out;
}

MonthHitResult HitTestMonth(float x, float y, float dpiScale, const MonthFrame& frame) {
    (void)dpiScale;
    if (frame.prev.Contains(x, y)) return MonthHitResult{ MonthHitKind::Prev, -1 };
    if (frame.next.Contains(x, y)) return MonthHitResult{ MonthHitKind::Next, -1 };
    if (frame.today.Contains(x, y)) return MonthHitResult{ MonthHitKind::Today, -1 };
    switch (HitTestModeControl(x, y, frame.mode)) {
    case ModeHit::Day:   return MonthHitResult{ MonthHitKind::ModeDay, -1 };
    case ModeHit::Week:  return MonthHitResult{ MonthHitKind::ModeWeek, -1 };
    case ModeHit::Month: return MonthHitResult{ MonthHitKind::ModeMonth, -1 };
    case ModeHit::None:  break;
    }
    for (int i = 0; i < 42; ++i) {
        if (frame.cells[(size_t)i].Contains(x, y)) return MonthHitResult{ MonthHitKind::Cell, i };
    }
    return MonthHitResult{};
}

} // namespace GuiCalendar
