// Author: Ivan Stojmenovic
#pragma once
#include "framework.h"

class HotkeyManager;

class OptionsDialog
{
public:
    static void Show(HWND owner, HINSTANCE hInst, HotkeyManager* hotkeys);
};
