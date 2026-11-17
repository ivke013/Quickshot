// Author: Ivan Stojmenovic
#include "framework.h"
#include "Exporter.h"
#include "ScreenCapture.h"
#include "Settings.h"

using namespace Gdiplus;

CLSID Exporter::EncoderClsid(const wchar_t* mime)
{
    UINT num = 0, size = 0;
    GetImageEncodersSize(&num, &size);
    if (size == 0)
        return CLSID{};

    std::vector<BYTE> buffer(size);
    ImageCodecInfo* info = reinterpret_cast<ImageCodecInfo*>(buffer.data());
    GetImageEncoders(num, size, info);

    for (UINT i = 0; i < num; ++i)
    {
        if (wcscmp(info[i].MimeType, mime) == 0)
            return info[i].Clsid;
    }
    return CLSID{};
}

HBITMAP Exporter::ComposeRegion(HBITMAP screenBmp, int screenX, int screenY,
    const RECT& selection, const AnnotationDocument& doc)
{
    RECT r = selection;
    if (r.right < r.left) std::swap(r.left, r.right);
    if (r.bottom < r.top) std::swap(r.top, r.bottom);

    int w = r.right - r.left;
    int h = r.bottom - r.top;
    if (w <= 0 || h <= 0)
        return nullptr;

    int srcX = r.left - screenX;
    int srcY = r.top - screenY;
    HBITMAP cropped = ScreenCapture::CropBitmap(screenBmp, srcX, srcY, w, h, screenX, screenY);
    if (!cropped)
        return nullptr;

    Bitmap bmp(cropped, nullptr);
    Graphics g(&bmp);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);

    // Annotations are stored in screen coordinates; convert to local.
    for (const auto& a : doc.Items())
    {
        Annotation local = a;
        for (auto& p : local.points)
        {
            p.x -= r.left;
            p.y -= r.top;
        }
        local.bounds.left -= r.left;
        local.bounds.right -= r.left;
        local.bounds.top -= r.top;
        local.bounds.bottom -= r.top;
        AnnotationDocument::DrawOne(g, local);
    }

    HBITMAP result = nullptr;
    bmp.GetHBITMAP(Color(255, 255, 255), &result);
    DeleteObject(cropped);
    return result;
}

bool Exporter::CopyBitmapToClipboard(HWND hwnd, HBITMAP bitmap)
{
    if (!bitmap)
        return false;

    HBITMAP copy = ScreenCapture::CloneBitmap(bitmap);
    if (!copy)
        return false;

    if (!OpenClipboard(hwnd))
    {
        DeleteObject(copy);
        return false;
    }
    EmptyClipboard();
    SetClipboardData(CF_BITMAP, copy);
    CloseClipboard();
    // Ownership transferred to clipboard.
    return true;
}

bool Exporter::CopyTextToClipboard(HWND hwnd, const wchar_t* text)
{
    if (!text || !*text)
        return false;

    const size_t chars = wcslen(text) + 1;
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, chars * sizeof(wchar_t));
    if (!mem)
        return false;

    void* locked = GlobalLock(mem);
    if (!locked)
    {
        GlobalFree(mem);
        return false;
    }
    memcpy(locked, text, chars * sizeof(wchar_t));
    GlobalUnlock(mem);

    if (!OpenClipboard(hwnd))
    {
        GlobalFree(mem);
        return false;
    }
    EmptyClipboard();
    SetClipboardData(CF_UNICODETEXT, mem);
    CloseClipboard();
    return true;
}

static bool SaveWithEncoder(HBITMAP bitmap, const wchar_t* path, const wchar_t* mime, int jpegQuality)
{
    Bitmap bmp(bitmap, nullptr);
    CLSID clsid = Exporter::EncoderClsid(mime);
    if (clsid == CLSID{})
        return false;

    if (wcscmp(mime, L"image/jpeg") == 0)
    {
        EncoderParameters params{};
        params.Count = 1;
        params.Parameter[0].Guid = EncoderQuality;
        params.Parameter[0].Type = EncoderParameterValueTypeLong;
        params.Parameter[0].NumberOfValues = 1;
        ULONG q = (ULONG)((std::max)(1, (std::min)(100, jpegQuality)));
        params.Parameter[0].Value = &q;
        return bmp.Save(path, &clsid, &params) == Ok;
    }
    return bmp.Save(path, &clsid, nullptr) == Ok;
}

bool Exporter::SaveBitmapToFile(HWND hwnd, HBITMAP bitmap, const wchar_t* suggestedFolder)
{
    const auto& s = Settings::Instance().Data();
    std::wstring suggested = Settings::Instance().BuildFileName();
    wchar_t file[MAX_PATH] = {};
    wcsncpy_s(file, suggested.c_str(), _TRUNCATE);

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = L"PNG Image (*.png)\0*.png\0JPEG Image (*.jpg)\0*.jpg;*.jpeg\0\0";
    ofn.nFilterIndex = (s.imageFormat == 1) ? 2 : 1;
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrInitialDir = suggestedFolder;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = (s.imageFormat == 1) ? L"jpg" : L"png";

    if (!GetSaveFileNameW(&ofn))
        return false;

    const wchar_t* mime = L"image/png";
    std::wstring path = file;
    auto dot = path.find_last_of(L'.');
    if (dot != std::wstring::npos)
    {
        std::wstring ext = path.substr(dot);
        for (auto& c : ext) c = (wchar_t)towlower(c);
        if (ext == L".jpg" || ext == L".jpeg")
            mime = L"image/jpeg";
    }
    else if (ofn.nFilterIndex == 2)
    {
        mime = L"image/jpeg";
    }
    return SaveWithEncoder(bitmap, file, mime, s.jpegQuality);
}

bool Exporter::SaveBitmapAuto(HBITMAP bitmap, const std::wstring& folder, std::wstring* outPath)
{
    const std::wstring name = folder + L"\\" + Settings::Instance().BuildFileName();
    const wchar_t* mime = Settings::Instance().ImageMime();
    const int quality = Settings::Instance().Data().jpegQuality;
    bool ok = SaveWithEncoder(bitmap, name.c_str(), mime, quality);
    if (ok && outPath)
        *outPath = name;
    return ok;
}

bool Exporter::PrintBitmap(HWND hwnd, HBITMAP bitmap)
{
    if (!bitmap)
        return false;

    BITMAP bm{};
    if (!GetObjectW(bitmap, sizeof(bm), &bm) || bm.bmWidth <= 0 || bm.bmHeight <= 0)
        return false;

    PRINTDLGW pd{};
    pd.lStructSize = sizeof(pd);
    pd.hwndOwner = hwnd;
    pd.Flags = PD_RETURNDC | PD_NOSELECTION | PD_NOPAGENUMS | PD_USEDEVMODECOPIESANDCOLLATE;
    if (!PrintDlgW(&pd) || !pd.hDC)
        return false;

    HDC hdc = pd.hDC;
    DOCINFOW di{};
    di.cbSize = sizeof(di);
    di.lpszDocName = L"Quickshot";

    bool ok = false;
    if (StartDocW(hdc, &di) > 0)
    {
        if (StartPage(hdc) > 0)
        {
            const int pageW = GetDeviceCaps(hdc, HORZRES);
            const int pageH = GetDeviceCaps(hdc, VERTRES);
            const double sx = (double)pageW / (double)bm.bmWidth;
            const double sy = (double)pageH / (double)bm.bmHeight;
            const double scale = (sx < sy) ? sx : sy;
            const int destW = (int)(bm.bmWidth * scale);
            const int destH = (int)(bm.bmHeight * scale);
            const int destX = (pageW - destW) / 2;
            const int destY = (pageH - destH) / 2;

            HDC mem = CreateCompatibleDC(hdc);
            if (mem)
            {
                HGDIOBJ old = SelectObject(mem, bitmap);
                SetStretchBltMode(hdc, HALFTONE);
                SetBrushOrgEx(hdc, 0, 0, nullptr);
                ok = StretchBlt(hdc, destX, destY, destW, destH,
                    mem, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY) != FALSE;
                SelectObject(mem, old);
                DeleteDC(mem);
            }
            EndPage(hdc);
        }
        EndDoc(hdc);
    }

    DeleteDC(hdc);
    if (pd.hDevMode) GlobalFree(pd.hDevMode);
    if (pd.hDevNames) GlobalFree(pd.hDevNames);
    return ok;
}
