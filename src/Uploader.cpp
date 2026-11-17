// Author: Ivan Stojmenovic
#include "framework.h"
#include "Uploader.h"
#include "Settings.h"
#include "Exporter.h"

#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")

using namespace Gdiplus;

namespace
{
    bool EncodeBitmap(HBITMAP bitmap, const wchar_t* mime, int jpegQuality, std::vector<BYTE>& out)
    {
        out.clear();
        Bitmap bmp(bitmap, nullptr);
        if (bmp.GetLastStatus() != Ok)
            return false;

        CLSID clsid = Exporter::EncoderClsid(mime);
        if (clsid == CLSID{})
            return false;

        IStream* stream = nullptr;
        if (FAILED(CreateStreamOnHGlobal(nullptr, TRUE, &stream)) || !stream)
            return false;

        Status st = GenericError;
        if (wcscmp(mime, L"image/jpeg") == 0)
        {
            EncoderParameters params{};
            params.Count = 1;
            params.Parameter[0].Guid = EncoderQuality;
            params.Parameter[0].Type = EncoderParameterValueTypeLong;
            params.Parameter[0].NumberOfValues = 1;
            ULONG q = (ULONG)((std::max)(1, (std::min)(100, jpegQuality)));
            params.Parameter[0].Value = &q;
            st = bmp.Save(stream, &clsid, &params);
        }
        else
        {
            st = bmp.Save(stream, &clsid, nullptr);
        }

        if (st != Ok)
        {
            stream->Release();
            return false;
        }

        HGLOBAL hg = nullptr;
        if (FAILED(GetHGlobalFromStream(stream, &hg)) || !hg)
        {
            stream->Release();
            return false;
        }

        const SIZE_T size = GlobalSize(hg);
        void* data = GlobalLock(hg);
        if (!data || size == 0)
        {
            if (data) GlobalUnlock(hg);
            stream->Release();
            return false;
        }

        out.resize(size);
        memcpy(out.data(), data, size);
        GlobalUnlock(hg);
        stream->Release();
        return !out.empty();
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

    std::wstring TrimUrl(std::wstring s)
    {
        while (!s.empty() && (s.back() == L'\r' || s.back() == L'\n' || s.back() == L' ' || s.back() == L'\t'))
            s.pop_back();
        size_t i = 0;
        while (i < s.size() && (s[i] == L' ' || s[i] == L'\t' || s[i] == L'\r' || s[i] == L'\n'))
            ++i;
        return s.substr(i);
    }

    std::wstring ExtractUrl(const std::string& body)
    {
        // Prefer JSON "url":"..."
        const char* key = "\"url\"";
        size_t pos = body.find(key);
        if (pos != std::string::npos)
        {
            pos = body.find(':', pos);
            if (pos != std::string::npos)
            {
                pos = body.find('"', pos + 1);
                if (pos != std::string::npos)
                {
                    size_t end = body.find('"', pos + 1);
                    if (end != std::string::npos)
                    {
                        std::string u = body.substr(pos + 1, end - pos - 1);
                        return TrimUrl(std::wstring(u.begin(), u.end()));
                    }
                }
            }
        }

        // Plain-text response: first line that looks like http(s)
        size_t lineStart = 0;
        while (lineStart < body.size())
        {
            size_t lineEnd = body.find_first_of("\r\n", lineStart);
            if (lineEnd == std::string::npos)
                lineEnd = body.size();
            std::string line = body.substr(lineStart, lineEnd - lineStart);
            while (!line.empty() && (line.back() == ' ' || line.back() == '\t'))
                line.pop_back();
            if (line.rfind("http://", 0) == 0 || line.rfind("https://", 0) == 0)
                return TrimUrl(std::wstring(line.begin(), line.end()));
            lineStart = lineEnd;
            while (lineStart < body.size() && (body[lineStart] == '\r' || body[lineStart] == '\n'))
                ++lineStart;
        }
        return {};
    }
}

bool Uploader::UploadBitmap(HBITMAP bitmap, std::wstring* outUrl, std::wstring* outError)
{
    if (outUrl) outUrl->clear();
    if (outError) outError->clear();

    auto& s = Settings::Instance().Data();
    std::wstring endpoint = s.uploadUrl;
    if (endpoint.empty())
        endpoint = L"https://0x0.st";

    const wchar_t* mime = Settings::Instance().ImageMime();
    const wchar_t* ext = Settings::Instance().ImageExtension();
    std::vector<BYTE> image;
    if (!EncodeBitmap(bitmap, mime, s.jpegQuality, image))
    {
        if (outError) *outError = L"Failed to encode image.";
        return false;
    }

    const std::string boundary = "----QuickshotBoundary7MA4YWxkTrZu0gW";
    const std::string filename = std::string("screenshot") +
        std::string(ext[0] == L'.' ? "" : ".") +
        [&]() {
            std::string e;
            for (const wchar_t* p = ext; *p; ++p)
                e.push_back((char)towlower(*p));
            return e;
        }();

    std::string contentType = (wcscmp(mime, L"image/jpeg") == 0) ? "image/jpeg" : "image/png";
    std::string body;
    body.reserve(image.size() + 512);
    body += "--" + boundary + "\r\n";
    body += "Content-Disposition: form-data; name=\"file\"; filename=\"" + filename + "\"\r\n";
    body += "Content-Type: " + contentType + "\r\n\r\n";
    body.append(reinterpret_cast<const char*>(image.data()), image.size());
    body += "\r\n--" + boundary + "--\r\n";

    bool https = true;
    std::wstring host, path;
    INTERNET_PORT port = INTERNET_DEFAULT_HTTPS_PORT;
    if (!ParseUrl(endpoint, &https, &host, &port, &path))
    {
        if (outError) *outError = L"Invalid upload URL.";
        return false;
    }

    DWORD accessType = WINHTTP_ACCESS_TYPE_DEFAULT_PROXY;
    std::wstring proxyServer;
    LPCWSTR proxyName = WINHTTP_NO_PROXY_NAME;
    LPCWSTR proxyBypass = WINHTTP_NO_PROXY_BYPASS;
    if (s.proxyEnabled)
    {
        if (!s.proxyHost.empty())
        {
            wchar_t portBuf[16] = {};
            wsprintfW(portBuf, L"%d", s.proxyPort);
            proxyServer = s.proxyHost + L":" + portBuf;
            accessType = WINHTTP_ACCESS_TYPE_NAMED_PROXY;
            proxyName = proxyServer.c_str();
            proxyBypass = L"<local>";
        }
        else
        {
            accessType = WINHTTP_ACCESS_TYPE_NO_PROXY;
        }
    }

    HINTERNET session = WinHttpOpen(L"Quickshot/1.1", accessType, proxyName, proxyBypass, 0);
    if (!session)
    {
        if (outError) *outError = L"WinHTTP open failed.";
        return false;
    }

    HINTERNET connect = WinHttpConnect(session, host.c_str(), port, 0);
    if (!connect)
    {
        WinHttpCloseHandle(session);
        if (outError) *outError = L"Connect failed.";
        return false;
    }

    DWORD flags = https ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET request = WinHttpOpenRequest(connect, L"POST", path.c_str(),
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!request)
    {
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        if (outError) *outError = L"OpenRequest failed.";
        return false;
    }

    if (s.proxyEnabled && !s.proxyUser.empty())
    {
        WinHttpSetCredentials(request, WINHTTP_AUTH_TARGET_PROXY,
            WINHTTP_AUTH_SCHEME_BASIC,
            s.proxyUser.c_str(), s.proxyPass.c_str(), nullptr);
    }

    std::wstring headers = L"Content-Type: multipart/form-data; boundary=";
    headers += std::wstring(boundary.begin(), boundary.end());

    BOOL ok = WinHttpSendRequest(request, headers.c_str(), (DWORD)headers.size(),
        (LPVOID)body.data(), (DWORD)body.size(), (DWORD)body.size(), 0);
    if (!ok || !WinHttpReceiveResponse(request, nullptr))
    {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        if (outError) *outError = L"Upload request failed.";
        return false;
    }

    // Retry once on proxy authentication required (407).
    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    if (WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX)
        && statusCode == 407
        && s.proxyEnabled && !s.proxyUser.empty())
    {
        WinHttpSetCredentials(request, WINHTTP_AUTH_TARGET_PROXY,
            WINHTTP_AUTH_SCHEME_BASIC,
            s.proxyUser.c_str(), s.proxyPass.c_str(), nullptr);
        ok = WinHttpSendRequest(request, headers.c_str(), (DWORD)headers.size(),
            (LPVOID)body.data(), (DWORD)body.size(), (DWORD)body.size(), 0);
        if (!ok || !WinHttpReceiveResponse(request, nullptr))
        {
            WinHttpCloseHandle(request);
            WinHttpCloseHandle(connect);
            WinHttpCloseHandle(session);
            if (outError) *outError = L"Upload request failed (proxy auth).";
            return false;
        }
    }

    std::string response;
    DWORD avail = 0;
    while (WinHttpQueryDataAvailable(request, &avail) && avail > 0)
    {
        std::vector<char> buf(avail);
        DWORD read = 0;
        if (!WinHttpReadData(request, buf.data(), avail, &read) || read == 0)
            break;
        response.append(buf.data(), read);
        avail = 0;
    }

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);

    std::wstring url = ExtractUrl(response);
    if (url.empty())
    {
        if (outError)
        {
            if (response.empty())
                *outError = L"Empty server response.";
            else
                *outError = L"Server did not return a URL.";
        }
        return false;
    }

    if (outUrl)
        *outUrl = url;
    return true;
}
