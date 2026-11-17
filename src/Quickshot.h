// Author: Ivan Stojmenovic
#pragma once

#include "resource.h"
#include "Settings.h"
#include "TrayIcon.h"
#include "HotkeyManager.h"
#include "ScreenCapture.h"
#include "Annotation.h"
#include "Exporter.h"
#include "CaptureOverlay.h"
#include "OptionsDialog.h"
#include "AboutDialog.h"

class App
{
public:
    static App& Instance();
    int Run(HINSTANCE hInstance);

    HWND Hwnd() const { return hwnd_; }
    HINSTANCE Hinst() const { return hInst_; }
    TrayIcon& Tray() { return tray_; }
    HotkeyManager& Hotkeys() { return hotkeys_; }

    void StartRegionCapture();
    void InstantFullscreenSave();
    void ShowOptions();
    void ShowAbout();
    void Exit();

private:
    App() = default;
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    HINSTANCE hInst_ = nullptr;
    HWND hwnd_ = nullptr;
    TrayIcon tray_;
    HotkeyManager hotkeys_;
    ULONG_PTR gdiplusToken_ = 0;
    bool capturing_ = false;
};
