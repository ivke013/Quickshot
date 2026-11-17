// Author: Ivan Stojmenovic
#pragma once
#include "framework.h"

struct UpdateInfo
{
    std::wstring version;
    std::wstring url;
    std::wstring notes;
};

class UpdateChecker
{
public:
    // Returns 1 if a>b, 0 if equal, -1 if a<b. Missing parts treated as 0.
    static int CompareVersions(const wchar_t* a, const wchar_t* b);

    // Fetches manifest JSON from Settings updateManifestUrl.
    static bool FetchManifest(UpdateInfo* out, std::wstring* outError);

    // true if remote version is newer than APP_VERSION_W.
    static bool IsNewer(const UpdateInfo& info);

    // Interactive check (MessageBox). Returns true if update was started (app should exit).
    static bool CheckInteractive(HWND owner);

    // Quiet check for startup; shows offer only if update exists.
    static void CheckQuietAsync(HWND owner);

    // Download package and schedule replace+restart. App must exit after success.
    static bool DownloadAndApply(HWND owner, const UpdateInfo& info, std::wstring* outError);

private:
    static bool HttpGet(const std::wstring& url, std::vector<BYTE>* outBody, std::wstring* outError);
    static bool HttpDownloadFile(const std::wstring& url, const std::wstring& path, std::wstring* outError);
    static bool ParseManifest(const std::string& body, UpdateInfo* out);
    static std::wstring JsonString(const std::string& body, const char* key);
};
