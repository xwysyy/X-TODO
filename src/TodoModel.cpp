#include "TodoModel.h"
#include <algorithm>
#include <cstdio>

namespace {
constexpr const char* kDefaultListId = "inbox";
constexpr const wchar_t* kDefaultListTitle = L"默认";
}

TodoModel::TodoModel() {
    EnsureList();
}

const TodoList& TodoModel::CurrentList() const {
    return lists_[currentList_];
}

TodoList& TodoModel::CurrentListMutable() {
    EnsureList();
    return lists_[currentList_];
}

const TodoList* TodoModel::ListAt(int index) const {
    if (index < 0 || index >= static_cast<int>(lists_.size())) return nullptr;
    return &lists_[(size_t)index];
}

int TodoModel::ActiveCount() const {
    return CurrentList().activeCount;
}

int TodoModel::CompletedCount() const {
    const TodoList& list = CurrentList();
    return static_cast<int>(list.items.size()) - list.activeCount;
}

int TodoModel::TotalActiveCount() const {
    int total = 0;
    for (const TodoList& list : lists_) total += list.activeCount;
    return total;
}

int TodoModel::AddActive(const std::wstring& text) {
    TodoList& list = CurrentListMutable();
    int at = list.activeCount;
    list.items.insert(list.items.begin() + at, TodoItem{ text, false });
    ++list.activeCount;
    return at;
}

void TodoModel::SetText(int index, const std::wstring& text) {
    TodoList& list = CurrentListMutable();
    if (index < 0 || index >= static_cast<int>(list.items.size())) return;
    list.items[(size_t)index].text = text;
}

void TodoModel::Remove(int index) {
    TodoList& list = CurrentListMutable();
    if (index < 0 || index >= static_cast<int>(list.items.size())) return;
    if (!list.items[(size_t)index].done) --list.activeCount;
    list.items.erase(list.items.begin() + index);
}

void TodoModel::SetDone(int index, bool done) {
    TodoList& list = CurrentListMutable();
    if (index < 0 || index >= static_cast<int>(list.items.size())) return;
    if (list.items[(size_t)index].done == done) return;

    TodoItem item = list.items[(size_t)index];
    item.done = done;
    list.items.erase(list.items.begin() + index);
    if (done) {
        --list.activeCount;
        list.items.insert(list.items.begin() + list.activeCount, item);
    } else {
        list.items.insert(list.items.begin() + list.activeCount, item);
        ++list.activeCount;
    }
}

void TodoModel::ClearCompleted() {
    TodoList& list = CurrentListMutable();
    list.items.erase(list.items.begin() + list.activeCount, list.items.end());
}

void TodoModel::MoveActive(int from, int insertAt) {
    TodoList& list = CurrentListMutable();
    if (from < 0 || from >= list.activeCount) return;

    TodoItem item = list.items[(size_t)from];
    list.items.erase(list.items.begin() + from);

    int maxAt = list.activeCount - 1;
    if (insertAt < 0) insertAt = 0;
    if (insertAt > maxAt) insertAt = maxAt;
    list.items.insert(list.items.begin() + insertAt, item);
}

int TodoModel::AddList(const std::wstring& title) {
    EnsureList();
    TodoList list;
    list.id = MakeListId();
    list.title = title.empty() ? kDefaultListTitle : title;
    list.completedExpanded = false;
    list.activeCount = 0;
    lists_.push_back(std::move(list));
    currentList_ = static_cast<int>(lists_.size()) - 1;
    return currentList_;
}

bool TodoModel::SetCurrentListIndex(int index) {
    EnsureList();
    if (index < 0 || index >= static_cast<int>(lists_.size())) return false;
    currentList_ = index;
    return true;
}

bool TodoModel::SetCurrentListId(const std::string& id) {
    EnsureList();
    for (size_t i = 0; i < lists_.size(); ++i) {
        if (lists_[i].id == id) {
            currentList_ = static_cast<int>(i);
            return true;
        }
    }
    return false;
}

void TodoModel::RenameCurrentList(const std::wstring& title) {
    if (title.empty()) return;
    CurrentListMutable().title = title;
}

bool TodoModel::DeleteCurrentList() {
    EnsureList();
    if (lists_.size() <= 1) return false;
    lists_.erase(lists_.begin() + currentList_);
    if (currentList_ >= static_cast<int>(lists_.size()))
        currentList_ = static_cast<int>(lists_.size()) - 1;
    EnsureList();
    return true;
}

void TodoModel::SetCurrentCompletedExpanded(bool expanded) {
    CurrentListMutable().completedExpanded = expanded;
}

void TodoModel::ReplaceAll(std::vector<TodoItem> items, bool completedExpanded) {
    TodoList list;
    list.id = kDefaultListId;
    list.title = kDefaultListTitle;
    list.completedExpanded = completedExpanded;
    list.items = std::move(items);
    Normalize(list);

    lists_.clear();
    lists_.push_back(std::move(list));
    currentList_ = 0;
    nextListId_ = 1;
}

void TodoModel::ReplaceLists(std::vector<TodoList> lists, const std::string& currentId) {
    lists_.clear();
    for (TodoList& list : lists) {
        if (list.id.empty()) list.id = MakeListId();
        if (list.title.empty()) list.title = kDefaultListTitle;
        Normalize(list);
        lists_.push_back(std::move(list));
    }
    EnsureList();
    currentList_ = 0;
    if (!currentId.empty()) SetCurrentListId(currentId);
}

void TodoModel::EnsureList() {
    if (lists_.empty()) {
        TodoList list;
        list.id = kDefaultListId;
        list.title = kDefaultListTitle;
        lists_.push_back(std::move(list));
    }
    if (currentList_ < 0 || currentList_ >= static_cast<int>(lists_.size()))
        currentList_ = 0;

    int maxSeen = 0;
    for (const TodoList& list : lists_) {
        if (list.id.rfind("list-", 0) == 0) {
            int n = 0;
            if (std::sscanf(list.id.c_str() + 5, "%d", &n) == 1 && n > maxSeen)
                maxSeen = n;
        }
    }
    if (nextListId_ <= maxSeen) nextListId_ = maxSeen + 1;
}

void TodoModel::Normalize(TodoList& list) {
    auto mid = std::stable_partition(list.items.begin(), list.items.end(),
        [](const TodoItem& it) { return !it.done; });
    list.activeCount = static_cast<int>(mid - list.items.begin());
}

std::string TodoModel::MakeListId() {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "list-%d", nextListId_);
    nextListId_++;
    return std::string(buf);
}
