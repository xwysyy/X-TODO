#include "StoreFormat.h"
#include "todo_model_test_utils.h"

#include <algorithm>
#include <string>
#include <vector>

using namespace xtodo_test;

namespace {

struct ModelSnapshot {
  int currentListIndex = 0;
  std::vector<TodoList> lists;
};

ModelSnapshot CaptureModel(const TodoModel& model) {
  ModelSnapshot out;
  out.currentListIndex = model.CurrentListIndex();
  out.lists = model.Lists();
  return out;
}

void ExpectModelSnapshot(const TodoModel& model, const ModelSnapshot& expected) {
  EXPECT_EQ(model.CurrentListIndex(), expected.currentListIndex);
  EXPECT_EQ(model.ListCount(), static_cast<int>(expected.lists.size()));
  for (int i = 0; i < model.ListCount(); ++i) {
    const TodoList* actual = model.ListAt(i);
    EXPECT_TRUE(actual != nullptr);
    const TodoList& want = expected.lists[static_cast<size_t>(i)];
    EXPECT_EQ(actual->id, want.id);
    EXPECT_EQ(actual->title, want.title);
    EXPECT_EQ(actual->completedExpanded, want.completedExpanded);
    EXPECT_EQ(actual->activeCount, want.activeCount);
    EXPECT_EQ(actual->items.size(), want.items.size());
    for (size_t j = 0; j < actual->items.size(); ++j) {
      EXPECT_EQ(actual->items[j].text, want.items[j].text);
      EXPECT_EQ(actual->items[j].done, want.items[j].done);
      EXPECT_EQ(actual->items[j].level, want.items[j].level);
      EXPECT_EQ(actual->items[j].collapsed, want.items[j].collapsed);
    }
  }
}

void ExpectCalendarBlocksEqual(const CalendarModel& actual, const std::vector<CalendarBlock>& expected) {
  const auto& blocks = actual.Blocks();
  EXPECT_EQ(blocks.size(), expected.size());
  for (size_t i = 0; i < blocks.size(); ++i) {
    EXPECT_EQ(blocks[i].id, expected[i].id);
    EXPECT_EQ(blocks[i].day, expected[i].day);
    EXPECT_EQ(blocks[i].startMinute, expected[i].startMinute);
    EXPECT_EQ(blocks[i].endMinute, expected[i].endMinute);
    EXPECT_EQ(blocks[i].title, expected[i].title);
  }
}

void ExpectGeometryEqual(const WindowGeometry& actual, const WindowGeometry& expected) {
  EXPECT_EQ(actual.x, expected.x);
  EXPECT_EQ(actual.y, expected.y);
  EXPECT_EQ(actual.w, expected.w);
  EXPECT_EQ(actual.h, expected.h);
  EXPECT_EQ(actual.valid, expected.valid);
}

void ExpectUiEqual(const UiState& actual, const UiState& expected) {
  EXPECT_EQ(actual.alwaysOnTop, expected.alwaysOnTop);
  EXPECT_EQ(actual.mountMode, expected.mountMode);
  EXPECT_EQ(actual.lang, expected.lang);
  EXPECT_EQ(actual.themeMode, expected.themeMode);
  EXPECT_EQ(actual.themeId, expected.themeId);
  EXPECT_EQ(actual.lightThemeId, expected.lightThemeId);
  EXPECT_EQ(actual.darkThemeId, expected.darkThemeId);
  EXPECT_EQ(actual.capsuleStyle, expected.capsuleStyle);
  EXPECT_EQ(actual.capsuleDockEdge, expected.capsuleDockEdge);
  EXPECT_NEAR(actual.capsuleDockT, expected.capsuleDockT, 0.000001);
  EXPECT_EQ(actual.capsuleMonitor, expected.capsuleMonitor);
  EXPECT_EQ(actual.activeView, expected.activeView);
  EXPECT_EQ(actual.calendarDay, expected.calendarDay);
  EXPECT_TRUE(actual.calendarView == expected.calendarView);
  EXPECT_EQ(actual.backupDir, expected.backupDir);
  EXPECT_EQ(actual.backupLastEpoch, expected.backupLastEpoch);
}

void SeedRichState(TodoModel& model, CalendarModel& calendar, WindowGeometry& geom, UiState& ui) {
  EXPECT_TRUE(model.RenameList(0, L"Inbox"));
  model.AddActive(L"root", 0);
  model.AddActive(L"child with newline\nline 2", 1);
  EXPECT_TRUE(model.ToggleCollapsed(0));
  model.SetDone(0, true);
  model.AddActive(L"unicode 中文 quote \" slash \\ tab\t", 0);

  model.AddList(L"Work");
  model.AddActive(L"work root", 0);
  model.AddActive(L"work child", 1);
  model.SetCurrentCompletedExpanded(true);

  const int leapBlock = calendar.AddBlock("2024-02-29", 9 * 60, 10 * 60 + 15, L"Leap day block");
  const int reviewBlock = calendar.AddBlock("2026-06-22", 13 * 60, 14 * 60, L"Review PR");
  EXPECT_TRUE(leapBlock > 0);
  EXPECT_TRUE(reviewBlock > leapBlock);

  geom = WindowGeometry{11, 22, 640, 480, true};
  ui.alwaysOnTop = false;
  ui.mountMode = "capsule";
  ui.lang = "en";
  ui.themeMode = "follow_system";
  ui.themeId = "paper";
  ui.lightThemeId = "paper";
  ui.darkThemeId = "paper";
  ui.capsuleStyle = "dot";
  ui.capsuleDockEdge = "left";
  ui.capsuleDockT = 0.75;
  ui.capsuleMonitor = "\\\\.\\DISPLAY1";
  ui.activeView = "calendar";
  ui.calendarDay = "2024-02-29";
  ui.calendarView = CalendarViewMode::Month;
  ui.backupDir = L"C:\\x-todo-backup";
  ui.backupLastEpoch = 1771590000LL;

  AssertInvariants(model);
}

void RoundTripPreservesRichTodoCalendarGeometryAndUiState() {
  TodoModel model;
  CalendarModel calendar;
  WindowGeometry geom;
  UiState ui;
  SeedRichState(model, calendar, geom, ui);

  const ModelSnapshot expectedModel = CaptureModel(model);
  const std::vector<CalendarBlock> expectedCalendar = calendar.Blocks();
  const WindowGeometry expectedGeom = geom;
  const UiState expectedUi = ui;

  const std::string serialized = StoreFormat::Serialize(model, calendar, geom, ui);

  TodoModel reparsedModel;
  CalendarModel reparsedCalendar;
  WindowGeometry reparsedGeom;
  UiState reparsedUi;
  EXPECT_TRUE(StoreFormat::Parse(serialized, reparsedModel, reparsedCalendar, reparsedGeom, reparsedUi));

  AssertInvariants(reparsedModel);
  ExpectModelSnapshot(reparsedModel, expectedModel);
  ExpectCalendarBlocksEqual(reparsedCalendar, expectedCalendar);
  ExpectGeometryEqual(reparsedGeom, expectedGeom);
  ExpectUiEqual(reparsedUi, expectedUi);
}

void InvalidInputsDoNotMutateAlreadyLoadedState() {
  TodoModel model;
  EXPECT_TRUE(model.RenameList(0, L"Inbox"));
  model.AddActive(L"keep me", 0);
  model.AddList(L"Work");
  model.AddActive(L"work item", 0);
  CalendarModel calendar;
  const int block = calendar.AddBlock("2026-06-22", 9 * 60, 10 * 60, L"meeting");
  EXPECT_TRUE(block > 0);
  WindowGeometry geom{1, 2, 300, 400, true};
  UiState ui;
  ui.mountMode = "capsule";
  ui.themeId = "mint";
  ui.activeView = "calendar";

  const ModelSnapshot modelBefore = CaptureModel(model);
  const std::vector<CalendarBlock> calendarBefore = calendar.Blocks();
  const WindowGeometry geomBefore = geom;
  const UiState uiBefore = ui;

  std::string depthBomb;
  for (int i = 0; i < 80; ++i) depthBomb.push_back('[');
  for (int i = 0; i < 80; ++i) depthBomb.push_back(']');

  const std::vector<std::string> badInputs = {
      "{ definitely not json",
      "[1,2,3]",
      "\"top level scalar\"",
      depthBomb,
  };

  for (const std::string& bad : badInputs) {
    EXPECT_FALSE(StoreFormat::Parse(bad, model, calendar, geom, ui));
    ExpectModelSnapshot(model, modelBefore);
    ExpectCalendarBlocksEqual(calendar, calendarBefore);
    EXPECT_EQ(geom.x, geomBefore.x);
    EXPECT_EQ(geom.y, geomBefore.y);
    EXPECT_EQ(geom.w, geomBefore.w);
    EXPECT_EQ(geom.h, geomBefore.h);
    EXPECT_EQ(geom.valid, geomBefore.valid);
    EXPECT_EQ(ui.mountMode, uiBefore.mountMode);
    EXPECT_EQ(ui.themeId, uiBefore.themeId);
    EXPECT_EQ(ui.activeView, uiBefore.activeView);
  }
}

void BracketDepthGuardIgnoresBracketsInsideStringsAndEscapes() {
  const std::string noisy(96, '{');
  const std::string noisy2(96, ']');
  const std::string text = std::string(R"({
    "lists": [
      { "id": "inbox", "title": "Inbox", "items": [
        { "text": ")") + noisy + noisy2 + R"(", "done": false, "level": 0, "collapsed": false }
      ] }
    ],
    "calendar": [
      { "id": 1, "day": "2026-06-22", "start": 60, "end": 120, "title": ")" + noisy2 + R"(" }
    ]
  })";

  TodoModel model;
  CalendarModel calendar;
  WindowGeometry geom;
  UiState ui;
  EXPECT_TRUE(StoreFormat::Parse(text, model, calendar, geom, ui));
  const std::string combined = noisy + noisy2;
  const std::wstring combinedWide(combined.begin(), combined.end());
  EXPECT_EQ(model.Items().size(), static_cast<size_t>(1));
  EXPECT_EQ(model.Items()[0].text, combinedWide);
  EXPECT_EQ(calendar.Blocks().size(), static_cast<size_t>(1));
  EXPECT_EQ(calendar.Blocks()[0].title, std::wstring(noisy2.begin(), noisy2.end()));
  AssertInvariants(model);
}

void ParseNormalizesMessyImportedListsAndCalendarBeforeSerializing() {
  const std::string text = R"({
    "ui": {
      "currentList": "list-7",
      "mount": "desktop",
      "activeView": "calendar",
      "calendarDay": "2026-06-22",
      "calendarView": "month"
    },
    "lists": [
      {
        "id": "list-7",
        "title": "Imported",
        "completedExpanded": true,
        "items": [
          { "text": "done first", "done": true, "level": 99, "collapsed": true },
          { "text": "active too deep", "done": false, "level": 3, "collapsed": true },
          { "text": "active child", "done": false, "level": 3, "collapsed": false },
          { "text": "done child", "done": true, "level": 3, "collapsed": true }
        ]
      },
      {
        "id": "",
        "title": "",
        "items": [
          { "text": "generated list active", "done": false, "level": 2, "collapsed": true }
        ]
      }
    ],
    "calendar": [
      { "id": 5, "day": "2026-06-22", "start": 600, "end": 660, "title": "A" },
      { "id": 5, "day": "2026-06-22", "start": 660, "end": 660, "title": "duplicate" },
      { "id": 6, "day": "2026-13-22", "start": 1, "end": 2, "title": "bad day" }
    ]
  })";

  TodoModel model;
  CalendarModel calendar;
  WindowGeometry geom;
  UiState ui;
  EXPECT_TRUE(StoreFormat::Parse(text, model, calendar, geom, ui));
  EXPECT_EQ(model.ListCount(), 2);
  EXPECT_EQ(model.CurrentList().id, std::string("list-7"));
  ExpectTexts(model, {L"active too deep", L"active child", L"done first", L"done child"});
  ExpectLevels(model, {0, 1, 0, 1});
  ExpectDones(model, {false, false, true, true});
  ExpectCollapsed(model, {true, false, true, false});
  EXPECT_EQ(model.ListAt(1)->id, std::string("list-1"));
  EXPECT_EQ(model.ListAt(1)->title, std::wstring(L"默认"));
  AssertInvariants(model);

  const auto today = calendar.BlocksForDay("2026-06-22");
  EXPECT_EQ(today.size(), static_cast<size_t>(2));
  EXPECT_EQ(today[0]->id, 5);
  EXPECT_TRUE(today[1]->id != 5);
  EXPECT_EQ(today[1]->endMinute, 661);

  const std::string serialized = StoreFormat::Serialize(model, calendar, geom, ui);
  TodoModel reparsed;
  CalendarModel reparsedCalendar;
  WindowGeometry reparsedGeom;
  UiState reparsedUi;
  EXPECT_TRUE(StoreFormat::Parse(serialized, reparsed, reparsedCalendar, reparsedGeom, reparsedUi));
  EXPECT_EQ(StoreFormat::Serialize(reparsed, reparsedCalendar, reparsedGeom, reparsedUi), serialized);
}

void ThemeIdsViewsAndDockFieldsRejectUnsafeValuesWithoutClobberingExistingState() {
  const std::string tooLong(65, 'a');
  const std::string text = std::string(R"({
    "ui": {
      "themeId": "../paper",
      "lightThemeId": "paper/slash",
      "darkThemeId": ")") + tooLong + R"(",
      "themeMode": "custom",
      "capsuleDockEdge": "top",
      "capsuleDockT": -3.0,
      "calendarDay": "2026/06/22",
      "calendarView": "agenda",
      "activeView": "popup"
    },
    "lists": [ { "id": "inbox", "title": "Inbox", "items": [] } ]
  })";

  TodoModel model;
  CalendarModel calendar;
  WindowGeometry geom;
  UiState ui;
  ui.themeId = "custom.safe";
  ui.lightThemeId = "mint";
  ui.darkThemeId = "sand";
  ui.capsuleDockEdge = "left";
  ui.capsuleDockT = 0.25;
  ui.calendarDay = "2026-06-22";
  ui.calendarView = CalendarViewMode::Week;
  ui.activeView = "calendar";

  EXPECT_TRUE(StoreFormat::Parse(text, model, calendar, geom, ui));
  EXPECT_EQ(ui.themeMode, std::string("custom"));
  EXPECT_EQ(ui.themeId, std::string("custom.safe"));
  EXPECT_EQ(ui.lightThemeId, std::string("mint"));
  EXPECT_EQ(ui.darkThemeId, std::string("sand"));
  EXPECT_EQ(ui.capsuleDockEdge, std::string("left"));
  EXPECT_NEAR(ui.capsuleDockT, 0.0, 0.000001);
  EXPECT_EQ(ui.calendarDay, std::string("2026-06-22"));
  EXPECT_TRUE(ui.calendarView == CalendarViewMode::Week);
  EXPECT_EQ(ui.activeView, std::string("calendar"));
  AssertInvariants(model);
}

void SerializeOmitsMalformedCalendarDayButPreservesValidCalendarView() {
  TodoModel model;
  CalendarModel calendar;
  WindowGeometry geom;
  UiState ui;
  ui.activeView = "calendar";
  ui.calendarDay = "../../bad";
  ui.calendarView = CalendarViewMode::Month;

  const std::string serialized = StoreFormat::Serialize(model, calendar, geom, ui);
  EXPECT_TRUE(serialized.find("calendarDay") == std::string::npos);
  EXPECT_TRUE(serialized.find("\"calendarView\": \"month\"") != std::string::npos);

  TodoModel loaded;
  CalendarModel loadedCalendar;
  WindowGeometry loadedGeom;
  UiState loadedUi;
  EXPECT_TRUE(StoreFormat::Parse(serialized, loaded, loadedCalendar, loadedGeom, loadedUi));
  EXPECT_EQ(loadedUi.activeView, std::string("calendar"));
  EXPECT_TRUE(loadedUi.calendarDay.empty());
  EXPECT_TRUE(loadedUi.calendarView == CalendarViewMode::Month);
}

const TestCase kTests[] = {
    {"RoundTripPreservesRichTodoCalendarGeometryAndUiState", RoundTripPreservesRichTodoCalendarGeometryAndUiState},
    {"InvalidInputsDoNotMutateAlreadyLoadedState", InvalidInputsDoNotMutateAlreadyLoadedState},
    {"BracketDepthGuardIgnoresBracketsInsideStringsAndEscapes", BracketDepthGuardIgnoresBracketsInsideStringsAndEscapes},
    {"ParseNormalizesMessyImportedListsAndCalendarBeforeSerializing", ParseNormalizesMessyImportedListsAndCalendarBeforeSerializing},
    {"ThemeIdsViewsAndDockFieldsRejectUnsafeValuesWithoutClobberingExistingState", ThemeIdsViewsAndDockFieldsRejectUnsafeValuesWithoutClobberingExistingState},
    {"SerializeOmitsMalformedCalendarDayButPreservesValidCalendarView", SerializeOmitsMalformedCalendarDayButPreservesValidCalendarView},
};

} // namespace

int main() { return RunTests("store_format_hardening", kTests); }
