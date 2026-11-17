// Author: Ivan Stojmenovic
#include "framework.h"
#include "HotkeyManager.h"
#include "Settings.h"

void HotkeyManager::Attach(HWND hwnd)
{
    hwnd_ = hwnd;
}

void HotkeyManager::UnregisterAll()
{
    if (!hwnd_)
        return;
    if (captureRegistered_)
    {
        UnregisterHotKey(hwnd_, HOTKEY_ID_CAPTURE);
        captureRegistered_ = false;
    }
    if (fullSaveRegistered_)
    {
        UnregisterHotKey(hwnd_, HOTKEY_ID_FULLSAVE);
        fullSaveRegistered_ = false;
    }
}

void HotkeyManager::ApplyFromSettings()
{
    UnregisterAll();
    if (!hwnd_)
        return;

    const auto& s = Settings::Instance().Data();
    if (s.enableCaptureHotkey)
    {
        captureRegistered_ = RegisterHotKey(hwnd_, HOTKEY_ID_CAPTURE, s.captureModifiers, s.captureVk) != FALSE;
    }
    if (s.enableFullSaveHotkey)
    {
        fullSaveRegistered_ = RegisterHotKey(hwnd_, HOTKEY_ID_FULLSAVE, s.fullSaveModifiers, s.fullSaveVk) != FALSE;
    }
}

static const wchar_t* VkName(UINT vk)
{
    switch (vk)
    {
    case VK_F1: return L"F1";
    case VK_F2: return L"F2";
    case VK_F3: return L"F3";
    case VK_F4: return L"F4";
    case VK_F5: return L"F5";
    case VK_F6: return L"F6";
    case VK_F7: return L"F7";
    case VK_F8: return L"F8";
    case VK_F9: return L"F9";
    case VK_F10: return L"F10";
    case VK_F11: return L"F11";
    case VK_F12: return L"F12";
    case VK_SNAPSHOT: return L"Prnt Scrn";
    case VK_INSERT: return L"Insert";
    case VK_DELETE: return L"Delete";
    case VK_HOME: return L"Home";
    case VK_END: return L"End";
    case VK_PRIOR: return L"Page Up";
    case VK_NEXT: return L"Page Down";
    case VK_SPACE: return L"Space";
    default: break;
    }
    return nullptr;
}

std::wstring HotkeyManager::FormatHotkey(UINT modifiers, UINT vk)
{
    std::wstring s;
    if (modifiers & MOD_CONTROL) s += L"Ctrl + ";
    if (modifiers & MOD_SHIFT) s += L"Shift + ";
    if (modifiers & MOD_ALT) s += L"Alt + ";
    if (modifiers & MOD_WIN) s += L"Win + ";

    if (const wchar_t* named = VkName(vk))
    {
        s += named;
    }
    else if (vk >= 0x30 && vk <= 0x39)
    {
        s += (wchar_t)vk;
    }
    else if (vk >= 0x41 && vk <= 0x5A)
    {
        s += (wchar_t)vk;
    }
    else
    {
        wchar_t buf[32];
        UINT scan = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
        LONG lParam = (LONG)(scan << 16);
        if (GetKeyNameTextW(lParam, buf, 32) > 0)
            s += buf;
        else
        {
            wsprintfW(buf, L"VK_%02X", vk);
            s += buf;
        }
    }
    return s;
}
