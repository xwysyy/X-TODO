#include "TodoModel.h"

#include <exception>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

[[noreturn]] void Fail(const char* file, int line, const std::string& message) {
    std::ostringstream out;
    out << file << ':' << line << ": " << message;
    throw std::runtime_error(out.str());
}

#define EXPECT_TRUE(expr) \
    do { if (!(expr)) Fail(__FILE__, __LINE__, std::string("Expected true: ") + #expr); } while (false)

#define EXPECT_FALSE(expr) \
    do { if ((expr)) Fail(__FILE__, __LINE__, std::string("Expected false: ") + #expr); } while (false)

template <class A, class B>
void ExpectEqual(const A& actual, const B& expected, const char* actualExpr,
                 const char* expectedExpr, const char* file, int line) {
    if (!(actual == expected)) {
        std::ostringstream out;
        out << "Expected equality: " << actualExpr << " == " << expectedExpr;
        Fail(file, line, out.str());
    }
}

#define EXPECT_EQ(actual, expected) \
    do { ExpectEqual((actual), (expected), #actual, #expected, __FILE__, __LINE__); } while (false)

std::vector<std::wstring> Texts(const TodoModel& model) {
    std::vector<std::wstring> out;
    for (const TodoItem& item : model.Items()) out.push_back(item.text);
    return out;
}

std::vector<int> Levels(const TodoModel& model) {
    std::vector<int> out;
    for (const TodoItem& item : model.Items()) out.push_back(item.level);
    return out;
}

std::vector<bool> Dones(const TodoModel& model) {
    std::vector<bool> out;
    for (const TodoItem& item : model.Items()) out.push_back(item.done);
    return out;
}

int LocalSubtreeEnd(const TodoList& list, int index, int limit) {
    const int count = static_cast<int>(list.items.size());
    if (index < 0 || index >= count) return index;
    if (limit < index + 1) limit = index + 1;
    if (limit > count) limit = count;
    const int baseLevel = list.items[static_cast<size_t>(index)].level;
    int end = index + 1;
    while (end < limit && list.items[static_cast<size_t>(end)].level > baseLevel) ++end;
    return end;
}

void ExpectTexts(const TodoModel& model, std::initializer_list<const wchar_t*> expected) {
    std::vector<std::wstring> actual = Texts(model);
    std::vector<std::wstring> want;
    for (const wchar_t* text : expected) want.emplace_back(text);
    EXPECT_EQ(actual, want);
}

void ExpectLevels(const TodoModel& model, std::initializer_list<int> expected) {
    std::vector<int> actual = Levels(model);
    std::vector<int> want(expected);
    EXPECT_EQ(actual, want);
}

void ExpectDones(const TodoModel& model, std::initializer_list<bool> expected) {
    std::vector<bool> actual = Dones(model);
    std::vector<bool> want(expected);
    EXPECT_EQ(actual, want);
}

void AssertListInvariants(const TodoList& list) {
    const int count = static_cast<int>(list.items.size());
    int active = 0;
    for (const TodoItem& item : list.items) {
        if (!item.done) ++active;
    }
    EXPECT_EQ(list.activeCount, active);
    EXPECT_TRUE(list.activeCount >= 0);
    EXPECT_TRUE(list.activeCount <= count);

    for (int i = 0; i < count; ++i) {
        const TodoItem& item = list.items[static_cast<size_t>(i)];
        EXPECT_TRUE(item.level >= 0);
        EXPECT_TRUE(item.level <= kMaxTodoLevel);
        if (i < list.activeCount) {
            EXPECT_FALSE(item.done);
        } else {
            EXPECT_TRUE(item.done);
        }
    }

    auto checkSection = [&](int first, int last) {
        if (first >= last) return;
        EXPECT_EQ(list.items[static_cast<size_t>(first)].level, 0);
        for (int i = first + 1; i < last; ++i) {
            EXPECT_TRUE(list.items[static_cast<size_t>(i)].level <=
                        list.items[static_cast<size_t>(i) - 1].level + 1);
        }
    };
    checkSection(0, list.activeCount);
    checkSection(list.activeCount, count);

    for (int i = 0; i < count; ++i) {
        const int limit = list.items[static_cast<size_t>(i)].done ? count : list.activeCount;
        if (list.items[static_cast<size_t>(i)].collapsed) {
            EXPECT_TRUE(LocalSubtreeEnd(list, i, limit) > i + 1);
        }
    }
}

void AssertInvariants(const TodoModel& model) {
    EXPECT_TRUE(model.ListCount() >= 1);
    EXPECT_TRUE(model.CurrentListIndex() >= 0);
    EXPECT_TRUE(model.CurrentListIndex() < model.ListCount());

    int totalActive = 0;
    for (const TodoList& list : model.Lists()) {
        AssertListInvariants(list);
        totalActive += list.activeCount;
    }
    EXPECT_EQ(model.TotalActiveCount(), totalActive);
}

void DefaultModelHasOneInboxList() {
    TodoModel model;
    EXPECT_EQ(model.ListCount(), 1);
    EXPECT_EQ(model.CurrentListIndex(), 0);
    EXPECT_EQ(model.CurrentList().id, std::string("inbox"));
    EXPECT_EQ(model.CurrentList().title, std::wstring(L"默认"));
    EXPECT_EQ(model.Count(), 0);
    EXPECT_EQ(model.ActiveCount(), 0);
    EXPECT_EQ(model.CompletedCount(), 0);
    AssertInvariants(model);
}

void AddDoneUndoAndClearMaintainActiveCompletedPartition() {
    TodoModel model;
    EXPECT_EQ(model.AddActive(L"A"), 0);
    EXPECT_EQ(model.AddActive(L"B"), 1);
    EXPECT_EQ(model.AddActive(L"C"), 2);

    model.SetDone(1, true);
    ExpectTexts(model, {L"A", L"C", L"B"});
    ExpectDones(model, {false, false, true});
    EXPECT_EQ(model.ActiveCount(), 2);
    EXPECT_EQ(model.CompletedCount(), 1);
    AssertInvariants(model);

    model.SetDone(2, false);
    ExpectTexts(model, {L"A", L"C", L"B"});
    ExpectDones(model, {false, false, false});
    EXPECT_EQ(model.ActiveCount(), 3);
    EXPECT_EQ(model.CompletedCount(), 0);
    AssertInvariants(model);

    model.SetDone(0, true);
    model.SetDone(0, true);
    ExpectTexts(model, {L"B", L"C", L"A"});
    ExpectDones(model, {false, true, true});
    model.ClearCompleted();
    ExpectTexts(model, {L"B"});
    EXPECT_EQ(model.ActiveCount(), 1);
    EXPECT_EQ(model.CompletedCount(), 0);
    AssertInvariants(model);
}

void InsertAfterCurrentSubtreeCanCreateSiblingBeforeNextRoot() {
    TodoModel model;
    model.AddActive(L"Parent", 0);
    model.AddActive(L"Child", 1);
    model.AddActive(L"Grandchild", 2);
    model.AddActive(L"Next root", 0);

    EXPECT_EQ(model.SubtreeEnd(0), 3);
    const int inserted = model.InsertActive(model.SubtreeEnd(0), L"Inserted sibling", model.Items()[0].level);

    EXPECT_EQ(inserted, 3);
    ExpectTexts(model, {L"Parent", L"Child", L"Grandchild", L"Inserted sibling", L"Next root"});
    ExpectLevels(model, {0, 1, 2, 0, 0});
    EXPECT_EQ(model.SubtreeEnd(0), 3);
    AssertInvariants(model);
}

void SubtreeDoneUndoMovesWholeBlockAndRebasesLevels() {
    TodoModel model;
    model.AddActive(L"A", 0);
    model.AddActive(L"A.1", 1);
    model.AddActive(L"A.2", 1);
    model.AddActive(L"B", 0);
    model.AddActive(L"C", 0);

    model.SetDone(0, true);
    ExpectTexts(model, {L"B", L"C", L"A", L"A.1", L"A.2"});
    ExpectDones(model, {false, false, true, true, true});
    ExpectLevels(model, {0, 0, 0, 1, 1});
    EXPECT_EQ(model.ActiveCount(), 2);
    EXPECT_EQ(model.CompletedCount(), 3);
    AssertInvariants(model);

    model.SetDone(model.ActiveCount(), false);
    ExpectTexts(model, {L"B", L"C", L"A", L"A.1", L"A.2"});
    ExpectDones(model, {false, false, false, false, false});
    ExpectLevels(model, {0, 0, 0, 1, 1});
    EXPECT_EQ(model.ActiveCount(), 5);
    EXPECT_EQ(model.CompletedCount(), 0);
    AssertInvariants(model);
}

void RemoveDeletesActiveAndCompletedSubtrees() {
    TodoModel active;
    active.AddActive(L"A", 0);
    active.AddActive(L"A.1", 1);
    active.AddActive(L"A.2", 1);
    active.AddActive(L"B", 0);
    active.Remove(0);
    ExpectTexts(active, {L"B"});
    EXPECT_EQ(active.ActiveCount(), 1);
    AssertInvariants(active);

    TodoModel completed;
    completed.AddActive(L"A", 0);
    completed.AddActive(L"A.1", 1);
    completed.AddActive(L"A.2", 1);
    completed.AddActive(L"B", 0);
    completed.SetDone(0, true);
    completed.Remove(completed.ActiveCount());
    ExpectTexts(completed, {L"B"});
    EXPECT_EQ(completed.ActiveCount(), 1);
    EXPECT_EQ(completed.CompletedCount(), 0);
    AssertInvariants(completed);
}

void MoveActiveReordersWholeSubtreeAndRejectsDropsInsideItself() {
    TodoModel model;
    model.AddActive(L"A", 0);
    model.AddActive(L"A.1", 1);
    model.AddActive(L"A.2", 1);
    model.AddActive(L"B", 0);
    model.AddActive(L"C", 0);

    EXPECT_FALSE(model.MoveActive(0, 2));
    ExpectTexts(model, {L"A", L"A.1", L"A.2", L"B", L"C"});

    EXPECT_TRUE(model.MoveActive(0, 4));
    ExpectTexts(model, {L"B", L"A", L"A.1", L"A.2", L"C"});
    ExpectLevels(model, {0, 0, 1, 1, 0});
    AssertInvariants(model);

    EXPECT_TRUE(model.MoveActive(1, model.ActiveCount()));
    ExpectTexts(model, {L"B", L"C", L"A", L"A.1", L"A.2"});
    ExpectLevels(model, {0, 0, 0, 1, 1});
    AssertInvariants(model);
}

void IndentOutdentAndCollapseRespectTreeBoundaries() {
    TodoModel model;
    model.AddActive(L"A");
    model.AddActive(L"B");
    model.AddActive(L"C");

    EXPECT_TRUE(model.IndentItemUnder(1, 0));
    EXPECT_TRUE(model.IndentItemUnder(2, 1));
    ExpectLevels(model, {0, 1, 1});
    EXPECT_TRUE(model.IndentItemUnder(2, 1));
    ExpectLevels(model, {0, 1, 2});
    EXPECT_TRUE(model.HasChildren(0));
    EXPECT_TRUE(model.HasChildren(1));
    EXPECT_FALSE(model.HasChildren(2));

    EXPECT_TRUE(model.ToggleCollapsed(0));
    EXPECT_TRUE(model.Items()[0].collapsed);
    EXPECT_FALSE(model.ToggleCollapsed(2));
    EXPECT_FALSE(model.Items()[2].collapsed);

    EXPECT_TRUE(model.OutdentItem(1));
    ExpectLevels(model, {0, 0, 1});
    EXPECT_FALSE(model.HasChildren(0));
    EXPECT_FALSE(model.Items()[0].collapsed);
    EXPECT_TRUE(model.HasChildren(1));
    AssertInvariants(model);

    EXPECT_TRUE(model.OutdentItem(2));
    ExpectLevels(model, {0, 0, 0});
    EXPECT_FALSE(model.HasChildren(1));
    AssertInvariants(model);
}

void ReplaceAllNormalizesLegacySingleListData() {
    TodoModel model;
    std::vector<TodoItem> items = {
        TodoItem{L"done first", true, 99, true},
        TodoItem{L"active root", false, 3, true},
        TodoItem{L"active child", false, 3, false},
        TodoItem{L"done child", true, 2, false},
    };

    model.ReplaceAll(std::move(items), true);
    ExpectTexts(model, {L"active root", L"active child", L"done first", L"done child"});
    ExpectDones(model, {false, false, true, true});
    ExpectLevels(model, {0, 1, 0, 1});
    EXPECT_EQ(model.ActiveCount(), 2);
    EXPECT_EQ(model.CompletedCount(), 2);
    EXPECT_TRUE(model.CurrentList().completedExpanded);
    EXPECT_EQ(model.CurrentList().id, std::string("inbox"));
    AssertInvariants(model);
}

void ReplaceListsNormalizesEveryListSelectsCurrentAndPreservesNextId() {
    TodoList generatedId;
    generatedId.title = L"";
    generatedId.items = {
        TodoItem{L"done", true, 3, true},
        TodoItem{L"active", false, 2, false},
    };

    TodoList existing;
    existing.id = "list-5";
    existing.title = L"Work";
    existing.completedExpanded = true;
    existing.items = {
        TodoItem{L"work active", false, 0, false},
        TodoItem{L"work done", true, 0, false},
    };

    TodoModel model;
    model.ReplaceLists({generatedId, existing}, "list-5");

    EXPECT_EQ(model.ListCount(), 2);
    EXPECT_EQ(model.ListAt(0)->id, std::string("list-1"));
    EXPECT_EQ(model.ListAt(0)->title, std::wstring(L"默认"));
    EXPECT_EQ(model.ListAt(0)->activeCount, 1);
    EXPECT_EQ(model.CurrentListIndex(), 1);
    EXPECT_EQ(model.CurrentList().id, std::string("list-5"));
    EXPECT_TRUE(model.CurrentList().completedExpanded);
    AssertInvariants(model);

    const int added = model.AddList(L"Later");
    EXPECT_EQ(added, 2);
    EXPECT_EQ(model.CurrentList().id, std::string("list-6"));
    AssertInvariants(model);
}

void MultipleListsKeepItemsAndActiveCountsIsolated() {
    TodoModel model;
    model.AddActive(L"Inbox item");
    const int workIndex = model.AddList(L"Work");
    model.AddActive(L"Work item");
    model.AddActive(L"Second work item");
    model.SetDone(0, true);

    EXPECT_EQ(model.CurrentListIndex(), workIndex);
    EXPECT_EQ(model.ActiveCount(), 1);
    EXPECT_EQ(model.CompletedCount(), 1);
    EXPECT_EQ(model.TotalActiveCount(), 2);

    EXPECT_TRUE(model.SetCurrentListIndex(0));
    ExpectTexts(model, {L"Inbox item"});
    EXPECT_EQ(model.ActiveCount(), 1);
    EXPECT_EQ(model.CompletedCount(), 0);

    EXPECT_TRUE(model.SetCurrentListIndex(workIndex));
    ExpectTexts(model, {L"Second work item", L"Work item"});
    ExpectDones(model, {false, true});
    AssertInvariants(model);
}

void RenameListRejectsInvalidOrEmptyTitleAndPreservesExistingTitle() {
    TodoModel model;

    EXPECT_FALSE(model.RenameList(-1, L"Bad"));
    EXPECT_FALSE(model.RenameList(99, L"Bad"));
    EXPECT_TRUE(model.RenameList(0, L"Inbox renamed"));
    EXPECT_EQ(model.ListAt(0)->title, std::wstring(L"Inbox renamed"));

    EXPECT_FALSE(model.RenameList(0, L""));
    EXPECT_EQ(model.ListAt(0)->title, std::wstring(L"Inbox renamed"));
    AssertInvariants(model);
}

void RemoveListRejectsInvalidIndexesAndLastRemainingList() {
    TodoModel model;

    EXPECT_FALSE(model.RemoveList(-1));
    EXPECT_FALSE(model.RemoveList(99));
    EXPECT_FALSE(model.RemoveList(0));
    EXPECT_EQ(model.ListCount(), 1);
    EXPECT_EQ(model.CurrentListIndex(), 0);
    EXPECT_EQ(model.CurrentList().id, std::string("inbox"));
    AssertInvariants(model);
}

void RemoveListAdjustsCurrentIndexAndPreservesOtherLists() {
    TodoModel model;
    EXPECT_TRUE(model.RenameList(0, L"Inbox"));
    model.AddActive(L"Inbox item");

    const int work = model.AddList(L"Work");
    model.AddActive(L"Work item");
    model.AddActive(L"Done work");
    model.SetDone(1, true);

    const int home = model.AddList(L"Home");
    model.AddActive(L"Home item");
    EXPECT_EQ(model.CurrentListIndex(), home);

    EXPECT_TRUE(model.SetCurrentListIndex(work));
    EXPECT_TRUE(model.RemoveList(0));
    EXPECT_EQ(model.CurrentListIndex(), 0);
    EXPECT_EQ(model.CurrentList().title, std::wstring(L"Work"));
    ExpectTexts(model, {L"Work item", L"Done work"});
    ExpectDones(model, {false, true});
    EXPECT_EQ(model.ActiveCount(), 1);
    EXPECT_EQ(model.CompletedCount(), 1);

    EXPECT_TRUE(model.RemoveList(1));
    EXPECT_EQ(model.ListCount(), 1);
    EXPECT_EQ(model.CurrentListIndex(), 0);
    EXPECT_EQ(model.CurrentList().title, std::wstring(L"Work"));
    AssertInvariants(model);
}

void RemoveCurrentLastListSelectsPreviousList() {
    TodoModel model;
    EXPECT_TRUE(model.RenameList(0, L"A"));
    model.AddList(L"B");
    model.AddList(L"C");

    EXPECT_EQ(model.CurrentListIndex(), 2);
    EXPECT_TRUE(model.RemoveList(2));
    EXPECT_EQ(model.ListCount(), 2);
    EXPECT_EQ(model.CurrentListIndex(), 1);
    EXPECT_EQ(model.CurrentList().title, std::wstring(L"B"));

    EXPECT_TRUE(model.RemoveList(1));
    EXPECT_EQ(model.ListCount(), 1);
    EXPECT_EQ(model.CurrentListIndex(), 0);
    EXPECT_EQ(model.CurrentList().title, std::wstring(L"A"));
    AssertInvariants(model);
}

void InvalidIndexesAndNoOpMutationsAreSafe() {
    TodoModel model;
    model.AddActive(L"Only");

    model.SetText(-1, L"bad");
    model.SetText(99, L"bad");
    model.Remove(-1);
    model.Remove(99);
    model.SetDone(-1, true);
    model.SetDone(99, true);
    EXPECT_FALSE(model.MoveActive(-1, 0));
    EXPECT_FALSE(model.MoveActive(99, 0));
    EXPECT_FALSE(model.IndentItemUnder(0, 0));
    EXPECT_FALSE(model.OutdentItem(0));
    EXPECT_FALSE(model.ToggleCollapsed(0));
    EXPECT_FALSE(model.SetCurrentListIndex(-1));
    EXPECT_FALSE(model.SetCurrentListIndex(99));
    EXPECT_FALSE(model.SetCurrentListId("missing"));

    ExpectTexts(model, {L"Only"});
    EXPECT_EQ(model.ActiveCount(), 1);
    AssertInvariants(model);
}

void DeterministicMutationSequenceMaintainsInvariantsAfterEveryStep() {
    TodoModel model;
    AssertInvariants(model);

    model.AddActive(L"A");
    AssertInvariants(model);
    model.AddActive(L"B");
    AssertInvariants(model);
    EXPECT_TRUE(model.IndentItemUnder(1, 0));
    AssertInvariants(model);
    EXPECT_TRUE(model.ToggleCollapsed(0));
    AssertInvariants(model);
    model.InsertActive(model.SubtreeEnd(0), L"C", 0);
    AssertInvariants(model);
    EXPECT_TRUE(model.MoveActive(0, model.ActiveCount()));
    AssertInvariants(model);
    model.SetDone(1, true);
    AssertInvariants(model);
    model.SetDone(model.ActiveCount(), false);
    AssertInvariants(model);
    model.Remove(0);
    AssertInvariants(model);
    model.ClearCompleted();
    AssertInvariants(model);

    TodoList custom;
    custom.id = "list-3";
    custom.title = L"Imported";
    custom.items = {
        TodoItem{L"imported done", true, 3, true},
        TodoItem{L"imported active", false, 3, true},
    };
    model.ReplaceLists({custom}, "list-3");
    AssertInvariants(model);

    EXPECT_TRUE(model.RenameList(0, L"Imported renamed"));
    AssertInvariants(model);
    model.AddList(L"Scratch");
    AssertInvariants(model);
    EXPECT_TRUE(model.RemoveList(0));
    AssertInvariants(model);
}

struct TestCase {
    const char* name;
    void (*fn)();
};

const TestCase kTests[] = {
    {"DefaultModelHasOneInboxList", DefaultModelHasOneInboxList},
    {"AddDoneUndoAndClearMaintainActiveCompletedPartition", AddDoneUndoAndClearMaintainActiveCompletedPartition},
    {"InsertAfterCurrentSubtreeCanCreateSiblingBeforeNextRoot", InsertAfterCurrentSubtreeCanCreateSiblingBeforeNextRoot},
    {"SubtreeDoneUndoMovesWholeBlockAndRebasesLevels", SubtreeDoneUndoMovesWholeBlockAndRebasesLevels},
    {"RemoveDeletesActiveAndCompletedSubtrees", RemoveDeletesActiveAndCompletedSubtrees},
    {"MoveActiveReordersWholeSubtreeAndRejectsDropsInsideItself", MoveActiveReordersWholeSubtreeAndRejectsDropsInsideItself},
    {"IndentOutdentAndCollapseRespectTreeBoundaries", IndentOutdentAndCollapseRespectTreeBoundaries},
    {"ReplaceAllNormalizesLegacySingleListData", ReplaceAllNormalizesLegacySingleListData},
    {"ReplaceListsNormalizesEveryListSelectsCurrentAndPreservesNextId", ReplaceListsNormalizesEveryListSelectsCurrentAndPreservesNextId},
    {"MultipleListsKeepItemsAndActiveCountsIsolated", MultipleListsKeepItemsAndActiveCountsIsolated},
    {"RenameListRejectsInvalidOrEmptyTitleAndPreservesExistingTitle", RenameListRejectsInvalidOrEmptyTitleAndPreservesExistingTitle},
    {"RemoveListRejectsInvalidIndexesAndLastRemainingList", RemoveListRejectsInvalidIndexesAndLastRemainingList},
    {"RemoveListAdjustsCurrentIndexAndPreservesOtherLists", RemoveListAdjustsCurrentIndexAndPreservesOtherLists},
    {"RemoveCurrentLastListSelectsPreviousList", RemoveCurrentLastListSelectsPreviousList},
    {"InvalidIndexesAndNoOpMutationsAreSafe", InvalidIndexesAndNoOpMutationsAreSafe},
    {"DeterministicMutationSequenceMaintainsInvariantsAfterEveryStep", DeterministicMutationSequenceMaintainsInvariantsAfterEveryStep},
};

} // namespace

int main() {
    int failed = 0;
    int passed = 0;
    for (const TestCase& test : kTests) {
        try {
            test.fn();
            ++passed;
            std::cout << "[PASS] " << test.name << '\n';
        } catch (const std::exception& ex) {
            ++failed;
            std::cerr << "[FAIL] " << test.name << "\n       " << ex.what() << '\n';
        } catch (...) {
            ++failed;
            std::cerr << "[FAIL] " << test.name << "\n       unknown exception\n";
        }
    }

    std::cout << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
