#include "CalendarModel.h"
#include "test_framework.h"

#include <string>
#include <vector>

using namespace xtodo_test;

namespace {

void ExpectBlock(const CalendarBlock& block, int id, const std::string& day, int start, int end, const std::wstring& title) {
  EXPECT_EQ(block.id, id);
  EXPECT_EQ(block.day, day);
  EXPECT_EQ(block.startMinute, start);
  EXPECT_EQ(block.endMinute, end);
  EXPECT_EQ(block.title, title);
}

void AddBlockSortsByDayStartEndAndIdAndDayFilteringPreservesOrder() {
  CalendarModel calendar;
  const int late = calendar.AddBlock("2026-06-22", 18 * 60, 19 * 60, L"late");
  const int early = calendar.AddBlock("2026-06-22", 9 * 60, 10 * 60, L"early");
  const int previousDay = calendar.AddBlock("2026-06-21", 23 * 60, 24 * 60, L"previous");
  const int shorter = calendar.AddBlock("2026-06-22", 9 * 60, 9 * 60 + 30, L"shorter");

  EXPECT_TRUE(late > 0);
  EXPECT_TRUE(early > late);
  EXPECT_TRUE(previousDay > early);
  EXPECT_TRUE(shorter > previousDay);

  const auto& all = calendar.Blocks();
  EXPECT_EQ(all.size(), static_cast<size_t>(4));
  EXPECT_EQ(all[0].id, previousDay);
  EXPECT_EQ(all[1].id, shorter);
  EXPECT_EQ(all[2].id, early);
  EXPECT_EQ(all[3].id, late);

  const auto today = calendar.BlocksForDay("2026-06-22");
  EXPECT_EQ(today.size(), static_cast<size_t>(3));
  EXPECT_EQ(today[0]->id, shorter);
  EXPECT_EQ(today[1]->id, early);
  EXPECT_EQ(today[2]->id, late);
}

void RangeNormalizationAlwaysProducesPositiveClampedDuration() {
  CalendarModel calendar;
  const int negative = calendar.AddBlock("2026-06-22", -50, -10, L"negative");
  const int backwardsAtEnd = calendar.AddBlock("2026-06-22", 2000, 10, L"backwards end");
  const int fullDay = calendar.AddBlock("2026-06-22", -20, 2000, L"full day");
  EXPECT_TRUE(negative > 0);
  EXPECT_TRUE(backwardsAtEnd > 0);
  EXPECT_TRUE(fullDay > 0);

  const CalendarBlock* a = calendar.FindBlock(negative);
  const CalendarBlock* b = calendar.FindBlock(backwardsAtEnd);
  const CalendarBlock* c = calendar.FindBlock(fullDay);
  EXPECT_TRUE(a != nullptr);
  EXPECT_TRUE(b != nullptr);
  EXPECT_TRUE(c != nullptr);
  EXPECT_EQ(a->startMinute, 0);
  EXPECT_EQ(a->endMinute, 1);
  EXPECT_EQ(b->startMinute, 1439);
  EXPECT_EQ(b->endMinute, 1440);
  EXPECT_EQ(c->startMinute, 0);
  EXPECT_EQ(c->endMinute, 1440);

  EXPECT_TRUE(calendar.SetBlockRange(negative, 1440, 1440));
  a = calendar.FindBlock(negative);
  EXPECT_TRUE(a != nullptr);
  EXPECT_EQ(a->startMinute, 1439);
  EXPECT_EQ(a->endMinute, 1440);
  EXPECT_FALSE(calendar.SetBlockRange(404, 0, 1));
}

void ReplaceBlocksDropsInvalidDaysDeduplicatesIdsAndSeedsNextId() {
  CalendarModel calendar;
  calendar.ReplaceBlocks({
      CalendarBlock{7, "2026-06-22", 60, 120, L"explicit"},
      CalendarBlock{7, "2026-06-22", 130, 130, L"duplicate id"},
      CalendarBlock{-1, "2026-06-23", 2000, 10, L"generated id"},
      CalendarBlock{99, "2026-13-01", 60, 120, L"bad month"},
      CalendarBlock{100, "2026-06-00", 60, 120, L"bad day"},
  });

  const auto& blocks = calendar.Blocks();
  EXPECT_EQ(blocks.size(), static_cast<size_t>(3));
  ExpectBlock(blocks[0], 7, "2026-06-22", 60, 120, L"explicit");
  EXPECT_TRUE(blocks[1].id != 7);
  EXPECT_EQ(blocks[1].day, std::string("2026-06-22"));
  EXPECT_EQ(blocks[1].startMinute, 130);
  EXPECT_EQ(blocks[1].endMinute, 131);
  EXPECT_EQ(blocks[2].day, std::string("2026-06-23"));
  EXPECT_EQ(blocks[2].startMinute, 1439);
  EXPECT_EQ(blocks[2].endMinute, 1440);

  const int next = calendar.AddBlock("2026-06-24", 60, 120, L"next");
  EXPECT_TRUE(next > blocks[2].id);
  EXPECT_EQ(next, 10);
}

void InvalidDayKeysAreRejectedWithoutChangingExistingBlocks() {
  CalendarModel calendar;
  const int id = calendar.AddBlock("2026-06-22", 60, 120, L"ok");
  EXPECT_TRUE(id > 0);
  const size_t before = calendar.Blocks().size();

  const std::string badDays[] = {
      "", "2026-6-22", "2026/06/22", "2026-00-22", "2026-13-22", "2026-06-00", "2026-06-32", "abcd-ef-gh"};
  for (const std::string& day : badDays) {
    EXPECT_EQ(calendar.AddBlock(day, 60, 120, L"bad"), -1);
    EXPECT_EQ(calendar.Blocks().size(), before);
  }
}

void MutatingTitlesRemovingAndResortingAreReferenceSafe() {
  CalendarModel calendar;
  const int a = calendar.AddBlock("2026-06-22", 600, 660, L"A");
  const int b = calendar.AddBlock("2026-06-22", 480, 540, L"B");
  EXPECT_TRUE(calendar.SetBlockTitle(a, L"A renamed"));
  EXPECT_FALSE(calendar.SetBlockTitle(404, L"missing"));
  EXPECT_TRUE(calendar.SetBlockRange(a, 300, 330));

  const auto& blocks = calendar.Blocks();
  EXPECT_EQ(blocks.size(), static_cast<size_t>(2));
  EXPECT_EQ(blocks[0].id, a);
  EXPECT_EQ(blocks[0].title, std::wstring(L"A renamed"));
  EXPECT_EQ(blocks[1].id, b);

  EXPECT_TRUE(calendar.RemoveBlock(a));
  EXPECT_FALSE(calendar.RemoveBlock(a));
  EXPECT_TRUE(calendar.FindBlock(a) == nullptr);
  EXPECT_TRUE(calendar.FindBlock(b) != nullptr);
}

const TestCase kTests[] = {
    {"AddBlockSortsByDayStartEndAndIdAndDayFilteringPreservesOrder", AddBlockSortsByDayStartEndAndIdAndDayFilteringPreservesOrder},
    {"RangeNormalizationAlwaysProducesPositiveClampedDuration", RangeNormalizationAlwaysProducesPositiveClampedDuration},
    {"ReplaceBlocksDropsInvalidDaysDeduplicatesIdsAndSeedsNextId", ReplaceBlocksDropsInvalidDaysDeduplicatesIdsAndSeedsNextId},
    {"InvalidDayKeysAreRejectedWithoutChangingExistingBlocks", InvalidDayKeysAreRejectedWithoutChangingExistingBlocks},
    {"MutatingTitlesRemovingAndResortingAreReferenceSafe", MutatingTitlesRemovingAndResortingAreReferenceSafe},
};

} // namespace

int main() { return RunTests("calendar_model_property", kTests); }
