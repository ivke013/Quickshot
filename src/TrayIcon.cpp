// Author: Ivan Stojmenovic
#include "framework.h"
#include "TrayIcon.h"
#include "Loc.h"
#include "Settings.h"
#include "Resource.h"

bool TrayIcon::Create(HWND hwnd, HINSTANCE hInst, UINT iconId, UINT callbackMsg)
{
    hwnd_ = hwnd;
    ZeroMemory(&nid_, sizeof(nid_));
    nid_.cbSize = sizeof(NOTIFYICONDATAW);
    nid_.hWnd = hwnd;
    nid_.uID = 1;
    nid_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid_.uCallbackMessage = callbackMsg;

    // Prefer a crisp tray-sized icon (16x16 / SM_CXSMICON).
    const int cx = GetSystemMetrics(SM_CXSMICON);
    const int cy = GetSystemMetrics(SM_CYSMICON);
    nid_.hIcon = (HICON)LoadImageW(
        hInst, MAKEINTRESOURCEW(iconId), IMAGE_ICON, cx, cy, LR_DEFAULTCOLOR);
    ownsIcon_ = (nid_.hIcon != nullptr);
    if (!nid_.hIcon)
        nid_.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(iconId));

    wcscpy_s(nid_.szTip, L"Quickshot");

    created_ = Shell_NotifyIconW(NIM_ADD, &nid_) != FALSE;
    if (created_)
    {
        nid_.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &nid_);
    }
    return created_;
}

void TrayIcon::Destroy()
{
    if (created_)
    {
        Shell_NotifyIconW(NIM_DELETE, &nid_);
        created_ = false;
    }
    if (ownsIcon_ && nid_.hIcon)
    {
        DestroyIcon(nid_.hIcon);
        nid_.hIcon = nullptr;
        ownsIcon_ = false;
    }
}

void TrayIcon::ShowBalloon(const wchar_t* title, const wchar_t* text)
{
    if (!created_)
        return;

    nid_.uFlags = NIF_INFO;
    wcsncpy_s(nid_.szInfoTitle, title, _TRUNCATE);
    wcsncpy_s(nid_.szInfo, text, _TRUNCATE);
    nid_.dwInfoFlags = NIIF_INFO;
    Shell_NotifyIconW(NIM_MODIFY, &nid_);
    nid_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
}

void TrayIcon::ShowContextMenu()
{
    POINT pt;
    GetCursorPos(&pt);

    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, IDM_TRAY_CAPTURE, Loc::T(StrId::TrayCapture));
    AppendMenuW(menu, MF_STRING, IDM_TRAY_OPTIONS, Loc::T(StrId::TrayOptions));
    AppendMenuW(menu, MF_STRING, IDM_TRAY_ABOUT, Loc::T(StrId::TrayAbout));
    AppendMenuW(menu, MF_STRING, IDM_TRAY_CHECK_UPDATE, Loc::T(StrId::TrayCheckUpdates));

    HMENU share = CreatePopupMenu();
    const bool hasLink = !Settings::Instance().Data().lastUploadUrl.empty();
    AppendMenuW(share, MF_STRING | (hasLink ? MF_ENABLED : MF_GRAYED),
        IDM_TRAY_COPY_LINK, Loc::T(StrId::TrayCopyLink));
    AppendMenuW(share, MF_STRING | (hasLink ? MF_ENABLED : MF_GRAYED),
        IDM_TRAY_OPEN_LINK, Loc::T(StrId::TrayOpenLink));
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)share, Loc::T(StrId::TrayShare));

    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, IDM_TRAY_EXIT, Loc::T(StrId::TrayExit));

    SetForegroundWindow(hwnd_);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, pt.x, pt.y, 0, hwnd_, nullptr);
    DestroyMenu(menu);
    PostMessageW(hwnd_, WM_NULL, 0, 0);
}
