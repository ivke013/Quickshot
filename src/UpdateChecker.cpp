// Author: Ivan Stojmenovic
#include "framework.h"
#include "UpdateChecker.h"
#include "Settings.h"
#include "Loc.h"
#include "Resource.h"

#include <winhttp.h>
#include <ctime>
#pragma comment(lib, "winhttp.lib")

namespace
{
    void ApplyProxy(HINTERNET session)
    {
        if (!session)
            return;
        const auto& s = Settings::Instance().Data();
        if (!s.proxyEnabled || s.proxyHost.empty())
            return;

        wchar_t portBuf[16] = {};
        wsprintfW(portBuf, L"%d", s.proxyPort);
        std::wstring proxyServer = s.proxyHost + L":" + portBuf;

        WINHTTP_PROXY_INFO info{};
        info.dwAccessType = WINHTTP_ACCESS_TYPE_NAMED_PROXY;
        info.lpszProxy = const_cast<LPWSTR>(proxyServer.c_str());
        info.lpszProxyBypass = const_cast<LPWSTR>(L"<local>");
        WinHttpSetOption(session, WINHTTP_OPTION_PROXY, &info, sizeof(info));
    }

    bool ParseUrl(const std::wstring& url, bool* https, std::wstring* host, INTERNET_PORT* port, std::wstring* path)
    {
        URL_COMPONENTSW uc{};
        uc.dwStructSize = sizeof(uc);
        wchar_t hostBuf[256] = {};
        wchar_t pathBuf[2048] = {};
        uc.lpszHostName = hostBuf;
        uc.dwHostNameLength = 256;
        uc.lpszUrlPath = pathBuf;
        uc.dwUrlPathLength = 2048;
        if (!WinHttpCrackUrl(url.c_str(), 0, 0, &uc))
            return false;
        *https = (uc.nScheme == INTERNET_SCHEME_HTTPS);
        *host = hostBuf;
        *port = uc.nPort;
        *path = pathBuf;
        if (path->empty())
            *path = L"/";
        return !host->empty();
    }

    std::wstring Utf8ToWide(const std::string& s)
    {
        if (s.empty())
            return {};
        const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
        if (n <= 0)
            return std::wstring(s.begin(), s.end());
        std::wstring out(n, 0);
        MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), out.data(), n);
        return out;
    }

    struct QuietCtx
    {
        HWND owner = nullptr;
    };
}

int UpdateChecker::CompareVersions(const wchar_t* a, const wchar_t* b)
{
    auto next = [](const wchar_t*& p) -> int {
        while (*p && (*p < L'0' || *p > L'9'))
            ++p;
        if (!*p)
            return 0;
        int v = 0;
        while (*p >= L'0' && *p <= L'9')
        {
            v = v * 10 + (*p - L'0');
            ++p;
        }
        return v;
    };

    const wchar_t* pa = a ? a : L"0";
    const wchar_t* pb = b ? b : L"0";
    for (int i = 0; i < 4; ++i)
    {
        const int va = next(pa);
        const int vb = next(pb);
        if (va > vb) return 1;
        if (va < vb) return -1;
    }
    return 0;
}

std::wstring UpdateChecker::JsonString(const std::string& body, const char* key)
{
    std::string needle = std::string("\"") + key + "\"";
    size_t pos = body.find(needle);
    if (pos == std::string::npos)
        return {};
    pos = body.find(':', pos + needle.size());
    if (pos == std::string::npos)
        return {};
    pos = body.find('"', pos + 1);
    if (pos == std::string::npos)
        return {};
    size_t end = pos + 1;
    while (end < body.size())
    {
        if (body[end] == '\\' && end + 1 < body.size())
        {
            end += 2;
            continue;
        }
        if (body[end] == '"')
            break;
        ++end;
    }
    if (end >= body.size())
        return {};
    std::string raw = body.substr(pos + 1, end - pos - 1);
    std::string unesc;
    unesc.reserve(raw.size());
    for (size_t i = 0; i < raw.size(); ++i)
    {
        if (raw[i] == '\\' && i + 1 < raw.size())
        {
            unesc.push_back(raw[i + 1]);
            ++i;
        }
        else
            unesc.push_back(raw[i]);
    }
    return Utf8ToWide(unesc);
}

bool UpdateChecker::ParseManifest(const std::string& body, UpdateInfo* out)
{
    if (!out)
        return false;
    *out = {};
    out->version = JsonString(body, "version");
    out->url = JsonString(body, "url");
    out->notes = JsonString(body, "notes");
    if (out->notes.empty())
        out->notes = JsonString(body, "changelog");

    // Fallback: first non-empty line = version, second = url
    if (out->version.empty() || out->url.empty())
    {
        std::wstring wide = Utf8ToWide(body);
        std::wstring line1, line2;
        size_t i = 0;
        auto readLine = [&](std::wstring& dst) {
            while (i < wide.size() && (wide[i] == L'\r' || wide[i] == L'\n' || wide[i] == L' ' || wide[i] == L'\t'))
                ++i;
            size_t start = i;
            while (i < wide.size() && wide[i] != L'\r' && wide[i] != L'\n')
                ++i;
            dst = wide.substr(start, i - start);
        };
        readLine(line1);
        readLine(line2);
        if (out->version.empty())
            out->version = line1;
        if (out->url.empty())
            out->url = line2;
    }

    return !out->version.empty() && !out->url.empty();
}

bool UpdateChecker::HttpGet(const std::wstring& url, std::vector<BYTE>* outBody, std::wstring* outError)
{
    if (outBody) outBody->clear();
    bool https = true;
    std::wstring host, path;
    INTERNET_PORT port = INTERNET_DEFAULT_HTTPS_PORT;
    if (!ParseUrl(url, &https, &host, &port, &path))
    {
        if (outError) *outError = Loc::T(StrId::UpdateBadUrl);
        return false;
    }

    const auto& s = Settings::Instance().Data();
    DWORD access = WINHTTP_ACCESS_TYPE_DEFAULT_PROXY;
    std::wstring proxyServer;
    LPCWSTR proxyName = WINHTTP_NO_PROXY_NAME;
    LPCWSTR bypass = WINHTTP_NO_PROXY_BYPASS;
    if (s.proxyEnabled)
    {
        if (!s.proxyHost.empty())
        {
            wchar_t portBuf[16] = {};
            wsprintfW(portBuf, L"%d", s.proxyPort);
            proxyServer = s.proxyHost + L":" + portBuf;
            access = WINHTTP_ACCESS_TYPE_NAMED_PROXY;
            proxyName = proxyServer.c_str();
            bypass = L"<local>";
        }
        else
            access = WINHTTP_ACCESS_TYPE_NO_PROXY;
    }

    HINTERNET session = WinHttpOpen(L"Quickshot/" APP_VERSION_W, access, proxyName, bypass, 0);
    if (!session)
    {
        if (outError) *outError = Loc::T(StrId::UpdateFailed);
        return false;
    }
    ApplyProxy(session);

    HINTERNET connect = WinHttpConnect(session, host.c_str(), port, 0);
    if (!connect)
    {
        WinHttpCloseHandle(session);
        if (outError) *outError = Loc::T(StrId::UpdateFailed);
        return false;
    }

    DWORD flags = https ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET request = WinHttpOpenRequest(connect, L"GET", path.c_str(),
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!request)
    {
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        if (outError) *outError = Loc::T(StrId::UpdateFailed);
        return false;
    }

    if (s.proxyEnabled && !s.proxyUser.empty())
    {
        WinHttpSetCredentials(request, WINHTTP_AUTH_TARGET_PROXY, WINHTTP_AUTH_SCHEME_BASIC,
            s.proxyUser.c_str(), s.proxyPass.c_str(), nullptr);
    }

    BOOL ok = WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (!ok || !WinHttpReceiveResponse(request, nullptr))
    {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        if (outError) *outError = Loc::T(StrId::UpdateFailed);
        return false;
    }

    std::vector<BYTE> body;
    DWORD avail = 0;
    while (WinHttpQueryDataAvailable(request, &avail) && avail > 0)
    {
        size_t old = body.size();
        body.resize(old + avail);
        DWORD read = 0;
        if (!WinHttpReadData(request, body.data() + old, avail, &read) || read == 0)
        {
            body.resize(old);
            break;
        }
        body.resize(old + read);
        avail = 0;
    }

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);

    if (outBody)
        *outBody = std::move(body);
    return outBody && !outBody->empty();
}

bool UpdateChecker::HttpDownloadFile(const std::wstring& url, const std::wstring& path, std::wstring* outError)
{
    std::vector<BYTE> body;
    if (!HttpGet(url, &body, outError))
        return false;

    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        if (outError) *outError = Loc::T(StrId::UpdateFailed);
        return false;
    }
    DWORD written = 0;
    const BOOL ok = WriteFile(file, body.data(), (DWORD)body.size(), &written, nullptr);
    CloseHandle(file);
    if (!ok || written != body.size())
    {
        DeleteFileW(path.c_str());
        if (outError) *outError = Loc::T(StrId::UpdateFailed);
        return false;
    }
    return true;
}

bool UpdateChecker::FetchManifest(UpdateInfo* out, std::wstring* outError)
{
    const std::wstring url = Settings::Instance().Data().updateManifestUrl;
    if (url.empty())
    {
        if (outError) *outError = Loc::T(StrId::UpdateNoUrl);
        return false;
    }

    std::vector<BYTE> body;
    if (!HttpGet(url, &body, outError))
        return false;

    std::string text(reinterpret_cast<const char*>(body.data()), body.size());
    if (!ParseManifest(text, out))
    {
        if (outError) *outError = Loc::T(StrId::UpdateBadManifest);
        return false;
    }
    return true;
}

bool UpdateChecker::IsNewer(const UpdateInfo& info)
{
    return CompareVersions(info.version.c_str(), APP_VERSION_W) > 0;
}

bool UpdateChecker::DownloadAndApply(HWND owner, const UpdateInfo& info, std::wstring* outError)
{
    wchar_t tempDir[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, tempDir);
    const std::wstring newExe = std::wstring(tempDir) + L"Quickshot_update.exe";
    const std::wstring script = std::wstring(tempDir) + L"Quickshot_apply_update.cmd";

    if (!HttpDownloadFile(info.url, newExe, outError))
        return false;

    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    const DWORD pid = GetCurrentProcessId();

    wchar_t cmd[2048];
    wsprintfW(cmd,
        L"@echo off\r\n"
        L":wait\r\n"
        L"timeout /t 1 /nobreak >nul\r\n"
        L"tasklist /FI \"PID eq %u\" 2>nul | find \"%u\" >nul && goto wait\r\n"
        L"copy /Y \"%s\" \"%s\" >nul\r\n"
        L"start \"\" \"%s\"\r\n"
        L"del \"%s\"\r\n"
        L"del \"%%~f0\"\r\n",
        pid, pid, newExe.c_str(), exePath, exePath, newExe.c_str());

    HANDLE file = CreateFileW(script.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        if (outError) *outError = Loc::T(StrId::UpdateFailed);
        return false;
    }
    // Write as ANSI-ish via WideCharToMultiByte for cmd.exe
    const int n = WideCharToMultiByte(CP_ACP, 0, cmd, -1, nullptr, 0, nullptr, nullptr);
    std::string ansi(n > 0 ? n - 1 : 0, 0);
    if (n > 1)
        WideCharToMultiByte(CP_ACP, 0, cmd, -1, ansi.data(), n, nullptr, nullptr);
    DWORD written = 0;
    WriteFile(file, ansi.data(), (DWORD)ansi.size(), &written, nullptr);
    CloseHandle(file);

    const HINSTANCE r = ShellExecuteW(owner, L"open", script.c_str(), nullptr, nullptr, SW_HIDE);
    if (reinterpret_cast<INT_PTR>(r) <= 32)
    {
        if (outError) *outError = Loc::T(StrId::UpdateFailed);
        return false;
    }
    return true;
}

bool UpdateChecker::CheckInteractive(HWND owner)
{
    UpdateInfo info;
    std::wstring err;
    HCURSOR prev = SetCursor(LoadCursorW(nullptr, IDC_WAIT));
    const bool ok = FetchManifest(&info, &err);
    SetCursor(prev);

    if (!ok)
    {
        MessageBoxW(owner, err.c_str(), Loc::T(StrId::UpdateTitle), MB_ICONWARNING | MB_OK);
        return false;
    }

    if (!IsNewer(info))
    {
        MessageBoxW(owner, Loc::T(StrId::UpdateLatest), Loc::T(StrId::UpdateTitle), MB_ICONINFORMATION | MB_OK);
        return false;
    }

    std::wstring msg = Loc::T(StrId::UpdateAvailable);
    msg += L"\n\n";
    msg += Loc::T(StrId::UpdateCurrent);
    msg += L" ";
    msg += APP_VERSION_W;
    msg += L"\n";
    msg += Loc::T(StrId::UpdateNew);
    msg += L" ";
    msg += info.version;
    if (!info.notes.empty())
    {
        msg += L"\n\n";
        msg += info.notes;
    }
    msg += L"\n\n";
    msg += Loc::T(StrId::UpdateAsk);

    if (MessageBoxW(owner, msg.c_str(), Loc::T(StrId::UpdateTitle), MB_ICONQUESTION | MB_YESNO) != IDYES)
        return false;

    prev = SetCursor(LoadCursorW(nullptr, IDC_WAIT));
    const bool applied = DownloadAndApply(owner, info, &err);
    SetCursor(prev);
    if (!applied)
    {
        MessageBoxW(owner, err.c_str(), Loc::T(StrId::UpdateTitle), MB_ICONERROR | MB_OK);
        return false;
    }
    MessageBoxW(owner, Loc::T(StrId::UpdateRestarting), Loc::T(StrId::UpdateTitle), MB_ICONINFORMATION | MB_OK);
    return true;
}

void UpdateChecker::CheckQuietAsync(HWND owner)
{
    auto& s = Settings::Instance().Data();
    if (!s.checkUpdatesOnStartup || s.updateManifestUrl.empty())
        return;

    // At most once per 24h.
    const long long now = (long long)time(nullptr);
    if (s.lastUpdateCheck > 0 && (now - s.lastUpdateCheck) < 24 * 60 * 60)
        return;

    QuietCtx* ctx = new QuietCtx{ owner };
    HANDLE th = CreateThread(nullptr, 0, [](LPVOID p) -> DWORD {
        std::unique_ptr<QuietCtx> ctx(reinterpret_cast<QuietCtx*>(p));
        UpdateInfo info;
        std::wstring err;
        if (!FetchManifest(&info, &err) || !IsNewer(info))
        {
            Settings::Instance().Data().lastUpdateCheck = (long long)time(nullptr);
            Settings::Instance().Save();
            return 0;
        }

        Settings::Instance().Data().lastUpdateCheck = (long long)time(nullptr);
        Settings::Instance().Save();

        std::wstring msg = Loc::T(StrId::UpdateAvailable);
        msg += L" ";
        msg += info.version;
        msg += L"\n";
        msg += Loc::T(StrId::UpdateAsk);

        if (MessageBoxW(ctx->owner, msg.c_str(), Loc::T(StrId::UpdateTitle),
            MB_ICONQUESTION | MB_YESNO | MB_SETFOREGROUND | MB_TOPMOST) != IDYES)
            return 0;

        if (DownloadAndApply(ctx->owner, info, &err))
            PostMessageW(ctx->owner, WM_CLOSE, 0, 0);
        else if (!err.empty())
            MessageBoxW(ctx->owner, err.c_str(), Loc::T(StrId::UpdateTitle), MB_ICONERROR | MB_OK);
        return 0;
    }, ctx, 0, nullptr);

    if (th)
        CloseHandle(th);
    else
        delete ctx;
}
