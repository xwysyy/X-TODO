#include "SettingsWindow.h"
#include "ThemedWindowControls.h"

#include <windowsx.h>
#include <ctime>
#include <string>
#include <utility>
#include <vector>

namespace Settings {
namespace {

constexpr wchar_t kSettingsClass[] = L"XTodoSettingsWindow";
namespace Ui = ThemedWindow;

enum class RowKind { Title, Section, Language, ToggleAutostart, ToggleBackup, Folder, Status, Button, Spacer };
enum class Action { None, LangZh, LangEn, Autostart, BackupToggle, BackupFolder, Close };

struct Row {
    RowKind kind = RowKind::Status;
    std::wstring label;
    std::wstring value;
    int top = 0;
    int height = 0;
};

struct State {
    HWND hwnd = nullptr;
    HWND owner = nullptr;
    Host* host = nullptr;
    std::vector<Row> rows;
    Action hover = Action::None;
    int scroll = 0;
    int contentH = 0;
    int w = 0;
    int h = 0;
    bool done = false;
};

std::wstring FormatBackupTime(long long epoch, Lang lang) {
    if (epoch <= 0) return T(Str::BackupNever, lang);
    std::time_t value = static_cast<std::time_t>(epoch);
    std::tm local{};
    if (localtime_s(&local, &value) != 0) return T(Str::BackupNever, lang);

    wchar_t buf[64]{};
    if (wcsftime(buf, 64, L"%Y-%m-%d %H:%M", &local) == 0)
        return T(Str::BackupNever, lang);
    return buf;
}

void BuildRows(State& s) {
    Host& h = *s.host;
    s.rows.clear();
    auto add = [&](RowKind kind, std::wstring label, std::wstring value = L"") {
        Row row;
        row.kind = kind;
        row.label = std::move(label);
        row.value = std::move(value);
        s.rows.push_back(std::move(row));
    };

    add(RowKind::Title, T(Str::Settings, h.lang));
    add(RowKind::Section, T(Str::SettingsGeneral, h.lang));
    add(RowKind::Language, T(Str::Language, h.lang));
    add(RowKind::ToggleAutostart, T(Str::Autostart, h.lang));

    add(RowKind::Section, T(Str::SettingsDataBackup, h.lang));
    add(RowKind::ToggleBackup, T(Str::AutoBackup, h.lang));
    if (!h.backupDir.empty()) {
        add(RowKind::Folder, T(Str::BackupFolder, h.lang), h.backupDir);
        add(RowKind::Status, T(Str::BackupLast, h.lang), FormatBackupTime(h.backupLastEpoch, h.lang));
        if (!h.backupStatus.empty())
            add(RowKind::Status, L"", h.backupStatus);
    } else {
        add(RowKind::Status, T(Str::BackupLast, h.lang), T(Str::BackupDisabled, h.lang));
        if (!h.backupStatus.empty())
            add(RowKind::Status, L"", h.backupStatus);
    }

    add(RowKind::Spacer, L"");
    add(RowKind::Button, T(Str::SettingsClose, h.lang));
}

void LayoutRows(State& s) {
    const int pad = Ui::Px(s.hwnd, 14);
    const int titleH = Ui::Px(s.hwnd, 36);
    const int sectionH = Ui::Px(s.hwnd, 26);
    const int rowH = Ui::Px(s.hwnd, 34);
    const int statusH = Ui::Px(s.hwnd, 24);
    const int buttonH = Ui::Px(s.hwnd, 34);
    int y = pad;
    for (Row& row : s.rows) {
        row.top = y;
        switch (row.kind) {
            case RowKind::Title: row.height = titleH; break;
            case RowKind::Section: row.height = sectionH; break;
            case RowKind::Button: row.height = buttonH; break;
            case RowKind::Status: row.height = statusH; break;
            case RowKind::Spacer: row.height = Ui::Px(s.hwnd, 8); break;
            default: row.height = rowH; break;
        }
        y += row.height;
    }
    s.contentH = y + pad;
}

void ClampScroll(State& s) {
    int maxScroll = s.contentH - s.h;
    if (maxScroll < 0) maxScroll = 0;
    if (s.scroll < 0) s.scroll = 0;
    if (s.scroll > maxScroll) s.scroll = maxScroll;
}

RECT RowRect(const State& s, const Row& row, int inset = 12) {
    const int x = Ui::Px(s.hwnd, static_cast<float>(inset));
    return RECT{ x, row.top - s.scroll, s.w - x, row.top - s.scroll + row.height };
}

RECT ToggleRect(const State& s, const Row& row) {
    RECT rr = RowRect(s, row);
    const int w = Ui::Px(s.hwnd, 66);
    const int h = Ui::Px(s.hwnd, 24);
    const int right = rr.right;
    const int top = rr.top + (row.height - h) / 2;
    return RECT{ right - w, top, right, top + h };
}

void LanguageRects(const State& s, const Row& row, RECT& zh, RECT& en) {
    RECT rr = RowRect(s, row);
    const int w = Ui::Px(s.hwnd, 154);
    const int h = Ui::Px(s.hwnd, 26);
    const int top = rr.top + (row.height - h) / 2;
    zh = RECT{ rr.right - w, top, rr.right - w / 2, top + h };
    en = RECT{ rr.right - w / 2, top, rr.right, top + h };
}

RECT FolderButtonRect(const State& s, const Row& row) {
    RECT rr = RowRect(s, row);
    const int w = Ui::Px(s.hwnd, 78);
    const int h = Ui::Px(s.hwnd, 26);
    const int top = rr.top + (row.height - h) / 2;
    return RECT{ rr.right - w, top, rr.right, top + h };
}

RECT CloseButtonRect(const State& s, const Row& row) {
    RECT rr = RowRect(s, row);
    const int w = Ui::Px(s.hwnd, 104);
    const int h = Ui::Px(s.hwnd, 28);
    const int left = rr.right - w;
    const int top = rr.top + (row.height - h) / 2;
    return RECT{ left, top, left + w, top + h };
}

bool PtIn(const RECT& rect, int x, int y) {
    POINT pt{ x, y };
    return PtInRect(&rect, pt) != FALSE;
}

Action ActionAt(State& s, int x, int y) {
    const int docY = y + s.scroll;
    for (const Row& row : s.rows) {
        if (docY < row.top || docY >= row.top + row.height) continue;
        switch (row.kind) {
            case RowKind::Language: {
                RECT zh{}, en{};
                LanguageRects(s, row, zh, en);
                if (PtIn(zh, x, y)) return Action::LangZh;
                if (PtIn(en, x, y)) return Action::LangEn;
                return Action::None;
            }
            case RowKind::ToggleAutostart:
                return PtIn(ToggleRect(s, row), x, y) ? Action::Autostart : Action::None;
            case RowKind::ToggleBackup:
                return PtIn(ToggleRect(s, row), x, y) ? Action::BackupToggle : Action::None;
            case RowKind::Folder:
                return PtIn(FolderButtonRect(s, row), x, y) ? Action::BackupFolder : Action::None;
            case RowKind::Button:
                return PtIn(CloseButtonRect(s, row), x, y) ? Action::Close : Action::None;
            default:
                return Action::None;
        }
    }
    return Action::None;
}

void DrawDivider(HDC dc, RECT rect, const Theme::ColorSet& c) {
    RECT line{ rect.left, rect.bottom - 1, rect.right, rect.bottom };
    Ui::FillColor(dc, line, c.divider);
}

void DrawPill(HDC dc, HWND hwnd, RECT rect, const std::wstring& text, bool selected,
              bool hovered, HFONT font, const Theme::ColorSet& c) {
    const uint32_t fill = selected ? c.checkFill : (hovered ? c.buttonHover : c.paper);
    const uint32_t textColor = selected ? c.checkMark : c.text;
    Ui::FillRoundColor(dc, rect, Ui::Px(hwnd, 8), fill);
    Ui::StrokeRoundColor(dc, rect, Ui::Px(hwnd, 8), selected ? c.checkFill : c.paperEdge);
    Ui::DrawTextInRect(dc, text, rect, font, textColor,
                       DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

void DrawToggle(HDC dc, HWND hwnd, RECT rect, bool on, bool hovered, HFONT font,
                const Theme::ColorSet& c, Lang lang) {
    const uint32_t fill = on ? c.checkFill : (hovered ? c.buttonHover : c.paper);
    Ui::FillRoundColor(dc, rect, Ui::Px(hwnd, 12), fill);
    Ui::StrokeRoundColor(dc, rect, Ui::Px(hwnd, 12), on ? c.checkFill : c.paperEdge);

    const int knob = Ui::Px(hwnd, 18);
    const int pad = Ui::Px(hwnd, 3);
    const int knobLeft = on ? rect.right - pad - knob : rect.left + pad;
    RECT k{ knobLeft, rect.top + pad, knobLeft + knob, rect.top + pad + knob };
    Ui::FillRoundColor(dc, k, knob / 2, on ? c.checkMark : c.textWeak);

    RECT textRect = rect;
    if (on) textRect.right = k.left - Ui::Px(hwnd, 2);
    else textRect.left = k.right + Ui::Px(hwnd, 2);
    Ui::DrawTextInRect(dc, on ? T(Str::SettingOn, lang) : T(Str::SettingOff, lang),
                       textRect, font, on ? c.checkMark : c.text,
                       DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

void Paint(State& s) {
    PAINTSTRUCT ps{};
    HDC hdc = BeginPaint(s.hwnd, &ps);
    RECT rc{};
    GetClientRect(s.hwnd, &rc);

    HDC mem = CreateCompatibleDC(hdc);
    HBITMAP bmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
    HGDIOBJ oldBmp = SelectObject(mem, bmp);

    const Theme::ColorSet& c = s.host->theme.colors;
    Ui::FillColor(mem, rc, c.paperElevated);

    HFONT titleFont = Ui::CreateTextFont(s.hwnd, 14.0f, true);
    HFONT sectionFont = Ui::CreateTextFont(s.hwnd, 10.5f, true);
    HFONT rowFont = Ui::CreateTextFont(s.hwnd, 10.5f, false);
    HFONT smallFont = Ui::CreateTextFont(s.hwnd, 9.2f, false);

    for (const Row& row : s.rows) {
        const int top = row.top - s.scroll;
        if (top + row.height < 0 || top > rc.bottom) continue;
        RECT rr = RowRect(s, row);
        switch (row.kind) {
            case RowKind::Title:
                Ui::DrawTextInRect(mem, row.label, rr, titleFont, c.text,
                                   DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                break;
            case RowKind::Section:
                Ui::DrawTextInRect(mem, row.label, rr, sectionFont, c.textWeak,
                                   DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                break;
            case RowKind::Language: {
                Ui::DrawTextInRect(mem, row.label, rr, rowFont, c.text,
                                   DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                RECT zh{}, en{};
                LanguageRects(s, row, zh, en);
                DrawPill(mem, s.hwnd, zh, L"中文", s.host->lang == Lang::Zh,
                         s.hover == Action::LangZh, smallFont, c);
                DrawPill(mem, s.hwnd, en, L"English", s.host->lang == Lang::En,
                         s.hover == Action::LangEn, smallFont, c);
                DrawDivider(mem, rr, c);
                break;
            }
            case RowKind::ToggleAutostart:
            case RowKind::ToggleBackup: {
                const bool on = row.kind == RowKind::ToggleAutostart
                              ? s.host->autostart
                              : !s.host->backupDir.empty();
                const Action act = row.kind == RowKind::ToggleAutostart
                                 ? Action::Autostart
                                 : Action::BackupToggle;
                Ui::DrawTextInRect(mem, row.label, rr, rowFont, c.text,
                                   DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                DrawToggle(mem, s.hwnd, ToggleRect(s, row), on, s.hover == act,
                           smallFont, c, s.host->lang);
                DrawDivider(mem, rr, c);
                break;
            }
            case RowKind::Folder: {
                Ui::DrawTextInRect(mem, row.label, rr, rowFont, c.text,
                                   DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                RECT btn = FolderButtonRect(s, row);
                RECT pathRect{ rr.left + Ui::Px(s.hwnd, 86), rr.top, btn.left - Ui::Px(s.hwnd, 8), rr.bottom };
                std::wstring shown = Ui::ElideMiddle(mem, smallFont, row.value,
                                                     pathRect.right - pathRect.left);
                Ui::DrawTextInRect(mem, shown, pathRect, smallFont, c.textWeak,
                                   DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                DrawPill(mem, s.hwnd, btn, T(Str::BackupChangeFolder, s.host->lang),
                         false, s.hover == Action::BackupFolder, smallFont, c);
                DrawDivider(mem, rr, c);
                break;
            }
            case RowKind::Status: {
                if (!row.label.empty()) {
                    RECT labelRect = rr;
                    labelRect.right = rr.left + Ui::Px(s.hwnd, 104);
                    Ui::DrawTextInRect(mem, row.label, labelRect, smallFont, c.textWeak,
                                       DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                    RECT valueRect{ labelRect.right, rr.top, rr.right, rr.bottom };
                    Ui::DrawTextInRect(mem, row.value, valueRect, smallFont, c.textWeak,
                                       DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                } else {
                    Ui::DrawTextInRect(mem, row.value, rr, smallFont, c.textWeak,
                                       DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                }
                break;
            }
            case RowKind::Button: {
                RECT btn = CloseButtonRect(s, row);
                DrawPill(mem, s.hwnd, btn, row.label, false, s.hover == Action::Close, rowFont, c);
                break;
            }
            default:
                break;
        }
    }

    Ui::StrokeRoundColor(mem, RECT{ 0, 0, rc.right, rc.bottom }, Ui::Px(s.hwnd, 10), c.paperEdge);

    BitBlt(hdc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
    DeleteObject(titleFont);
    DeleteObject(sectionFont);
    DeleteObject(rowFont);
    DeleteObject(smallFont);
    SelectObject(mem, oldBmp);
    DeleteObject(bmp);
    DeleteDC(mem);
    EndPaint(s.hwnd, &ps);
}

void Rebuild(State& s) {
    BuildRows(s);
    LayoutRows(s);
    ClampScroll(s);
    InvalidateRect(s.hwnd, nullptr, FALSE);
}

void DoAction(State& s, Action action) {
    Host& h = *s.host;
    switch (action) {
        case Action::LangZh:
            if (h.setLanguage) h.setLanguage(Lang::Zh);
            break;
        case Action::LangEn:
            if (h.setLanguage) h.setLanguage(Lang::En);
            break;
        case Action::Autostart:
            if (h.setAutostart) h.setAutostart(!h.autostart);
            break;
        case Action::BackupToggle:
            if (h.backupDir.empty()) {
                if (h.chooseBackupFolder) h.chooseBackupFolder(s.hwnd);
            } else {
                if (h.disableBackup) h.disableBackup();
            }
            break;
        case Action::BackupFolder:
            if (h.chooseBackupFolder) h.chooseBackupFolder(s.hwnd);
            break;
        case Action::Close:
            s.done = true;
            DestroyWindow(s.hwnd);
            return;
        default:
            return;
    }
    Rebuild(s);
}

LRESULT CALLBACK SettingsProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    State* s = reinterpret_cast<State*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_NCCREATE) {
        auto cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        s = static_cast<State*>(cs->lpCreateParams);
        s->hwnd = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(s));
        return TRUE;
    }
    if (!s) return DefWindowProcW(hwnd, msg, wp, lp);

    switch (msg) {
        case WM_PAINT:
            Paint(*s);
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_MOUSEMOVE: {
            TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&tme);
            Action hit = ActionAt(*s, GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            if (hit != s->hover) {
                s->hover = hit;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
        case WM_MOUSELEAVE:
            if (s->hover != Action::None) {
                s->hover = Action::None;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        case WM_MOUSEWHEEL: {
            int delta = GET_WHEEL_DELTA_WPARAM(wp);
            s->scroll -= (delta / 120) * Ui::Px(hwnd, 36);
            ClampScroll(*s);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        case WM_LBUTTONUP:
            DoAction(*s, ActionAt(*s, GET_X_LPARAM(lp), GET_Y_LPARAM(lp)));
            return 0;
        case WM_KEYDOWN:
            if (wp == VK_ESCAPE) {
                s->done = true;
                DestroyWindow(hwnd);
            }
            return 0;
        case WM_CLOSE:
            s->done = true;
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            s->done = true;
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

bool RegisterSettingsClass() {
    WNDCLASSEXW wc{ sizeof(wc) };
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = SettingsProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kSettingsClass;
    if (RegisterClassExW(&wc)) return true;
    return GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

} // namespace

void ShowSettingsWindow(HWND owner, Host& host) {
    if (!RegisterSettingsClass()) return;

    State state{};
    state.owner = owner;
    state.host = &host;

    UINT dpi = owner ? GetDpiForWindow(owner) : 96;
    state.w = MulDiv(360, dpi, 96);
    state.h = MulDiv(340, dpi, 96);

    int x = MulDiv(120, dpi, 96);
    int y = MulDiv(80, dpi, 96);
    HMONITOR mon = MonitorFromWindow(owner ? owner : GetDesktopWindow(), MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{ sizeof(mi) };
    if (mon && GetMonitorInfoW(mon, &mi)) {
        x = mi.rcWork.left + ((mi.rcWork.right - mi.rcWork.left) - state.w) / 2;
        y = mi.rcWork.top + ((mi.rcWork.bottom - mi.rcWork.top) - state.h) / 2;
    }

    HWND hwnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, kSettingsClass,
                                L"X-TODO", WS_POPUP | WS_CLIPCHILDREN,
                                x, y, state.w, state.h,
                                owner, nullptr, GetModuleHandleW(nullptr), &state);
    if (!hwnd) return;

    BuildRows(state);
    LayoutRows(state);
    ClampScroll(state);

    if (owner) EnableWindow(owner, FALSE);
    ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);

    MSG msg{};
    BOOL got = TRUE;
    while (!state.done && (got = GetMessageW(&msg, nullptr, 0, 0)) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (got == 0) PostQuitMessage(static_cast<int>(msg.wParam));

    if (IsWindow(hwnd)) DestroyWindow(hwnd);
    if (owner) {
        EnableWindow(owner, TRUE);
        SetForegroundWindow(owner);
    }
}

} // namespace Settings
