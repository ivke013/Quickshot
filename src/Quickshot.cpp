// Author: Ivan Stojmenovic
#include "framework.h"
#include "Quickshot.h"
#include "Loc.h"
#include "UpdateChecker.h"

#define WM_SHOW_NOTIFY (WM_APP + 2)
#define WM_APP_CHECK_UPDATES (WM_APP + 3)

App& App::Instance()
{
    static App app;
    return app;
}

int App::Run(HINSTANCE hInstance)
{
    hInst_ = hInstance;

    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    if (Gdiplus::GdiplusStartup(&gdiplusToken_, &gdiplusStartupInput, nullptr) != Gdiplus::Ok)
        return 1;

    Settings::Instance().Load();

    WNDCLASSEXW wcex{ sizeof(wcex) };
    wcex.lpfnWndProc = App::WndProc;
    wcex.hInstance = hInstance;
    wcex.hIcon = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON,
        GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR);
    wcex.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wcex.lpszClassName = L"QuickshotHidden";
    wcex.hIconSm = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(IDI_TRAY), IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
    RegisterClassExW(&wcex);

    hwnd_ = CreateWindowExW(
        0, L"QuickshotHidden", L"Quickshot",
        WS_OVERLAPPEDWINDOW, 0, 0, 0, 0,
        nullptr, nullptr, hInstance, this);

    if (!hwnd_)
    {
        Gdiplus::GdiplusShutdown(gdiplusToken_);
        return 1;
    }

    // Hidden main window — tray only.
    ShowWindow(hwnd_, SW_HIDE);

    if (!tray_.Create(hwnd_, hInstance, IDI_TRAY, WM_TRAYICON))
    {
        MessageBoxW(nullptr, L"Failed to create tray icon.", L"Quickshot", MB_ICONERROR);
    }

    hotkeys_.Attach(hwnd_);
    hotkeys_.ApplyFromSettings();
    PostMessageW(hwnd_, WM_APP_CHECK_UPDATES, 0, 0);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    hotkeys_.UnregisterAll();
    tray_.Destroy();
    Gdiplus::GdiplusShutdown(gdiplusToken_);
    return (int)msg.wParam;
}

void App::StartRegionCapture()
{
    if (capturing_)
        return;
    capturing_ = true;
    CaptureOverlay::Run(hwnd_, hInst_);
    capturing_ = false;
}

void App::InstantFullscreenSave()
{
    auto& settings = Settings::Instance().Data();
    CapturedScreen screen = ScreenCapture::CaptureVirtualScreen(settings.captureCursor);
    if (!screen.bitmap)
        return;

    std::wstring path;
    if (Exporter::SaveBitmapAuto(screen.bitmap, Settings::Instance().DefaultSaveFolder(), &path))
    {
        if (settings.showNotifications)
        {
            std::wstring text = std::wstring(Loc::T(StrId::NotifySaved)) + L"\n" + path;
            tray_.ShowBalloon(Loc::T(StrId::NotifyTitle), text.c_str());
        }
    }
    screen.Release();
}

void App::ShowOptions()
{
    OptionsDialog::Show(hwnd_, hInst_, &hotkeys_);
}

void App::ShowAbout()
{
    AboutDialog::Show(hwnd_, hInst_);
}

void App::Exit()
{
    tray_.Destroy();
    DestroyWindow(hwnd_);
}

LRESULT CALLBACK App::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    App* self = nullptr;
    if (msg == WM_NCCREATE)
    {
        auto cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<App*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->hwnd_ = hwnd;
    }
    else
    {
        self = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (!self)
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    return self->HandleMessage(msg, wParam, lParam);
}

LRESULT App::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_TRAYICON:
        switch (LOWORD(lParam))
        {
        case WM_LBUTTONUP:
        case NIN_SELECT:
            StartRegionCapture();
            break;
        case WM_RBUTTONUP:
        case WM_CONTEXTMENU:
            tray_.ShowContextMenu();
            break;
        }
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDM_TRAY_CAPTURE:
            StartRegionCapture();
            break;
        case IDM_TRAY_OPTIONS:
            ShowOptions();
            break;
        case IDM_TRAY_ABOUT:
        case IDM_ABOUT:
            ShowAbout();
            break;
        case IDM_TRAY_CHECK_UPDATE:
            if (UpdateChecker::CheckInteractive(hwnd_))
                Exit();
            break;
        case IDM_TRAY_COPY_LINK:
        {
            const auto& url = Settings::Instance().Data().lastUploadUrl;
            if (!url.empty() && Exporter::CopyTextToClipboard(hwnd_, url.c_str()))
            {
                if (Settings::Instance().Data().showNotifications)
                    tray_.ShowBalloon(Loc::T(StrId::NotifyTitle), Loc::T(StrId::NotifyLinkCopied));
            }
            break;
        }
        case IDM_TRAY_OPEN_LINK:
        {
            const auto& url = Settings::Instance().Data().lastUploadUrl;
            if (!url.empty())
                ShellExecuteW(hwnd_, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            break;
        }
        case IDM_TRAY_EXIT:
        case IDM_EXIT:
            Exit();
            break;
        }
        return 0;

    case WM_HOTKEY:
        if (wParam == HOTKEY_ID_CAPTURE)
            StartRegionCapture();
        else if (wParam == HOTKEY_ID_FULLSAVE)
            InstantFullscreenSave();
        return 0;

    case WM_SHOW_NOTIFY:
        if (lParam)
        {
            wchar_t* text = reinterpret_cast<wchar_t*>(lParam);
            if (Settings::Instance().Data().showNotifications)
                tray_.ShowBalloon(Loc::T(StrId::NotifyTitle), text);
            free(text);
        }
        return 0;

    case WM_APP_CHECK_UPDATES:
        UpdateChecker::CheckQuietAsync(hwnd_);
        return 0;

    case WM_CLOSE:
        Exit();
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd_, msg, wParam, lParam);
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE,
    _In_ LPWSTR,
    _In_ int)
{
    // Per-monitor DPI so virtual-screen capture matches physical pixels (125%/150%+).
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_WIN95_CLASSES | ICC_TAB_CLASSES | ICC_STANDARD_CLASSES | ICC_LINK_CLASS };
    InitCommonControlsEx(&icc);

    return App::Instance().Run(hInstance);
}
