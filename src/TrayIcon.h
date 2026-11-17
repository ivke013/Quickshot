// Author: Ivan Stojmenovic
#pragma once
#include "framework.h"

class TrayIcon
{
public:
    bool Create(HWND hwnd, HINSTANCE hInst, UINT iconId, UINT callbackMsg);
    void Destroy();
    void ShowBalloon(const wchar_t* title, const wchar_t* text);
    void ShowContextMenu();

private:
    HWND hwnd_ = nullptr;
    NOTIFYICONDATAW nid_{};
    bool created_ = false;
    bool ownsIcon_ = false;
};
