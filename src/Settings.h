// Author: Ivan Stojmenovic
#pragma once
#include "framework.h"

struct AppSettings
{
    bool showNotifications = true;
    bool keepSelectedArea = false;
    bool captureCursor = false;
    bool startWithWindows = false;
    int uiLanguage = 0; // UiLang index: En Sr De Fr Es It Ru Hr

    // Formats / save
    int imageFormat = 0;          // 0 = PNG, 1 = JPEG
    int jpegQuality = 90;         // 1..100
    std::wstring fileNamePattern = L"Screenshot_{yyyy}{MM}{dd}_{HH}{mm}{ss}";
    std::wstring saveFolder;      // empty = Pictures\Screenshots

    // Cloud upload
    bool enableUpload = true;
    std::wstring uploadUrl = L"https://0x0.st"; // expects multipart field "file", returns URL
    std::wstring lastUploadUrl;

    // Proxy (upload)
    bool proxyEnabled = false;
    std::wstring proxyHost;
    int proxyPort = 8080;
    std::wstring proxyUser;
    std::wstring proxyPass;

    // Auto-update
    bool checkUpdatesOnStartup = true;
    // settings.ini [Update] UpdateUrl= — HTTPS JSON manifest
    std::wstring updateManifestUrl;
    long long lastUpdateCheck = 0;  // unix time

    bool enableCaptureHotkey = true;
    UINT captureModifiers = MOD_SHIFT;
    UINT captureVk = VK_F11;

    bool enableFullSaveHotkey = true;
    UINT fullSaveModifiers = MOD_SHIFT;
    UINT fullSaveVk = VK_SNAPSHOT;

    RECT lastSelection{ 0, 0, 0, 0 };
    bool hasLastSelection = false;

    COLORREF drawColor = RGB(255, 0, 0);
    float strokeWidth = 2.5f; // 1.0 .. 12.0
    int fontSize = 16;
    bool fontBold = false;
    bool fontItalic = false;
    bool fontUnderline = false;
};

class Settings
{
public:
    static Settings& Instance();

    void Load();
    void Save() const;

    AppSettings& Data() { return data_; }
    const AppSettings& Data() const { return data_; }

    std::wstring SettingsPath() const;
    std::wstring DefaultSaveFolder() const;
    std::wstring BuildFileName(const SYSTEMTIME* st = nullptr) const; // with extension
    const wchar_t* ImageMime() const;
    const wchar_t* ImageExtension() const;

    static bool IsStartWithWindowsEnabled();
    static bool SetStartWithWindows(bool enable);

private:
    Settings() = default;
    AppSettings data_;
    mutable std::wstring path_;
};
