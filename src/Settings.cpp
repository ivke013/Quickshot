// Author: Ivan Stojmenovic
#include "framework.h"
#include "Settings.h"
#include "Loc.h"

Settings& Settings::Instance()
{
    static Settings s;
    return s;
}

std::wstring Settings::SettingsPath() const
{
    if (!path_.empty())
        return path_;

    // settings.ini lives next to the EXE (install / portable folder).
    wchar_t module[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, module, MAX_PATH);
    std::wstring dir = module;
    const size_t slash = dir.find_last_of(L"\\/");
    if (slash != std::wstring::npos)
        dir.resize(slash);
    else
        dir = L".";

    const std::wstring localIni = dir + L"\\settings.ini";

    // One-time migrate from older %APPDATA%\MyScreenShot\settings.ini if present.
    if (GetFileAttributesW(localIni.c_str()) == INVALID_FILE_ATTRIBUTES)
    {
        wchar_t appData[MAX_PATH] = {};
        if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appData)))
        {
            const std::wstring legacy = std::wstring(appData) + L"\\MyScreenShot\\settings.ini";
            if (GetFileAttributesW(legacy.c_str()) != INVALID_FILE_ATTRIBUTES)
                CopyFileW(legacy.c_str(), localIni.c_str(), TRUE);
        }
    }

    path_ = localIni;
    return path_;
}

std::wstring Settings::DefaultSaveFolder() const
{
    if (!data_.saveFolder.empty())
    {
        CreateDirectoryW(data_.saveFolder.c_str(), nullptr);
        DWORD attrs = GetFileAttributesW(data_.saveFolder.c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY))
            return data_.saveFolder;
    }

    wchar_t pictures[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_MYPICTURES, nullptr, 0, pictures)))
    {
        std::wstring dir = std::wstring(pictures) + L"\\Screenshots";
        CreateDirectoryW(dir.c_str(), nullptr);
        return dir;
    }
    return L".";
}

static int ReadInt(const wchar_t* path, const wchar_t* section, const wchar_t* key, int def)
{
    return GetPrivateProfileIntW(section, key, def, path);
}

static void WriteInt(const wchar_t* path, const wchar_t* section, const wchar_t* key, int value)
{
    wchar_t buf[32];
    wsprintfW(buf, L"%d", value);
    WritePrivateProfileStringW(section, key, buf, path);
}

static std::wstring ReadString(const wchar_t* path, const wchar_t* section, const wchar_t* key, const wchar_t* def)
{
    wchar_t buf[512] = {};
    GetPrivateProfileStringW(section, key, def, buf, 512, path);
    return buf;
}

static void WriteString(const wchar_t* path, const wchar_t* section, const wchar_t* key, const wchar_t* value)
{
    WritePrivateProfileStringW(section, key, value, path);
}

static void ReplaceAll(std::wstring& s, const std::wstring& from, const std::wstring& to)
{
    if (from.empty())
        return;
    size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::wstring::npos)
    {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
}

const wchar_t* Settings::ImageMime() const
{
    return data_.imageFormat == 1 ? L"image/jpeg" : L"image/png";
}

const wchar_t* Settings::ImageExtension() const
{
    return data_.imageFormat == 1 ? L".jpg" : L".png";
}

std::wstring Settings::BuildFileName(const SYSTEMTIME* st) const
{
    SYSTEMTIME local{};
    if (!st)
    {
        GetLocalTime(&local);
        st = &local;
    }

    std::wstring pattern = data_.fileNamePattern.empty()
        ? L"Screenshot_{yyyy}{MM}{dd}_{HH}{mm}{ss}"
        : data_.fileNamePattern;

    wchar_t yyyy[8], MM[8], dd[8], HH[8], mm[8], ss[8];
    wsprintfW(yyyy, L"%04d", st->wYear);
    wsprintfW(MM, L"%02d", st->wMonth);
    wsprintfW(dd, L"%02d", st->wDay);
    wsprintfW(HH, L"%02d", st->wHour);
    wsprintfW(mm, L"%02d", st->wMinute);
    wsprintfW(ss, L"%02d", st->wSecond);

    ReplaceAll(pattern, L"{yyyy}", yyyy);
    ReplaceAll(pattern, L"{MM}", MM);
    ReplaceAll(pattern, L"{dd}", dd);
    ReplaceAll(pattern, L"{HH}", HH);
    ReplaceAll(pattern, L"{mm}", mm);
    ReplaceAll(pattern, L"{ss}", ss);

    // Strip path separators from filename.
    for (auto& c : pattern)
    {
        if (c == L'\\' || c == L'/' || c == L':' || c == L'*' || c == L'?' || c == L'"' || c == L'<' || c == L'>' || c == L'|')
            c = L'_';
    }

    const wchar_t* ext = ImageExtension();
    const size_t el = wcslen(ext);
    if (pattern.size() < el || _wcsicmp(pattern.c_str() + pattern.size() - el, ext) != 0)
        pattern += ext;
    return pattern;
}

namespace
{
    constexpr wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    constexpr wchar_t kRunValue[] = L"Quickshot";
    constexpr wchar_t kLegacyRunValue[] = L"MyScreenShot";

    std::wstring QuotedExePath()
    {
        wchar_t path[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, path, MAX_PATH);
        return std::wstring(L"\"") + path + L"\"";
    }

    void MigrateRunKey()
    {
        HKEY key = nullptr;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_QUERY_VALUE | KEY_SET_VALUE, &key) != ERROR_SUCCESS)
            return;

        DWORD type = 0;
        DWORD size = 0;
        const bool hasNew = RegQueryValueExW(key, kRunValue, nullptr, &type, nullptr, &size) == ERROR_SUCCESS
            && type == REG_SZ && size > sizeof(wchar_t);

        wchar_t oldBuf[MAX_PATH + 4] = {};
        type = 0;
        size = sizeof(oldBuf);
        const bool hasOld = RegQueryValueExW(key, kLegacyRunValue, nullptr, &type,
            reinterpret_cast<LPBYTE>(oldBuf), &size) == ERROR_SUCCESS
            && type == REG_SZ && oldBuf[0] != 0;

        if (hasOld && !hasNew)
        {
            const std::wstring exe = QuotedExePath();
            RegSetValueExW(key, kRunValue, 0, REG_SZ,
                reinterpret_cast<const BYTE*>(exe.c_str()),
                static_cast<DWORD>((exe.size() + 1) * sizeof(wchar_t)));
        }
        if (hasOld)
            RegDeleteValueW(key, kLegacyRunValue);

        RegCloseKey(key);
    }
}

bool Settings::IsStartWithWindowsEnabled()
{
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_READ, &key) != ERROR_SUCCESS)
        return false;

    wchar_t buf[MAX_PATH + 4] = {};
    DWORD type = 0;
    DWORD size = sizeof(buf);
    const LONG rc = RegQueryValueExW(key, kRunValue, nullptr, &type, reinterpret_cast<LPBYTE>(buf), &size);
    RegCloseKey(key);
    return rc == ERROR_SUCCESS && type == REG_SZ && buf[0] != 0;
}

bool Settings::SetStartWithWindows(bool enable)
{
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_SET_VALUE | KEY_QUERY_VALUE, &key) != ERROR_SUCCESS)
        return false;

    LONG rc = ERROR_SUCCESS;
    if (enable)
    {
        const std::wstring exe = QuotedExePath();
        rc = RegSetValueExW(key, kRunValue, 0, REG_SZ,
            reinterpret_cast<const BYTE*>(exe.c_str()),
            (DWORD)((exe.size() + 1) * sizeof(wchar_t)));
        RegDeleteValueW(key, kLegacyRunValue);
    }
    else
    {
        rc = RegDeleteValueW(key, kRunValue);
        if (rc == ERROR_FILE_NOT_FOUND)
            rc = ERROR_SUCCESS;
        RegDeleteValueW(key, kLegacyRunValue);
    }
    RegCloseKey(key);
    return rc == ERROR_SUCCESS;
}

void Settings::Load()
{
    MigrateRunKey();
    path_ = SettingsPath();
    const wchar_t* p = path_.c_str();

    data_.showNotifications = ReadInt(p, L"General", L"ShowNotifications", 1) != 0;
    data_.keepSelectedArea = ReadInt(p, L"General", L"KeepSelectedArea", 0) != 0;
    data_.captureCursor = ReadInt(p, L"General", L"CaptureCursor", 0) != 0;
    // Registry is the source of truth for autostart.
    data_.startWithWindows = IsStartWithWindowsEnabled();
    data_.uiLanguage = ReadInt(p, L"General", L"UiLanguage", 0);
    if (data_.uiLanguage < 0 || data_.uiLanguage >= Loc::LanguageCount())
        data_.uiLanguage = 0;
    Loc::SetLanguage(Loc::FromIndex(data_.uiLanguage));

    data_.enableCaptureHotkey = ReadInt(p, L"Hotkeys", L"EnableCapture", 1) != 0;
    data_.captureModifiers = (UINT)ReadInt(p, L"Hotkeys", L"CaptureModifiers", MOD_SHIFT);
    data_.captureVk = (UINT)ReadInt(p, L"Hotkeys", L"CaptureVk", VK_F11);

    data_.enableFullSaveHotkey = ReadInt(p, L"Hotkeys", L"EnableFullSave", 1) != 0;
    data_.fullSaveModifiers = (UINT)ReadInt(p, L"Hotkeys", L"FullSaveModifiers", MOD_SHIFT);
    data_.fullSaveVk = (UINT)ReadInt(p, L"Hotkeys", L"FullSaveVk", VK_SNAPSHOT);

    data_.lastSelection.left = ReadInt(p, L"Selection", L"Left", 0);
    data_.lastSelection.top = ReadInt(p, L"Selection", L"Top", 0);
    data_.lastSelection.right = ReadInt(p, L"Selection", L"Right", 0);
    data_.lastSelection.bottom = ReadInt(p, L"Selection", L"Bottom", 0);
    data_.hasLastSelection = ReadInt(p, L"Selection", L"HasLast", 0) != 0;

    data_.drawColor = (COLORREF)ReadInt(p, L"Draw", L"Color", (int)RGB(255, 0, 0));
    {
        int tenths = ReadInt(p, L"Draw", L"StrokeWidth", 25); // 25 = 2.5
        if (tenths < 10) tenths = 10;
        if (tenths > 120) tenths = 120;
        data_.strokeWidth = tenths / 10.0f;
    }
    data_.fontSize = ReadInt(p, L"Draw", L"FontSize", 16);
    data_.fontBold = ReadInt(p, L"Draw", L"Bold", 0) != 0;
    data_.fontItalic = ReadInt(p, L"Draw", L"Italic", 0) != 0;
    data_.fontUnderline = ReadInt(p, L"Draw", L"Underline", 0) != 0;

    data_.imageFormat = ReadInt(p, L"Formats", L"ImageFormat", 0);
    if (data_.imageFormat != 0 && data_.imageFormat != 1)
        data_.imageFormat = 0;
    data_.jpegQuality = ReadInt(p, L"Formats", L"JpegQuality", 90);
    if (data_.jpegQuality < 1) data_.jpegQuality = 1;
    if (data_.jpegQuality > 100) data_.jpegQuality = 100;
    data_.fileNamePattern = ReadString(p, L"Formats", L"FileNamePattern",
        L"Screenshot_{yyyy}{MM}{dd}_{HH}{mm}{ss}");
    if (data_.fileNamePattern.empty())
        data_.fileNamePattern = L"Screenshot_{yyyy}{MM}{dd}_{HH}{mm}{ss}";
    data_.saveFolder = ReadString(p, L"Formats", L"SaveFolder", L"");

    data_.enableUpload = ReadInt(p, L"Cloud", L"EnableUpload", 1) != 0;
    data_.uploadUrl = ReadString(p, L"Cloud", L"UploadUrl", L"https://0x0.st");
    if (data_.uploadUrl.empty())
        data_.uploadUrl = L"https://0x0.st";
    data_.lastUploadUrl = ReadString(p, L"Cloud", L"LastUploadUrl", L"");

    data_.proxyEnabled = ReadInt(p, L"Proxy", L"Enabled", 0) != 0;
    data_.proxyHost = ReadString(p, L"Proxy", L"Host", L"");
    data_.proxyPort = ReadInt(p, L"Proxy", L"Port", 8080);
    if (data_.proxyPort < 1) data_.proxyPort = 1;
    if (data_.proxyPort > 65535) data_.proxyPort = 65535;
    data_.proxyUser = ReadString(p, L"Proxy", L"User", L"");
    data_.proxyPass = ReadString(p, L"Proxy", L"Pass", L"");

    data_.checkUpdatesOnStartup = ReadInt(p, L"Update", L"CheckOnStartup", 1) != 0;
    // Prefer UpdateUrl; keep ManifestUrl as legacy fallback.
    data_.updateManifestUrl = ReadString(p, L"Update", L"UpdateUrl", L"");
    if (data_.updateManifestUrl.empty())
        data_.updateManifestUrl = ReadString(p, L"Update", L"ManifestUrl", L"");
    data_.lastUpdateCheck = (long long)ReadInt(p, L"Update", L"LastCheck", 0);

    // Ensure [Update] keys exist in settings.ini so they can be edited by hand.
    WriteInt(p, L"Update", L"CheckOnStartup", data_.checkUpdatesOnStartup ? 1 : 0);
    WriteString(p, L"Update", L"UpdateUrl", data_.updateManifestUrl.c_str());
    WriteInt(p, L"Update", L"LastCheck", (int)data_.lastUpdateCheck);
}

void Settings::Save() const
{
    const wchar_t* p = SettingsPath().c_str();

    WriteInt(p, L"General", L"ShowNotifications", data_.showNotifications ? 1 : 0);
    WriteInt(p, L"General", L"KeepSelectedArea", data_.keepSelectedArea ? 1 : 0);
    WriteInt(p, L"General", L"CaptureCursor", data_.captureCursor ? 1 : 0);
    WriteInt(p, L"General", L"StartWithWindows", data_.startWithWindows ? 1 : 0);
    WriteInt(p, L"General", L"UiLanguage", data_.uiLanguage);
    SetStartWithWindows(data_.startWithWindows);
    Loc::SetLanguage(Loc::FromIndex(data_.uiLanguage));

    WriteInt(p, L"Hotkeys", L"EnableCapture", data_.enableCaptureHotkey ? 1 : 0);
    WriteInt(p, L"Hotkeys", L"CaptureModifiers", (int)data_.captureModifiers);
    WriteInt(p, L"Hotkeys", L"CaptureVk", (int)data_.captureVk);

    WriteInt(p, L"Hotkeys", L"EnableFullSave", data_.enableFullSaveHotkey ? 1 : 0);
    WriteInt(p, L"Hotkeys", L"FullSaveModifiers", (int)data_.fullSaveModifiers);
    WriteInt(p, L"Hotkeys", L"FullSaveVk", (int)data_.fullSaveVk);

    WriteInt(p, L"Selection", L"Left", data_.lastSelection.left);
    WriteInt(p, L"Selection", L"Top", data_.lastSelection.top);
    WriteInt(p, L"Selection", L"Right", data_.lastSelection.right);
    WriteInt(p, L"Selection", L"Bottom", data_.lastSelection.bottom);
    WriteInt(p, L"Selection", L"HasLast", data_.hasLastSelection ? 1 : 0);

    WriteInt(p, L"Draw", L"Color", (int)data_.drawColor);
    WriteInt(p, L"Draw", L"StrokeWidth", (int)std::lround(data_.strokeWidth * 10.0f));
    WriteInt(p, L"Draw", L"FontSize", data_.fontSize);
    WriteInt(p, L"Draw", L"Bold", data_.fontBold ? 1 : 0);
    WriteInt(p, L"Draw", L"Italic", data_.fontItalic ? 1 : 0);
    WriteInt(p, L"Draw", L"Underline", data_.fontUnderline ? 1 : 0);

    WriteInt(p, L"Formats", L"ImageFormat", data_.imageFormat);
    WriteInt(p, L"Formats", L"JpegQuality", data_.jpegQuality);
    WriteString(p, L"Formats", L"FileNamePattern", data_.fileNamePattern.c_str());
    WriteString(p, L"Formats", L"SaveFolder", data_.saveFolder.c_str());

    WriteInt(p, L"Cloud", L"EnableUpload", data_.enableUpload ? 1 : 0);
    WriteString(p, L"Cloud", L"UploadUrl", data_.uploadUrl.c_str());
    WriteString(p, L"Cloud", L"LastUploadUrl", data_.lastUploadUrl.c_str());

    WriteInt(p, L"Proxy", L"Enabled", data_.proxyEnabled ? 1 : 0);
    WriteString(p, L"Proxy", L"Host", data_.proxyHost.c_str());
    WriteInt(p, L"Proxy", L"Port", data_.proxyPort);
    WriteString(p, L"Proxy", L"User", data_.proxyUser.c_str());
    WriteString(p, L"Proxy", L"Pass", data_.proxyPass.c_str());

    WriteInt(p, L"Update", L"CheckOnStartup", data_.checkUpdatesOnStartup ? 1 : 0);
    WriteString(p, L"Update", L"UpdateUrl", data_.updateManifestUrl.c_str());
    WriteInt(p, L"Update", L"LastCheck", (int)data_.lastUpdateCheck);
}
