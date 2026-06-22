#include "CalendarLayout.h"
#include "test_framework.h"

#include <cmath>
#include <vector>

using namespace xtodo_test;

namespace {

float Mid(float a, float b) { return (a + b) * 0.5f; }

GuiCalendar::TimeRange TR(int start, int end) { return GuiCalendar::TimeRange{ start, end }; }

void ModeControlSegmentsAreEqualContiguousAndHitTestable() {
    const GuiCalendar::ModeControl mc = GuiCalendar::ComputeModeControl(320.0f, 1.0f);
    EXPECT_TRUE(mc.day.left < mc.week.left);
    EXPECT_TRUE(mc.week.left < mc.month.left);
    EXPECT_TRUE(mc.day.right <= mc.week.left + 0.5f);
    EXPECT_TRUE(mc.week.right <= mc.month.left + 0.5f);
    EXPECT_TRUE(std::fabs(mc.day.Width() - mc.week.Width()) < 0.5f);
    EXPECT_TRUE(std::fabs(mc.week.Width() - mc.month.Width()) < 0.5f);

    EXPECT_TRUE(GuiCalendar::HitTestModeControl(Mid(mc.day.left, mc.day.right),
                                                Mid(mc.day.top, mc.day.bottom), mc) ==
                GuiCalendar::ModeHit::Day);
    EXPECT_TRUE(GuiCalendar::HitTestModeControl(Mid(mc.week.left, mc.week.right),
                                                Mid(mc.week.top, mc.week.bottom), mc) ==
                GuiCalendar::ModeHit::Week);
    EXPECT_TRUE(GuiCalendar::HitTestModeControl(Mid(mc.month.left, mc.month.right),
                                                Mid(mc.month.top, mc.month.bottom), mc) ==
                GuiCalendar::ModeHit::Month);
    EXPECT_TRUE(GuiCalendar::HitTestModeControl(mc.day.left - 5.0f, mc.day.top - 80.0f, mc) ==
                GuiCalendar::ModeHit::None);
}

void WeekFrameHasSevenEqualContiguousColumns() {
    const GuiCalendar::WeekFrame f = GuiCalendar::ComputeWeekFrame(700.0f, 600.0f, 1.0f);
    EXPECT_TRUE(f.hourHeight > 0.0f);
    EXPECT_NEAR(f.contentHeight, 24.0f * f.hourHeight, 0.01);
    for (int i = 0; i < 7; ++i) {
        EXPECT_TRUE(f.columns[(size_t)i].right > f.columns[(size_t)i].left);
        if (i > 0) {
            EXPECT_TRUE(std::fabs(f.columns[(size_t)i].left - f.columns[(size_t)(i - 1)].right) < 0.5f);
        }
        EXPECT_TRUE(std::fabs(f.columns[(size_t)i].Width() - f.columns[0].Width()) < 1.0f);
        EXPECT_TRUE(std::fabs(f.dayHeaders[(size_t)i].left - f.columns[(size_t)i].left) < 0.5f);
        EXPECT_TRUE(std::fabs(f.dayHeaders[(size_t)i].right - f.columns[(size_t)i].right) < 0.5f);
    }
    EXPECT_TRUE(f.columns[0].left >= f.gutter.right - 0.5f);
}

void LanePackingIsDeterministic() {
    using GuiCalendar::PackDayLanes;

    const std::vector<GuiCalendar::LaneSpan> a = PackDayLanes({ TR(540, 600), TR(600, 660) });
    EXPECT_EQ((int)a.size(), 2);
    EXPECT_EQ(a[0].lane, 0);
    EXPECT_EQ(a[0].laneCount, 1);
    EXPECT_EQ(a[1].lane, 0);
    EXPECT_EQ(a[1].laneCount, 1);

    const std::vector<GuiCalendar::LaneSpan> b = PackDayLanes({ TR(540, 600), TR(570, 630) });
    EXPECT_EQ(b[0].lane, 0);
    EXPECT_EQ(b[1].lane, 1);
    EXPECT_EQ(b[0].laneCount, 2);
    EXPECT_EQ(b[1].laneCount, 2);

    const std::vector<GuiCalendar::LaneSpan> c =
        PackDayLanes({ TR(540, 600), TR(555, 615), TR(585, 660) });
    EXPECT_EQ(c[0].lane, 0);
    EXPECT_EQ(c[1].lane, 1);
    EXPECT_EQ(c[2].lane, 2);
    EXPECT_EQ(c[0].laneCount, 3);
    EXPECT_EQ(c[2].laneCount, 3);

    const std::vector<GuiCalendar::LaneSpan> d =
        PackDayLanes({ TR(540, 660), TR(570, 600), TR(660, 720) });
    EXPECT_EQ(d[0].laneCount, 2);
    EXPECT_EQ(d[1].laneCount, 2);
    EXPECT_EQ(d[2].lane, 0);
    EXPECT_EQ(d[2].laneCount, 1);
}

void WeekBlockRectsMapDayAndLane() {
    const GuiCalendar::WeekFrame f = GuiCalendar::ComputeWeekFrame(700.0f, 600.0f, 1.0f);
    const GuiCalendar::LaneSpan single{ 0, 1 };
    const Gui::Rect d0 = GuiCalendar::ComputeWeekBlockRect(f, 0, single, 540, 600);
    const Gui::Rect d3 = GuiCalendar::ComputeWeekBlockRect(f, 3, single, 540, 600);
    EXPECT_NEAR(d0.top, d3.top, 0.01);
    EXPECT_NEAR(d0.bottom, d3.bottom, 0.01);
    EXPECT_TRUE(d3.left > d0.left);
    EXPECT_TRUE(d0.left >= f.columns[0].left - 0.01f);
    EXPECT_TRUE(d0.right <= f.columns[0].right + 0.01f);

    const Gui::Rect a = GuiCalendar::ComputeWeekBlockRect(f, 2, GuiCalendar::LaneSpan{ 0, 2 }, 540, 600);
    const Gui::Rect b = GuiCalendar::ComputeWeekBlockRect(f, 2, GuiCalendar::LaneSpan{ 1, 2 }, 540, 600);
    EXPECT_TRUE(b.left > a.left);
    EXPECT_TRUE(a.right <= b.left + 0.5f);
}

void WeekHitTestRoutesHeaderColumnsAndBlocks() {
    const GuiCalendar::WeekFrame f = GuiCalendar::ComputeWeekFrame(700.0f, 600.0f, 1.0f);
    const std::vector<GuiCalendar::WeekBlockRect> none;

    EXPECT_TRUE(GuiCalendar::HitTestWeek(Mid(f.prev.left, f.prev.right), Mid(f.prev.top, f.prev.bottom),
                                         0.0f, 1.0f, f, none).kind == GuiCalendar::WeekHitKind::Prev);
    EXPECT_TRUE(GuiCalendar::HitTestWeek(Mid(f.today.left, f.today.right), Mid(f.today.top, f.today.bottom),
                                         0.0f, 1.0f, f, none).kind == GuiCalendar::WeekHitKind::Today);
    EXPECT_TRUE(GuiCalendar::HitTestWeek(Mid(f.mode.week.left, f.mode.week.right),
                                         Mid(f.mode.week.top, f.mode.week.bottom), 0.0f, 1.0f, f, none)
                    .kind == GuiCalendar::WeekHitKind::ModeWeek);

    const GuiCalendar::WeekHitResult header =
        GuiCalendar::HitTestWeek(Mid(f.dayHeaders[3].left, f.dayHeaders[3].right),
                                 Mid(f.dayHeaders[3].top, f.dayHeaders[3].bottom), 0.0f, 1.0f, f, none);
    EXPECT_TRUE(header.kind == GuiCalendar::WeekHitKind::DayHeader);
    EXPECT_EQ(header.dayIndex, 3);

    // A block in column 2 at content y for 1:00, well inside the viewport at scroll 0.
    const Gui::Rect rect = GuiCalendar::ComputeWeekBlockRect(f, 2, GuiCalendar::LaneSpan{ 0, 1 }, 60, 120);
    const std::vector<GuiCalendar::WeekBlockRect> blocks = { GuiCalendar::WeekBlockRect{ 7, 2, rect } };
    const float localY = f.timelineViewport.top + Mid(rect.top, rect.bottom);
    const GuiCalendar::WeekHitResult bh =
        GuiCalendar::HitTestWeek(Mid(rect.left, rect.right), localY, 0.0f, 1.0f, f, blocks);
    EXPECT_TRUE(bh.kind == GuiCalendar::WeekHitKind::Block);
    EXPECT_EQ(bh.blockId, 7);

    const GuiCalendar::WeekHitResult empty =
        GuiCalendar::HitTestWeek(Mid(f.columns[5].left, f.columns[5].right),
                                 f.timelineViewport.top + 4.0f, 0.0f, 1.0f, f, none);
    EXPECT_TRUE(empty.kind == GuiCalendar::WeekHitKind::EmptyColumn);
    EXPECT_EQ(empty.dayIndex, 5);
}

void MonthFrameHasFortyTwoCellGrid() {
    const GuiCalendar::MonthFrame f = GuiCalendar::ComputeMonthFrame(700.0f, 600.0f, 1.0f);
    EXPECT_TRUE(f.cellWidth > 0.0f);
    EXPECT_TRUE(f.cellHeight > 0.0f);
    EXPECT_EQ((int)f.cells.size(), 42);

    // Row 1 cell 0 sits directly below row 0 cell 0.
    EXPECT_NEAR(f.cells[7].left, f.cells[0].left, 0.01);
    EXPECT_NEAR(f.cells[7].top, f.cells[0].bottom, 0.01);
    // Columns within a row are contiguous and equal width.
    for (int c = 1; c < 7; ++c) {
        EXPECT_TRUE(std::fabs(f.cells[(size_t)c].left - f.cells[(size_t)(c - 1)].right) < 0.5f);
        EXPECT_TRUE(std::fabs(f.cells[(size_t)c].Width() - f.cells[0].Width()) < 1.0f);
        EXPECT_TRUE(std::fabs(f.weekdayHeaders[(size_t)c].left - f.cells[(size_t)c].left) < 0.5f);
    }
}

void MonthHitTestReturnsCellIndexAndHeader() {
    const GuiCalendar::MonthFrame f = GuiCalendar::ComputeMonthFrame(700.0f, 600.0f, 1.0f);
    EXPECT_TRUE(GuiCalendar::HitTestMonth(Mid(f.next.left, f.next.right), Mid(f.next.top, f.next.bottom),
                                          1.0f, f).kind == GuiCalendar::MonthHitKind::Next);
    EXPECT_TRUE(GuiCalendar::HitTestMonth(Mid(f.mode.month.left, f.mode.month.right),
                                          Mid(f.mode.month.top, f.mode.month.bottom), 1.0f, f)
                    .kind == GuiCalendar::MonthHitKind::ModeMonth);

    const GuiCalendar::MonthHitResult first =
        GuiCalendar::HitTestMonth(Mid(f.cells[0].left, f.cells[0].right),
                                  Mid(f.cells[0].top, f.cells[0].bottom), 1.0f, f);
    EXPECT_TRUE(first.kind == GuiCalendar::MonthHitKind::Cell);
    EXPECT_EQ(first.cellIndex, 0);

    const GuiCalendar::MonthHitResult last =
        GuiCalendar::HitTestMonth(Mid(f.cells[41].left, f.cells[41].right),
                                  Mid(f.cells[41].top, f.cells[41].bottom), 1.0f, f);
    EXPECT_TRUE(last.kind == GuiCalendar::MonthHitKind::Cell);
    EXPECT_EQ(last.cellIndex, 41);
}

const TestCase kTests[] = {
    {"ModeControlSegmentsAreEqualContiguousAndHitTestable", ModeControlSegmentsAreEqualContiguousAndHitTestable},
    {"WeekFrameHasSevenEqualContiguousColumns", WeekFrameHasSevenEqualContiguousColumns},
    {"LanePackingIsDeterministic", LanePackingIsDeterministic},
    {"WeekBlockRectsMapDayAndLane", WeekBlockRectsMapDayAndLane},
    {"WeekHitTestRoutesHeaderColumnsAndBlocks", WeekHitTestRoutesHeaderColumnsAndBlocks},
    {"MonthFrameHasFortyTwoCellGrid", MonthFrameHasFortyTwoCellGrid},
    {"MonthHitTestReturnsCellIndexAndHeader", MonthHitTestReturnsCellIndexAndHeader},
};

} // namespace

int main() {
    return RunTests("calendar_layout", kTests);
}
