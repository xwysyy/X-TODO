#pragma once

#include "I18n.h"
#include "Theme.h"

#include <windows.h>
#include <functional>
#include <string>

namespace Settings {

struct Host {
    Lang lang = Lang::Zh;
    Theme::ThemeVisual theme;
    bool autostart = false;
    std::wstring backupDir;
    long long backupLastEpoch = 0;
    std::wstring backupStatus;

    std::function<void(Lang)> setLanguage;
    std::function<void(bool)> setAutostart;
    std::function<void(HWND)> chooseBackupFolder;
    std::function<void()> disableBackup;
};

void ShowSettingsWindow(HWND owner, Host& host);

} // namespace Settings
