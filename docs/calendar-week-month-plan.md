# Calendar Week And Month View Plan

## Context

Current calendar support is a day view. Blocks are stored as independent records
with `day`, `startMinute`, `endMinute`, and `title`. The current UI already has
creation by drag, block move and resize, inline title editing, and separate
start and end time editing.

The next step is to add week and month views while keeping the existing day view
as the detailed editing surface.

## Goals

- Add a calendar view mode state with three modes: `day`, `week`, and `month`.
- Keep calendar as a top-level app view selected from the title bar.
- Reuse existing calendar block storage and preserve `XTODO v4` compatibility.
- Let week view support time-based scanning across seven days.
- Let month view support fast navigation, density scanning, and day selection.
- Keep layout and hit-testing logic Win32-free so contract tests can cover it.
- Preserve current day-view editing behavior as the first editing implementation.

## Non-Goals For The First Version

- No recurring events.
- No multi-day blocks.
- No all-day row.
- No cross-calendar sync.
- No month-cell drag creation.
- No overlapping block packing beyond simple readable stacking.

These features can be added later after week and month navigation are stable.

## Product Shape

The calendar title bar keeps one calendar entry point. Inside the calendar view,
the header owns date navigation and view-mode switching.

The recommended header shape is:

- Previous period button.
- Current period label.
- Next period button.
- Today button.
- Segmented mode control: Day, Week, Month.

Period navigation depends on the active mode:

| Mode | Previous | Next | Today |
| --- | --- | --- | --- |
| Day | Previous day | Next day | Current local day |
| Week | Previous week | Next week | Week containing current local day |
| Month | Previous month | Next month | Month containing current local day |

Clicking a day label or month cell selects that date and switches to day view
only when the click target clearly means drill-in. Changing mode keeps the
selected day inside the new period.

## Data And Storage

### Calendar Blocks

Keep the existing `CalendarBlock` shape:

```cpp
struct CalendarBlock {
    int id;
    std::string day;
    int startMinute;
    int endMinute;
    std::wstring title;
};
```

No storage migration is needed for week and month display because all grouping
can be derived from `day`.

Add model helpers only when call sites need shared semantics:

- `BlocksForRange(startDay, endDay)` returning blocks where `startDay <= day < endDay`.
- `BlocksForMonth(year, month)` only if month logic is repeated in more than
  one place.

The helpers should return pointers or lightweight views and keep sorting by
`day`, `startMinute`, `endMinute`, and `id`.

### UI State

Extend `UiState` with a calendar mode:

```cpp
std::string calendarView = "day"; // day | week | month
```

Serialization should add:

```text
ui calendar_view=day
```

Parsing rules:

- Accept only `day`, `week`, or `month`.
- Missing or invalid value loads as `day`.
- Keep `calendar_day` as the selected anchor date.
- Do not change the existing `active_view` behavior.

Add `store_format_tests` coverage for valid mode, invalid mode, and old files
without the key.

## Date Utilities

Calendar view modes need date arithmetic in one place. Add small helpers near
the current day-key code or in a Win32-free date utility module:

- Parse `YYYY-MM-DD` to a date value.
- Format date value to `YYYY-MM-DD`.
- Offset by day count.
- Get start of week.
- Get month metadata: first day, number of days, leading blanks, trailing blanks.

Week start should be a deliberate product decision. Use Monday as the first day
unless the app already has a locale-specific rule by implementation time. The
choice must be tested in `gui_contract_tests` or a date utility test.

## Layout Modules

The current `CalendarLayout` owns day-view frame, block rectangles, hit-testing,
editing geometry, time parsing, and time formatting. Week and month should keep
the same contract style.

Recommended layout split:

- Keep shared time parsing and formatting in `CalendarLayout`.
- Keep day layout in the current `GuiCalendar::Frame`.
- Add `WeekFrame`, `WeekBlockRect`, `WeekHitResult`, and week hit kinds.
- Add `MonthFrame`, `MonthCellRect`, `MonthHitResult`, and month hit kinds.
- Keep all structs in plain geometry terms, with no HWND, Direct2D, or Win32
  dependencies.

If `CalendarLayout.*` becomes too broad, split into:

- `CalendarDayLayout.*`
- `CalendarWeekLayout.*`
- `CalendarMonthLayout.*`
- `CalendarDate.*`

The split should happen when implementation size makes tests hard to read, not
before.

## Week View

### First Version Behavior

Week view shows seven day columns over one vertical time axis. It should support:

- Scrollable 24-hour timeline.
- Today column highlight.
- Selected day highlight.
- Blocks rendered in their day column at the same vertical scale as day view.
- Click on a day header selects that day.
- Double-click or explicit drill-in on a block opens day view and starts editing.
- Drag creation may be deferred to day view in the first version.

The first week view can use simple block collision handling:

- Non-overlapping blocks use full column width.
- Overlapping blocks share the column width in deterministic order.
- If collision packing is deferred, stack overlapping blocks with clear
  clipping and test that hit-testing still targets the top visible block.

### Week Layout Contracts

Tests should cover:

- Seven columns fit within narrow and normal widths.
- Time axis maps minutes to y coordinates consistently with day view.
- Blocks on different days produce different x ranges.
- Previous and next period controls hit correctly.
- Mode control hit-testing returns the correct target.
- Clicks outside timeline and header do not create blocks.

## Month View

### First Version Behavior

Month view shows a six-row grid. It should support:

- Current month label.
- Previous and next month navigation.
- Today cell highlight.
- Selected day cell highlight.
- Out-of-month leading and trailing days in muted style.
- Per-day block count or up to two compact titles.
- Click on a day cell selects that day.
- Double-click or explicit drill-in opens day view for that day.

Month cells are too small for precise time editing. Editing stays in day view
for the first version.

### Month Layout Contracts

Tests should cover:

- Month grid always has 42 cells.
- Leap-year February is handled.
- Month starting on each weekday maps to the expected cell offset.
- Hit-testing returns the correct day key for in-month and out-of-month cells.
- Dense days render bounded summaries without changing cell geometry.
- Narrow windows keep stable row and column rectangles.

## MainWindow Integration

Add one enum for the calendar mode:

```cpp
enum class CalendarViewMode {
    Day,
    Week,
    Month,
};
```

`MainWindow` should own:

- Current calendar anchor day.
- Current calendar mode.
- Day-view scroll state.
- Week-view scroll state, if separate scroll feels better in practice.
- Current edit block, only active in day view for the first version.

Behavior rules:

- Switching away from day view commits or closes the current editor using the
  existing empty-block cleanup behavior.
- Entering day view from a week or month target selects the clicked day.
- Day-view block editing keeps using the existing three child edit controls.
- Week and month render read-only summaries in the first version.
- The title-bar calendar toggle keeps returning to list view from any calendar
  mode.

## Rendering And Style

Use the existing theme system and calendar block palette. Avoid adding a new
visual system for week and month.

Rendering priorities:

- Stable geometry before richer visuals.
- Clear selected day and today states.
- Text clipping inside cells and columns.
- High-DPI scaling through the existing `dpiScale()` path.
- No child HWND controls in week or month summaries.

Small-window behavior matters because X-TODO can run as a narrow note window.
At narrow widths:

- Week view may reduce header text to weekday abbreviations.
- Month view may show counts instead of titles.
- Mode control labels may use short text after i18n strings are added.

## Testing Plan

Add contract tests before or alongside implementation:

- Date utility tests for week start, month length, and leap year.
- Store format tests for `calendar_view`.
- GUI contract tests for week frame, month frame, and hit-testing.
- GUI contract tests for mode switching targets.
- i18n tests for new labels.

Run the standard test path:

```bash
cmake -S . -B build-tests -DXTODO_BUILD_APP=OFF -DXTODO_BUILD_TESTS=ON
cmake --build build-tests --config Release --parallel
ctest --test-dir build-tests --output-on-failure -C Release
```

On Linux without CMake, use the documented direct `g++` fallback for affected
Win32-free targets. Treat Windows CI as the authority for the native app build.

## Manual Windows Checklist

Manual checks should run on a real Windows build or a CI-produced artifact:

- Toggle list and calendar view.
- Switch Day, Week, and Month repeatedly.
- Navigate previous and next periods in each mode.
- Use Today in each mode.
- Create, edit, move, and resize a block in day view.
- Switch modes while editing a block and confirm empty-block cleanup.
- Drill from week to day on a block and on a day header.
- Drill from month to day on an in-month and out-of-month cell.
- Resize the app to narrow, normal, and wide widths.
- Check high-DPI scaling.
- Check Chinese and English labels.

## Implementation Phases

### Phase 1: Foundation

- Add `CalendarViewMode` and `UiState::calendarView`.
- Parse and serialize `ui calendar_view`.
- Add date utility functions and tests.
- Add calendar mode labels to i18n.
- Add header mode-control hit-test contracts.

Deliverable: the app still behaves like the current day view, but state and
tests are ready for multiple modes.

### Phase 2: Week View

- Add week frame and hit-testing structs.
- Render seven day columns and the shared time axis.
- Render existing blocks grouped by day.
- Implement week previous, next, today, and day selection.
- Implement drill-in from week to day.
- Add week layout and hit-test contract tests.

Deliverable: week view is readable and navigable, with editing delegated to day
view.

### Phase 3: Month View

- Add month frame, 42-cell grid calculation, and hit-testing.
- Render month cells, selected day, today, and muted adjacent-month cells.
- Render per-day count or compact titles.
- Implement month previous, next, today, and day selection.
- Implement drill-in from month to day.
- Add month layout and hit-test contract tests.

Deliverable: month view supports scanning and navigation without editing inside
cells.

### Phase 4: Polish And Hardening

- Tune narrow-window behavior.
- Add deterministic clipping for dense week and month content.
- Verify mode switching while editing.
- Run full tests locally where available.
- Push and verify GitHub Actions for Linux and Windows.
- Perform manual Windows checks on the produced executable.

Deliverable: day, week, and month views share one calendar model and have stable
navigation, layout, and persistence.

## Open Decisions

- Week start: Monday by default.
- Week-view drag creation: defer to day view for the first version.
- Month summary style: block count first, compact titles if space allows.
- Drill-in gesture: single click selects, double click opens day view unless a
  visible button or keyboard shortcut is added.

These decisions should be revisited only when implementation or manual use shows
that the first version blocks normal calendar usage.
