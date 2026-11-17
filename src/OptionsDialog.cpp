// Author: Ivan Stojmenovic
#include "framework.h"
#include "OptionsDialog.h"
#include "Settings.h"
#include "HotkeyManager.h"
#include "Loc.h"
#include "Resource.h"

namespace
{
    struct OptionsState
    {
        AppSettings draft;
        HWND hwndTabs = nullptr;
        bool capturingHotkey = false;
        int captureTarget = 0;
    };

    OptionsState* g_opt = nullptr;
    HotkeyManager* g_hotkeys = nullptr;

    void SyncHotkeyLabels(HWND hDlg, const AppSettings& s)
    {
        SetDlgItemTextW(hDlg, IDC_EDIT_HOTKEY_CAPTURE,
            HotkeyManager::FormatHotkey(s.captureModifiers, s.captureVk).c_str());
        SetDlgItemTextW(hDlg, IDC_EDIT_HOTKEY_FULLSAVE,
            HotkeyManager::FormatHotkey(s.fullSaveModifiers, s.fullSaveVk).c_str());
    }

    void SyncFormatEnable(HWND hDlg)
    {
        const bool jpeg = IsDlgButtonChecked(hDlg, IDC_RADIO_FMT_JPEG) == BST_CHECKED;
        EnableWindow(GetDlgItem(hDlg, IDC_EDIT_JPEG_QUALITY), jpeg);
        EnableWindow(GetDlgItem(hDlg, IDC_LBL_JPEG_QUALITY), jpeg);
    }

    void SyncProxyEnable(HWND hDlg)
    {
        const BOOL en = IsDlgButtonChecked(hDlg, IDC_CHK_PROXY_ENABLE) == BST_CHECKED;
        const int ids[] = {
            IDC_LBL_PROXY_HOST, IDC_EDIT_PROXY_HOST,
            IDC_LBL_PROXY_PORT, IDC_EDIT_PROXY_PORT,
            IDC_LBL_PROXY_USER, IDC_EDIT_PROXY_USER,
            IDC_LBL_PROXY_PASS, IDC_EDIT_PROXY_PASS
        };
        for (int id : ids)
            EnableWindow(GetDlgItem(hDlg, id), en);
    }

    void ApplyUiTexts(HWND hDlg)
    {
        SetWindowTextW(hDlg, Loc::T(StrId::OptCaption));
        SetDlgItemTextW(hDlg, IDC_CHK_NOTIFY, Loc::T(StrId::ChkNotify));
        SetDlgItemTextW(hDlg, IDC_CHK_KEEP_AREA, Loc::T(StrId::ChkKeepArea));
        SetDlgItemTextW(hDlg, IDC_CHK_CURSOR, Loc::T(StrId::ChkCursor));
        SetDlgItemTextW(hDlg, IDC_CHK_START_WINDOWS, Loc::T(StrId::ChkAutostart));
        SetDlgItemTextW(hDlg, IDC_LBL_LANGUAGE, Loc::T(StrId::LblLanguage));
        SetDlgItemTextW(hDlg, IDC_CHK_HOTKEY_CAPTURE, Loc::T(StrId::ChkHotkeyCapture));
        SetDlgItemTextW(hDlg, IDC_CHK_HOTKEY_FULLSAVE, Loc::T(StrId::ChkHotkeyFullsave));
        SetDlgItemTextW(hDlg, IDC_LBL_HOTKEY_HINT, Loc::T(StrId::HotkeyHint));
        SetDlgItemTextW(hDlg, IDC_LBL_FMT, Loc::T(StrId::LblFmt));
        SetDlgItemTextW(hDlg, IDC_LBL_JPEG_QUALITY, Loc::T(StrId::LblJpegQuality));
        SetDlgItemTextW(hDlg, IDC_LBL_FILENAME, Loc::T(StrId::LblFilename));
        SetDlgItemTextW(hDlg, IDC_LBL_FILENAME_HINT, Loc::T(StrId::LblFilenameHint));
        SetDlgItemTextW(hDlg, IDC_LBL_SAVE_FOLDER, Loc::T(StrId::LblSaveFolder));
        SetDlgItemTextW(hDlg, IDC_BTN_BROWSE_FOLDER, Loc::T(StrId::BtnBrowse));
        SetDlgItemTextW(hDlg, IDC_CHK_ENABLE_UPLOAD, Loc::T(StrId::ChkEnableUpload));
        SetDlgItemTextW(hDlg, IDC_LBL_UPLOAD_URL, Loc::T(StrId::LblUploadUrl));
        SetDlgItemTextW(hDlg, IDC_LBL_UPLOAD_HINT, Loc::T(StrId::LblUploadHint));
        SetDlgItemTextW(hDlg, IDC_CHK_PROXY_ENABLE, Loc::T(StrId::ChkProxyEnable));
        SetDlgItemTextW(hDlg, IDC_LBL_PROXY_HOST, Loc::T(StrId::LblProxyHost));
        SetDlgItemTextW(hDlg, IDC_LBL_PROXY_PORT, Loc::T(StrId::LblProxyPort));
        SetDlgItemTextW(hDlg, IDC_LBL_PROXY_USER, Loc::T(StrId::LblProxyUser));
        SetDlgItemTextW(hDlg, IDC_LBL_PROXY_PASS, Loc::T(StrId::LblProxyPass));
        SetDlgItemTextW(hDlg, IDC_LBL_PROXY_HINT, Loc::T(StrId::LblProxyHint));
        SetDlgItemTextW(hDlg, IDC_CHK_CHECK_UPDATES, Loc::T(StrId::ChkCheckUpdates));
        SetDlgItemTextW(hDlg, IDC_LBL_UPDATE_URL, Loc::T(StrId::LblUpdateUrl));

        HWND tabs = GetDlgItem(hDlg, IDC_TAB_OPTIONS);
        if (tabs)
        {
            TCITEMW item{};
            item.mask = TCIF_TEXT;
            wchar_t t0[64], t1[64], t2[64], t3[64], t4[64];
            wcsncpy_s(t0, Loc::T(StrId::TabGeneral), _TRUNCATE);
            wcsncpy_s(t1, Loc::T(StrId::TabHotkeys), _TRUNCATE);
            wcsncpy_s(t2, Loc::T(StrId::TabFormats), _TRUNCATE);
            wcsncpy_s(t3, Loc::T(StrId::TabCloud), _TRUNCATE);
            wcsncpy_s(t4, Loc::T(StrId::TabProxy), _TRUNCATE);
            item.pszText = t0; TabCtrl_SetItem(tabs, 0, &item);
            item.pszText = t1; TabCtrl_SetItem(tabs, 1, &item);
            item.pszText = t2; TabCtrl_SetItem(tabs, 2, &item);
            item.pszText = t3; TabCtrl_SetItem(tabs, 3, &item);
            item.pszText = t4; TabCtrl_SetItem(tabs, 4, &item);
        }

        HWND combo = GetDlgItem(hDlg, IDC_COMBO_LANGUAGE);
        if (combo)
        {
            const int sel = (int)SendMessageW(combo, CB_GETCURSEL, 0, 0);
            SendMessageW(combo, CB_RESETCONTENT, 0, 0);
            for (int i = 0; i < Loc::LanguageCount(); ++i)
                SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)Loc::LanguageDisplayName(i));
            SendMessageW(combo, CB_SETCURSEL, sel >= 0 ? sel : 0, 0);
        }
    }

    bool BrowseForFolder(HWND owner, std::wstring& outPath)
    {
        const HRESULT coHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        const bool needUninit = (coHr == S_OK);

        IFileOpenDialog* dlg = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&dlg));
        if (FAILED(hr) || !dlg)
        {
            if (needUninit) CoUninitialize();
            return false;
        }

        DWORD options = 0;
        dlg->GetOptions(&options);
        dlg->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
        dlg->SetTitle(Loc::T(StrId::BrowseFolderTitle));

        if (!outPath.empty())
        {
            IShellItem* folder = nullptr;
            if (SUCCEEDED(SHCreateItemFromParsingName(outPath.c_str(), nullptr, IID_PPV_ARGS(&folder))) && folder)
            {
                dlg->SetFolder(folder);
                folder->Release();
            }
        }

        bool ok = false;
        if (SUCCEEDED(dlg->Show(owner)))
        {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dlg->GetResult(&item)) && item)
            {
                PWSTR path = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path)
                {
                    outPath = path;
                    CoTaskMemFree(path);
                    ok = true;
                }
                item->Release();
            }
        }
        dlg->Release();
        if (needUninit)
            CoUninitialize();
        return ok;
    }

    void LoadControls(HWND hDlg, const AppSettings& s)
    {
        CheckDlgButton(hDlg, IDC_CHK_NOTIFY, s.showNotifications ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_CHK_KEEP_AREA, s.keepSelectedArea ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_CHK_CURSOR, s.captureCursor ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_CHK_START_WINDOWS, s.startWithWindows ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_CHK_CHECK_UPDATES, s.checkUpdatesOnStartup ? BST_CHECKED : BST_UNCHECKED);
        SetDlgItemTextW(hDlg, IDC_EDIT_UPDATE_URL, s.updateManifestUrl.c_str());

        HWND combo = GetDlgItem(hDlg, IDC_COMBO_LANGUAGE);
        if (combo)
        {
            SendMessageW(combo, CB_RESETCONTENT, 0, 0);
            for (int i = 0; i < Loc::LanguageCount(); ++i)
                SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)Loc::LanguageDisplayName(i));
            SendMessageW(combo, CB_SETCURSEL, Loc::ToIndex(Loc::FromIndex(s.uiLanguage)), 0);
        }

        CheckDlgButton(hDlg, IDC_CHK_HOTKEY_CAPTURE, s.enableCaptureHotkey ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_CHK_HOTKEY_FULLSAVE, s.enableFullSaveHotkey ? BST_CHECKED : BST_UNCHECKED);
        EnableWindow(GetDlgItem(hDlg, IDC_EDIT_HOTKEY_CAPTURE), s.enableCaptureHotkey);
        EnableWindow(GetDlgItem(hDlg, IDC_EDIT_HOTKEY_FULLSAVE), s.enableFullSaveHotkey);
        SyncHotkeyLabels(hDlg, s);

        CheckDlgButton(hDlg, IDC_RADIO_FMT_PNG, s.imageFormat == 0 ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_RADIO_FMT_JPEG, s.imageFormat == 1 ? BST_CHECKED : BST_UNCHECKED);
        SetDlgItemInt(hDlg, IDC_EDIT_JPEG_QUALITY, (UINT)s.jpegQuality, FALSE);
        SetDlgItemTextW(hDlg, IDC_EDIT_FILENAME, s.fileNamePattern.c_str());
        if (!s.saveFolder.empty())
            SetDlgItemTextW(hDlg, IDC_EDIT_SAVE_FOLDER, s.saveFolder.c_str());
        else
            SetDlgItemTextW(hDlg, IDC_EDIT_SAVE_FOLDER, Settings::Instance().DefaultSaveFolder().c_str());
        SyncFormatEnable(hDlg);

        CheckDlgButton(hDlg, IDC_CHK_ENABLE_UPLOAD, s.enableUpload ? BST_CHECKED : BST_UNCHECKED);
        SetDlgItemTextW(hDlg, IDC_EDIT_UPLOAD_URL, s.uploadUrl.c_str());
        EnableWindow(GetDlgItem(hDlg, IDC_EDIT_UPLOAD_URL), s.enableUpload);

        CheckDlgButton(hDlg, IDC_CHK_PROXY_ENABLE, s.proxyEnabled ? BST_CHECKED : BST_UNCHECKED);
        SetDlgItemTextW(hDlg, IDC_EDIT_PROXY_HOST, s.proxyHost.c_str());
        SetDlgItemInt(hDlg, IDC_EDIT_PROXY_PORT, (UINT)s.proxyPort, FALSE);
        SetDlgItemTextW(hDlg, IDC_EDIT_PROXY_USER, s.proxyUser.c_str());
        SetDlgItemTextW(hDlg, IDC_EDIT_PROXY_PASS, s.proxyPass.c_str());
        SyncProxyEnable(hDlg);
    }

    void SaveControls(HWND hDlg, AppSettings& s)
    {
        s.showNotifications = IsDlgButtonChecked(hDlg, IDC_CHK_NOTIFY) == BST_CHECKED;
        s.keepSelectedArea = IsDlgButtonChecked(hDlg, IDC_CHK_KEEP_AREA) == BST_CHECKED;
        s.captureCursor = IsDlgButtonChecked(hDlg, IDC_CHK_CURSOR) == BST_CHECKED;
        s.startWithWindows = IsDlgButtonChecked(hDlg, IDC_CHK_START_WINDOWS) == BST_CHECKED;
        s.checkUpdatesOnStartup = IsDlgButtonChecked(hDlg, IDC_CHK_CHECK_UPDATES) == BST_CHECKED;
        {
            wchar_t url[512] = {};
            GetDlgItemTextW(hDlg, IDC_EDIT_UPDATE_URL, url, 512);
            s.updateManifestUrl = url;
        }
        {
            const int langSel = (int)SendDlgItemMessageW(hDlg, IDC_COMBO_LANGUAGE, CB_GETCURSEL, 0, 0);
            s.uiLanguage = Loc::ToIndex(Loc::FromIndex(langSel));
        }
        s.enableCaptureHotkey = IsDlgButtonChecked(hDlg, IDC_CHK_HOTKEY_CAPTURE) == BST_CHECKED;
        s.enableFullSaveHotkey = IsDlgButtonChecked(hDlg, IDC_CHK_HOTKEY_FULLSAVE) == BST_CHECKED;

        s.imageFormat = (IsDlgButtonChecked(hDlg, IDC_RADIO_FMT_JPEG) == BST_CHECKED) ? 1 : 0;
        BOOL translated = FALSE;
        UINT q = GetDlgItemInt(hDlg, IDC_EDIT_JPEG_QUALITY, &translated, FALSE);
        if (!translated) q = 90;
        if (q < 1) q = 1;
        if (q > 100) q = 100;
        s.jpegQuality = (int)q;

        wchar_t pattern[512] = {};
        GetDlgItemTextW(hDlg, IDC_EDIT_FILENAME, pattern, 512);
        s.fileNamePattern = pattern;
        if (s.fileNamePattern.empty())
            s.fileNamePattern = L"Screenshot_{yyyy}{MM}{dd}_{HH}{mm}{ss}";

        wchar_t folder[MAX_PATH] = {};
        GetDlgItemTextW(hDlg, IDC_EDIT_SAVE_FOLDER, folder, MAX_PATH);
        s.saveFolder = folder;
        // Trim trailing spaces/slashes for empty check
        while (!s.saveFolder.empty() && (s.saveFolder.back() == L' ' || s.saveFolder.back() == L'\\' || s.saveFolder.back() == L'/'))
            s.saveFolder.pop_back();

        s.enableUpload = IsDlgButtonChecked(hDlg, IDC_CHK_ENABLE_UPLOAD) == BST_CHECKED;
        wchar_t upload[512] = {};
        GetDlgItemTextW(hDlg, IDC_EDIT_UPLOAD_URL, upload, 512);
        s.uploadUrl = upload;
        if (s.uploadUrl.empty())
            s.uploadUrl = L"https://0x0.st";

        s.proxyEnabled = IsDlgButtonChecked(hDlg, IDC_CHK_PROXY_ENABLE) == BST_CHECKED;
        wchar_t host[256] = {};
        GetDlgItemTextW(hDlg, IDC_EDIT_PROXY_HOST, host, 256);
        s.proxyHost = host;
        while (!s.proxyHost.empty() && (s.proxyHost.back() == L' ' || s.proxyHost.back() == L'\t'))
            s.proxyHost.pop_back();
        BOOL portOk = FALSE;
        UINT port = GetDlgItemInt(hDlg, IDC_EDIT_PROXY_PORT, &portOk, FALSE);
        if (!portOk || port < 1) port = 8080;
        if (port > 65535) port = 65535;
        s.proxyPort = (int)port;
        wchar_t user[256] = {}, pass[256] = {};
        GetDlgItemTextW(hDlg, IDC_EDIT_PROXY_USER, user, 256);
        GetDlgItemTextW(hDlg, IDC_EDIT_PROXY_PASS, pass, 256);
        s.proxyUser = user;
        s.proxyPass = pass;
    }

    void ShowTab(HWND hDlg, int sel)
    {
        int generalCtrls[] = {
            IDC_CHK_NOTIFY, IDC_CHK_KEEP_AREA, IDC_CHK_CURSOR, IDC_CHK_START_WINDOWS,
            IDC_LBL_LANGUAGE, IDC_COMBO_LANGUAGE,
            IDC_CHK_CHECK_UPDATES, IDC_LBL_UPDATE_URL, IDC_EDIT_UPDATE_URL
        };
        int hotkeyCtrls[] = {
            IDC_CHK_HOTKEY_CAPTURE, IDC_EDIT_HOTKEY_CAPTURE,
            IDC_CHK_HOTKEY_FULLSAVE, IDC_EDIT_HOTKEY_FULLSAVE, IDC_LBL_HOTKEY_HINT
        };
        int formatCtrls[] = {
            IDC_LBL_FMT, IDC_RADIO_FMT_PNG, IDC_RADIO_FMT_JPEG,
            IDC_LBL_JPEG_QUALITY, IDC_EDIT_JPEG_QUALITY,
            IDC_LBL_FILENAME, IDC_EDIT_FILENAME, IDC_LBL_FILENAME_HINT,
            IDC_LBL_SAVE_FOLDER, IDC_EDIT_SAVE_FOLDER, IDC_BTN_BROWSE_FOLDER
        };
        for (int id : generalCtrls)
            ShowWindow(GetDlgItem(hDlg, id), sel == 0 ? SW_SHOW : SW_HIDE);
        for (int id : hotkeyCtrls)
            ShowWindow(GetDlgItem(hDlg, id), sel == 1 ? SW_SHOW : SW_HIDE);
        int cloudCtrls[] = {
            IDC_CHK_ENABLE_UPLOAD, IDC_LBL_UPLOAD_URL, IDC_EDIT_UPLOAD_URL, IDC_LBL_UPLOAD_HINT
        };
        int proxyCtrls[] = {
            IDC_CHK_PROXY_ENABLE,
            IDC_LBL_PROXY_HOST, IDC_EDIT_PROXY_HOST,
            IDC_LBL_PROXY_PORT, IDC_EDIT_PROXY_PORT,
            IDC_LBL_PROXY_USER, IDC_EDIT_PROXY_USER,
            IDC_LBL_PROXY_PASS, IDC_EDIT_PROXY_PASS,
            IDC_LBL_PROXY_HINT
        };
        for (int id : formatCtrls)
            ShowWindow(GetDlgItem(hDlg, id), sel == 2 ? SW_SHOW : SW_HIDE);
        for (int id : cloudCtrls)
            ShowWindow(GetDlgItem(hDlg, id), sel == 3 ? SW_SHOW : SW_HIDE);
        for (int id : proxyCtrls)
            ShowWindow(GetDlgItem(hDlg, id), sel == 4 ? SW_SHOW : SW_HIDE);
    }

    INT_PTR CALLBACK OptionsMainProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        if (msg == WM_INITDIALOG)
        {
            INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_TAB_CLASSES };
            InitCommonControlsEx(&icc);

            g_opt = new OptionsState();
            g_opt->draft = Settings::Instance().Data();
            g_hotkeys = reinterpret_cast<HotkeyManager*>(lParam);
            g_opt->hwndTabs = GetDlgItem(hDlg, IDC_TAB_OPTIONS);

            TCITEMW item{};
            item.mask = TCIF_TEXT;
            wchar_t t0[] = L"General";
            wchar_t t1[] = L"Hotkeys";
            wchar_t t2[] = L"Formats";
            wchar_t t3[] = L"Cloud";
            wchar_t t4[] = L"Proxy";
            item.pszText = t0;
            TabCtrl_InsertItem(g_opt->hwndTabs, 0, &item);
            item.pszText = t1;
            TabCtrl_InsertItem(g_opt->hwndTabs, 1, &item);
            item.pszText = t2;
            TabCtrl_InsertItem(g_opt->hwndTabs, 2, &item);
            item.pszText = t3;
            TabCtrl_InsertItem(g_opt->hwndTabs, 3, &item);
            item.pszText = t4;
            TabCtrl_InsertItem(g_opt->hwndTabs, 4, &item);

            Loc::SetLanguage(Loc::FromIndex(g_opt->draft.uiLanguage));
            LoadControls(hDlg, g_opt->draft);
            ApplyUiTexts(hDlg);
            ShowTab(hDlg, 0);
            return TRUE;
        }

        if (msg == WM_NOTIFY)
        {
            auto nm = reinterpret_cast<NMHDR*>(lParam);
            if (nm->idFrom == IDC_TAB_OPTIONS && nm->code == TCN_SELCHANGE && g_opt)
                ShowTab(hDlg, TabCtrl_GetCurSel(g_opt->hwndTabs));
            return TRUE;
        }

        if (msg == WM_COMMAND)
        {
            if (!g_opt)
                return FALSE;

            const int id = LOWORD(wParam);
            const int code = HIWORD(wParam);

            if (id == IDC_CHK_HOTKEY_CAPTURE)
            {
                BOOL en = IsDlgButtonChecked(hDlg, IDC_CHK_HOTKEY_CAPTURE) == BST_CHECKED;
                EnableWindow(GetDlgItem(hDlg, IDC_EDIT_HOTKEY_CAPTURE), en);
                return TRUE;
            }
            if (id == IDC_CHK_HOTKEY_FULLSAVE)
            {
                BOOL en = IsDlgButtonChecked(hDlg, IDC_CHK_HOTKEY_FULLSAVE) == BST_CHECKED;
                EnableWindow(GetDlgItem(hDlg, IDC_EDIT_HOTKEY_FULLSAVE), en);
                return TRUE;
            }
            if (id == IDC_RADIO_FMT_PNG || id == IDC_RADIO_FMT_JPEG)
            {
                SyncFormatEnable(hDlg);
                return TRUE;
            }
            if (id == IDC_BTN_BROWSE_FOLDER)
            {
                wchar_t current[MAX_PATH] = {};
                GetDlgItemTextW(hDlg, IDC_EDIT_SAVE_FOLDER, current, MAX_PATH);
                std::wstring path = current;
                if (BrowseForFolder(hDlg, path))
                    SetDlgItemTextW(hDlg, IDC_EDIT_SAVE_FOLDER, path.c_str());
                return TRUE;
            }
            if (id == IDC_COMBO_LANGUAGE && code == CBN_SELCHANGE)
            {
                const int langSel = (int)SendDlgItemMessageW(hDlg, IDC_COMBO_LANGUAGE, CB_GETCURSEL, 0, 0);
                Loc::SetLanguage(Loc::FromIndex(langSel));
                ApplyUiTexts(hDlg);
                return TRUE;
            }
            if (id == IDC_CHK_ENABLE_UPLOAD)
            {
                BOOL en = IsDlgButtonChecked(hDlg, IDC_CHK_ENABLE_UPLOAD) == BST_CHECKED;
                EnableWindow(GetDlgItem(hDlg, IDC_EDIT_UPLOAD_URL), en);
                return TRUE;
            }
            if (id == IDC_CHK_PROXY_ENABLE)
            {
                SyncProxyEnable(hDlg);
                return TRUE;
            }
            if (id == IDC_EDIT_HOTKEY_CAPTURE && code == EN_SETFOCUS)
            {
                g_opt->capturingHotkey = true;
                g_opt->captureTarget = 0;
                SetDlgItemTextW(hDlg, IDC_EDIT_HOTKEY_CAPTURE, Loc::T(StrId::PressKey));
                return TRUE;
            }
            if (id == IDC_EDIT_HOTKEY_FULLSAVE && code == EN_SETFOCUS)
            {
                g_opt->capturingHotkey = true;
                g_opt->captureTarget = 1;
                SetDlgItemTextW(hDlg, IDC_EDIT_HOTKEY_FULLSAVE, Loc::T(StrId::PressKey));
                return TRUE;
            }
            if (id == IDOK)
            {
                // Preserve hotkey values already stored in draft during capture
                AppSettings keepKeys = g_opt->draft;
                SaveControls(hDlg, g_opt->draft);
                g_opt->draft.captureModifiers = keepKeys.captureModifiers;
                g_opt->draft.captureVk = keepKeys.captureVk;
                g_opt->draft.fullSaveModifiers = keepKeys.fullSaveModifiers;
                g_opt->draft.fullSaveVk = keepKeys.fullSaveVk;

                Settings::Instance().Data() = g_opt->draft;
                Settings::Instance().Save();
                if (g_hotkeys)
                    g_hotkeys->ApplyFromSettings();
                EndDialog(hDlg, IDOK);
                return TRUE;
            }
            if (id == IDCANCEL)
            {
                EndDialog(hDlg, IDCANCEL);
                return TRUE;
            }
        }

        // Dialog manager often eats WM_KEYDOWN; use WM_GETDLGCODE via subclass instead.
        // Fallback: peek via message filter on edit using WM_NEXTDLGCTL focus + keyboard hook-like handling:
        if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN)
        {
            if (g_opt && g_opt->capturingHotkey)
            {
                UINT vk = (UINT)wParam;
                if (vk == VK_CONTROL || vk == VK_SHIFT || vk == VK_MENU || vk == VK_LWIN || vk == VK_RWIN || vk == VK_ESCAPE)
                {
                    if (vk == VK_ESCAPE)
                    {
                        g_opt->capturingHotkey = false;
                        SyncHotkeyLabels(hDlg, g_opt->draft);
                    }
                    return TRUE;
                }

                UINT mods = 0;
                if (GetKeyState(VK_CONTROL) & 0x8000) mods |= MOD_CONTROL;
                if (GetKeyState(VK_SHIFT) & 0x8000) mods |= MOD_SHIFT;
                if (GetKeyState(VK_MENU) & 0x8000) mods |= MOD_ALT;
                if ((GetKeyState(VK_LWIN) & 0x8000) || (GetKeyState(VK_RWIN) & 0x8000)) mods |= MOD_WIN;

                if (g_opt->captureTarget == 0)
                {
                    g_opt->draft.captureModifiers = mods;
                    g_opt->draft.captureVk = vk;
                }
                else
                {
                    g_opt->draft.fullSaveModifiers = mods;
                    g_opt->draft.fullSaveVk = vk;
                }
                g_opt->capturingHotkey = false;
                SyncHotkeyLabels(hDlg, g_opt->draft);
                SetFocus(GetDlgItem(hDlg, IDOK));
                return TRUE;
            }
        }

        if (msg == WM_DESTROY)
        {
            delete g_opt;
            g_opt = nullptr;
            g_hotkeys = nullptr;
            return TRUE;
        }

        return FALSE;
    }

    LRESULT CALLBACK HotkeyEditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR)
    {
        if ((msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN || msg == WM_CHAR) && g_opt && g_opt->capturingHotkey)
        {
            HWND hDlg = GetParent(hwnd);
            if (msg == WM_CHAR)
                return 0;

            UINT vk = (UINT)wParam;
            if (vk == VK_CONTROL || vk == VK_SHIFT || vk == VK_MENU || vk == VK_LWIN || vk == VK_RWIN)
                return 0;
            if (vk == VK_ESCAPE)
            {
                g_opt->capturingHotkey = false;
                SyncHotkeyLabels(hDlg, g_opt->draft);
                return 0;
            }

            UINT mods = 0;
            if (GetKeyState(VK_CONTROL) & 0x8000) mods |= MOD_CONTROL;
            if (GetKeyState(VK_SHIFT) & 0x8000) mods |= MOD_SHIFT;
            if (GetKeyState(VK_MENU) & 0x8000) mods |= MOD_ALT;
            if ((GetKeyState(VK_LWIN) & 0x8000) || (GetKeyState(VK_RWIN) & 0x8000)) mods |= MOD_WIN;

            if (GetDlgCtrlID(hwnd) == IDC_EDIT_HOTKEY_CAPTURE)
            {
                g_opt->draft.captureModifiers = mods;
                g_opt->draft.captureVk = vk;
            }
            else
            {
                g_opt->draft.fullSaveModifiers = mods;
                g_opt->draft.fullSaveVk = vk;
            }
            g_opt->capturingHotkey = false;
            SyncHotkeyLabels(hDlg, g_opt->draft);
            SetFocus(GetDlgItem(hDlg, IDOK));
            return 0;
        }
        if (msg == WM_GETDLGCODE)
            return DLGC_WANTALLKEYS;
        return DefSubclassProc(hwnd, msg, wParam, lParam);
    }

    INT_PTR CALLBACK OptionsProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        if (msg == WM_INITDIALOG)
        {
            INT_PTR r = OptionsMainProc(hDlg, msg, wParam, lParam);
            SetWindowSubclass(GetDlgItem(hDlg, IDC_EDIT_HOTKEY_CAPTURE), HotkeyEditProc, 1, 0);
            SetWindowSubclass(GetDlgItem(hDlg, IDC_EDIT_HOTKEY_FULLSAVE), HotkeyEditProc, 2, 0);
            return r;
        }
        if (msg == WM_DESTROY)
        {
            RemoveWindowSubclass(GetDlgItem(hDlg, IDC_EDIT_HOTKEY_CAPTURE), HotkeyEditProc, 1);
            RemoveWindowSubclass(GetDlgItem(hDlg, IDC_EDIT_HOTKEY_FULLSAVE), HotkeyEditProc, 2);
        }
        return OptionsMainProc(hDlg, msg, wParam, lParam);
    }
}

void OptionsDialog::Show(HWND owner, HINSTANCE hInst, HotkeyManager* hotkeys)
{
    DialogBoxParamW(hInst, MAKEINTRESOURCEW(IDD_OPTIONS), owner, OptionsProc, reinterpret_cast<LPARAM>(hotkeys));
}
