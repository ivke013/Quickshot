// Author: Ivan Stojmenovic
#include "framework.h"
#include "AboutDialog.h"
#include "Loc.h"
#include "Resource.h"
#include "UpdateChecker.h"

namespace
{
    INT_PTR CALLBACK AboutProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        if (msg == WM_INITDIALOG)
        {
            SetWindowTextW(hDlg, Loc::T(StrId::AboutCaption));
            SetDlgItemTextW(hDlg, IDC_ABOUT_TITLE, Loc::T(StrId::AboutTitle));

            wchar_t ver[128];
            wsprintfW(ver, Loc::T(StrId::AboutVersionFmt), APP_VERSION_W);
            SetDlgItemTextW(hDlg, IDC_ABOUT_VERSION, ver);
            SetDlgItemTextW(hDlg, IDC_ABOUT_AUTHOR, Loc::T(StrId::AboutAuthor));
            SetDlgItemTextW(hDlg, IDC_ABOUT_COPYRIGHT, Loc::T(StrId::AboutCopyright));
            SetDlgItemTextW(hDlg, IDC_ABOUT_CHECK_UPDATE, Loc::T(StrId::BtnCheckUpdates));

            wchar_t link[256];
            wsprintfW(link, L"<a href=\"mailto:office.stojmenovic@gmail.com\">%s</a>",
                Loc::T(StrId::AboutEmail));
            SetDlgItemTextW(hDlg, IDC_ABOUT_EMAIL, link);
            return TRUE;
        }

        if (msg == WM_NOTIFY)
        {
            auto* nm = reinterpret_cast<NMHDR*>(lParam);
            if (nm && nm->idFrom == IDC_ABOUT_EMAIL &&
                (nm->code == NM_CLICK || nm->code == NM_RETURN))
            {
                auto* link = reinterpret_cast<NMLINK*>(lParam);
                if (link && link->item.szUrl[0])
                    ShellExecuteW(hDlg, L"open", link->item.szUrl, nullptr, nullptr, SW_SHOWNORMAL);
                return TRUE;
            }
        }

        if (msg == WM_COMMAND)
        {
            const int id = LOWORD(wParam);
            if (id == IDC_ABOUT_CHECK_UPDATE)
            {
                if (UpdateChecker::CheckInteractive(hDlg))
                {
                    EndDialog(hDlg, IDOK);
                    if (HWND owner = GetParent(hDlg) ? GetParent(hDlg) : GetWindow(hDlg, GW_OWNER))
                        PostMessageW(owner, WM_CLOSE, 0, 0);
                }
                return TRUE;
            }
            if (id == IDOK || id == IDCANCEL)
            {
                EndDialog(hDlg, id);
                return TRUE;
            }
        }
        return FALSE;
    }
}

void AboutDialog::Show(HWND owner, HINSTANCE hInst)
{
    DialogBoxParamW(hInst, MAKEINTRESOURCEW(IDD_ABOUTBOX), owner, AboutProc, 0);
}
