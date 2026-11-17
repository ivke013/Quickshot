// Author: Ivan Stojmenovic
#pragma once
#include "framework.h"

class HotkeyManager
{
public:
    void Attach(HWND hwnd);
    void ApplyFromSettings();
    void UnregisterAll();

    static std::wstring FormatHotkey(UINT modifiers, UINT vk);

private:
    HWND hwnd_ = nullptr;
    bool captureRegistered_ = false;
    bool fullSaveRegistered_ = false;
};
