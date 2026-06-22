#include "todo_model_test_utils.h"

#include <algorithm>
#include <random>
#include <sstream>
#include <string>
#include <vector>

using namespace xtodo_test;

namespace {

struct ListSnapshot {
  std::string id;
  std::wstring title;
  bool completedExpanded = false;
  int activeCount = 0;
  std::vector<std::wstring> texts;
  std::vector<int> levels;
  std::vector<bool> dones;
  std::vector<bool> collapsed;
};

ListSnapshot CaptureList(const TodoList& list) {
  ListSnapshot out;
  out.id = list.id;
  out.title = list.title;
  out.completedExpanded = list.completedExpanded;
  out.activeCount = list.activeCount;
  for (const TodoItem& item : list.items) {
    out.texts.push_back(item.text);
    out.levels.push_back(item.level);
    out.dones.push_back(item.done);
    out.collapsed.push_back(item.collapsed);
  }
  return out;
}

std::vector<ListSnapshot> CaptureModel(const TodoModel& model) {
  std::vector<ListSnapshot> out;
  for (const TodoList& list : model.Lists()) out.push_back(CaptureList(list));
  return out;
}

void ExpectSnapshotEqual(const ListSnapshot& actual, const ListSnapshot& expected) {
  EXPECT_EQ(actual.id, expected.id);
  EXPECT_EQ(actual.title, expected.title);
  EXPECT_EQ(actual.completedExpanded, expected.completedExpanded);
  EXPECT_EQ(actual.activeCount, expected.activeCount);
  EXPECT_EQ(actual.texts, expected.texts);
  EXPECT_EQ(actual.levels, expected.levels);
  EXPECT_EQ(actual.dones, expected.dones);
  EXPECT_EQ(actual.collapsed, expected.collapsed);
}

void ExpectModelEqual(const TodoModel& actual, const TodoModel& expected) {
  EXPECT_EQ(actual.ListCount(), expected.ListCount());
  EXPECT_EQ(actual.CurrentListIndex(), expected.CurrentListIndex());
  const auto a = CaptureModel(actual);
  const auto e = CaptureModel(expected);
  EXPECT_EQ(a.size(), e.size());
  for (size_t i = 0; i < a.size(); ++i) ExpectSnapshotEqual(a[i], e[i]);
}

void AssertSubtreeContractsForList(const TodoList& list) {
  const int count = static_cast<int>(list.items.size());
  for (int i = 0; i < count; ++i) {
    const int limit = list.items[static_cast<size_t>(i)].done ? count : list.activeCount;
    const int end = LocalSubtreeEnd(list, i, limit);
    EXPECT_TRUE(end >= i + 1);
    EXPECT_TRUE(end <= limit);
    for (int j = i + 1; j < end; ++j) {
      EXPECT_TRUE(list.items[static_cast<size_t>(j)].level > list.items[static_cast<size_t>(i)].level);
    }
    if (end < limit) {
      EXPECT_TRUE(list.items[static_cast<size_t>(end)].level <= list.items[static_cast<size_t>(i)].level);
    }
    if (list.items[static_cast<size_t>(i)].collapsed) {
      EXPECT_TRUE(end > i + 1);
    }
  }
}

void AssertSubtreeContracts(TodoModel& model) {
  const std::string originalId = model.CurrentList().id;
  for (int listIndex = 0; listIndex < model.ListCount(); ++listIndex) {
    EXPECT_TRUE(model.SetCurrentListIndex(listIndex));
    const TodoList& list = model.CurrentList();
    AssertSubtreeContractsForList(list);
    for (int i = 0; i < model.Count(); ++i) {
      const int limit = model.Items()[static_cast<size_t>(i)].done ? model.Count() : model.ActiveCount();
      const int localEnd = LocalSubtreeEnd(list, i, limit);
      EXPECT_EQ(model.SubtreeEnd(i), localEnd);
      EXPECT_EQ(model.HasChildren(i), localEnd > i + 1);
    }
  }
  EXPECT_TRUE(model.SetCurrentListId(originalId));
}

void AssertModelContracts(TodoModel& model) {
  AssertInvariants(model);
  AssertSubtreeContracts(model);
}

void ReplaceListsIsAnIdempotentNormalizerAcrossAllLists() {
  TodoList inbox;
  inbox.id = "inbox";
  inbox.title = L"Inbox";
  inbox.completedExpanded = true;
  inbox.activeCount = 99; // ignored by ReplaceLists; recomputed from done flags.
  inbox.items = {
      TodoItem{L"done first", true, 99, true},
      TodoItem{L"active root too deep", false, 3, true},
      TodoItem{L"active child too deep", false, 3, true},
      TodoItem{L"active sibling", false, 1, false},
      TodoItem{L"done child", true, 3, true},
  };

  TodoList generated;
  generated.title = L""; // exercises default-title and generated-id paths.
  generated.items = {
      TodoItem{L"leaf collapse should clear", false, 0, true},
      TodoItem{L"completed root", true, 0, false},
      TodoItem{L"completed child", true, 2, true},
  };

  TodoList work;
  work.id = "list-41";
  work.title = L"Work";
  work.items = {
      TodoItem{L"w done", true, 0, false},
      TodoItem{L"w active", false, 2, true},
  };

  TodoModel model;
  model.ReplaceLists({inbox, generated, work}, "list-41");
  AssertModelContracts(model);
  EXPECT_EQ(model.CurrentList().id, std::string("list-41"));
  EXPECT_EQ(model.ListAt(1)->id, std::string("list-1"));
  EXPECT_EQ(model.ListAt(1)->title, std::wstring(L"默认"));

  TodoModel normalizedAgain;
  normalizedAgain.ReplaceLists(model.Lists(), model.CurrentList().id);
  ExpectModelEqual(normalizedAgain, model);
  AssertModelContracts(normalizedAgain);

  normalizedAgain.AddList(L"after import");
  EXPECT_EQ(normalizedAgain.CurrentList().id, std::string("list-42"));
  AssertModelContracts(normalizedAgain);
}

void CompletingNestedAndAncestorBlocksDoesNotMergeCompletedSiblings() {
  TodoModel model;
  model.AddActive(L"A", 0);
  model.AddActive(L"A.1", 1);
  model.AddActive(L"A.1.a", 2);
  model.AddActive(L"A.2", 1);
  model.AddActive(L"B", 0);
  model.AddActive(L"C", 0);
  EXPECT_TRUE(model.ToggleCollapsed(0));

  model.SetDone(1, true);
  ExpectTexts(model, {L"A", L"A.2", L"B", L"C", L"A.1", L"A.1.a"});
  ExpectLevels(model, {0, 1, 0, 0, 0, 1});
  ExpectDones(model, {false, false, false, false, true, true});
  EXPECT_TRUE(model.Items()[0].collapsed); // A still has A.2, so a real parent may stay collapsed.
  AssertModelContracts(model);

  model.SetDone(0, true);
  ExpectTexts(model, {L"B", L"C", L"A", L"A.2", L"A.1", L"A.1.a"});
  ExpectLevels(model, {0, 0, 0, 1, 0, 1});
  ExpectDones(model, {false, false, true, true, true, true});
  AssertModelContracts(model);

  model.SetDone(model.ActiveCount(), false);
  ExpectTexts(model, {L"B", L"C", L"A", L"A.2", L"A.1", L"A.1.a"});
  ExpectDones(model, {false, false, false, false, true, true});
  ExpectLevels(model, {0, 0, 0, 1, 0, 1});
  EXPECT_EQ(model.ActiveCount(), 4);
  AssertModelContracts(model);
}

void NonAdjacentIndentParentCannotBreakLevelContinuity() {
  TodoModel model;
  model.AddActive(L"A", 0);
  model.AddActive(L"A.1", 1);
  model.AddActive(L"A.1.a", 2);
  model.AddActive(L"A.2", 1);
  model.AddActive(L"A.2.a", 2);
  AssertModelContracts(model);

  EXPECT_FALSE(model.IndentItemUnder(4, 2));
  ExpectLevels(model, {0, 1, 2, 1, 2});
  AssertModelContracts(model);

  EXPECT_TRUE(model.IndentItemUnder(3, 2));
  ExpectLevels(model, {0, 1, 2, 2, 3});
  AssertModelContracts(model);
}

void NoOpMutationsAreReferenceTransparentAcrossAllLists() {
  TodoModel model;
  EXPECT_TRUE(model.RenameList(0, L"Inbox"));
  model.AddActive(L"Inbox item", 0);
  model.AddList(L"Work");
  model.AddActive(L"Work root", 0);
  model.AddActive(L"Work child", 1);
  model.SetDone(0, true);
  const auto before = CaptureModel(model);
  const int beforeCurrent = model.CurrentListIndex();

  model.SetText(-1, L"bad");
  model.SetText(99, L"bad");
  model.Remove(-1);
  model.Remove(99);
  model.SetDone(-1, true);
  model.SetDone(99, false);
  EXPECT_FALSE(model.MoveActive(-1, 0));
  EXPECT_FALSE(model.MoveActive(99, 0));
  EXPECT_FALSE(model.IndentItemUnder(0, 99));
  EXPECT_FALSE(model.IndentItemUnder(99, 0));
  EXPECT_FALSE(model.OutdentItem(-1));
  EXPECT_FALSE(model.OutdentItem(99));
  EXPECT_FALSE(model.ToggleCollapsed(-1));
  EXPECT_FALSE(model.ToggleCollapsed(99));
  EXPECT_FALSE(model.SetCurrentListIndex(-1));
  EXPECT_FALSE(model.SetCurrentListIndex(99));
  EXPECT_FALSE(model.SetCurrentListId("missing"));
  EXPECT_FALSE(model.RenameList(-1, L"bad"));
  EXPECT_FALSE(model.RenameList(99, L"bad"));
  EXPECT_FALSE(model.RemoveList(-1));
  EXPECT_FALSE(model.RemoveList(99));

  EXPECT_EQ(model.CurrentListIndex(), beforeCurrent);
  const auto after = CaptureModel(model);
  EXPECT_EQ(after.size(), before.size());
  for (size_t i = 0; i < after.size(); ++i) ExpectSnapshotEqual(after[i], before[i]);
  AssertModelContracts(model);
}

void RandomizedModelMetamorphicProperties() {
  for (int seed = 0; seed < 25; ++seed) {
    TodoModel model;
    std::mt19937 rng(0x58544f44u + static_cast<unsigned>(seed));
    for (int step = 0; step < 400; ++step) {
      const int op = static_cast<int>(rng() % 14);
      const int count = model.Count();
      const int active = model.ActiveCount();
      switch (op) {
        case 0:
          if (active < 45) model.AddActive(L"add " + std::to_wstring(seed) + L":" + std::to_wstring(step), static_cast<int>(rng() % 7) - 2);
          break;
        case 1:
          if (active < 45) model.InsertActive(static_cast<int>(rng() % 70) - 10, L"insert " + std::to_wstring(step), static_cast<int>(rng() % 7) - 2);
          break;
        case 2:
          if (count > 0) model.SetDone(static_cast<int>(rng() % count), (rng() % 2) == 0);
          break;
        case 3:
          if (count > 6) model.Remove(static_cast<int>(rng() % count));
          break;
        case 4:
          if (active > 0) model.MoveActive(static_cast<int>(rng() % active), static_cast<int>(rng() % (active + 1)));
          break;
        case 5:
          if (active > 1) {
            const int index = 1 + static_cast<int>(rng() % (active - 1));
            const int parent = static_cast<int>(rng() % index);
            model.IndentItemUnder(index, parent);
          }
          break;
        case 6:
          if (active > 0) model.OutdentItem(static_cast<int>(rng() % active));
          break;
        case 7:
          if (count > 0) model.ToggleCollapsed(static_cast<int>(rng() % count));
          break;
        case 8:
          if (model.ListCount() < 8) model.AddList(L"List " + std::to_wstring(seed) + L":" + std::to_wstring(step));
          break;
        case 9:
          if (model.ListCount() > 1) model.RemoveList(static_cast<int>(rng() % model.ListCount()));
          break;
        case 10:
          model.SetCurrentListIndex(static_cast<int>(rng() % model.ListCount()));
          break;
        case 11:
          model.SetCurrentCompletedExpanded((rng() % 2) == 0);
          break;
        case 12:
          if (model.ListCount() > 0) model.RenameList(static_cast<int>(rng() % model.ListCount()), L"renamed " + std::to_wstring(step));
          break;
        case 13:
          if (count > 0) model.SetText(static_cast<int>(rng() % count), L"text " + std::to_wstring(seed) + L":" + std::to_wstring(step));
          break;
      }

      AssertModelContracts(model);

      if (step % 29 == 0) {
        TodoModel roundTrip;
        roundTrip.ReplaceLists(model.Lists(), model.CurrentList().id);
        ExpectModelEqual(roundTrip, model);
        AssertModelContracts(roundTrip);
      }
    }
  }
}

const TestCase kTests[] = {
    {"ReplaceListsIsAnIdempotentNormalizerAcrossAllLists", ReplaceListsIsAnIdempotentNormalizerAcrossAllLists},
    {"CompletingNestedAndAncestorBlocksDoesNotMergeCompletedSiblings", CompletingNestedAndAncestorBlocksDoesNotMergeCompletedSiblings},
    {"NonAdjacentIndentParentCannotBreakLevelContinuity", NonAdjacentIndentParentCannotBreakLevelContinuity},
    {"NoOpMutationsAreReferenceTransparentAcrossAllLists", NoOpMutationsAreReferenceTransparentAcrossAllLists},
    {"RandomizedModelMetamorphicProperties", RandomizedModelMetamorphicProperties},
};

} // namespace

int main() { return RunTests("todo_model_property", kTests); }
