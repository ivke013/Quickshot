// Author: Ivan Stojmenovic
#include "framework.h"
#include "ScreenCapture.h"

CapturedScreen ScreenCapture::CaptureVirtualScreen(bool includeCursor)
{
    CapturedScreen result;
    result.x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    result.y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    result.width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    result.height = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    // Fallback if virtual-screen metrics are unavailable/zero.
    if (result.width <= 0 || result.height <= 0)
    {
        result.x = 0;
        result.y = 0;
        result.width = GetSystemMetrics(SM_CXSCREEN);
        result.height = GetSystemMetrics(SM_CYSCREEN);
    }

    HDC screenDc = GetDC(nullptr);
    HDC memDc = CreateCompatibleDC(screenDc);
    result.bitmap = CreateCompatibleBitmap(screenDc, result.width, result.height);
    HGDIOBJ old = SelectObject(memDc, result.bitmap);
    // Source origin is virtual-screen top-left (may be negative on multi-mon).
    BitBlt(memDc, 0, 0, result.width, result.height, screenDc, result.x, result.y, SRCCOPY | CAPTUREBLT);

    if (includeCursor)
    {
        CURSORINFO ci{ sizeof(CURSORINFO) };
        if (GetCursorInfo(&ci) && (ci.flags & CURSOR_SHOWING) && ci.hCursor)
        {
            ICONINFO ii{};
            if (GetIconInfo(ci.hCursor, &ii))
            {
                int cx = ci.ptScreenPos.x - result.x - (int)ii.xHotspot;
                int cy = ci.ptScreenPos.y - result.y - (int)ii.yHotspot;
                DrawIconEx(memDc, cx, cy, ci.hCursor, 0, 0, 0, nullptr, DI_NORMAL);
                if (ii.hbmMask) DeleteObject(ii.hbmMask);
                if (ii.hbmColor) DeleteObject(ii.hbmColor);
            }
        }
    }

    SelectObject(memDc, old);
    DeleteDC(memDc);
    ReleaseDC(nullptr, screenDc);
    return result;
}

HBITMAP ScreenCapture::CloneBitmap(HBITMAP source)
{
    BITMAP bm{};
    if (!GetObject(source, sizeof(bm), &bm))
        return nullptr;

    HDC screenDc = GetDC(nullptr);
    HDC srcDc = CreateCompatibleDC(screenDc);
    HDC dstDc = CreateCompatibleDC(screenDc);
    HBITMAP clone = CreateCompatibleBitmap(screenDc, bm.bmWidth, bm.bmHeight);
    HGDIOBJ oldSrc = SelectObject(srcDc, source);
    HGDIOBJ oldDst = SelectObject(dstDc, clone);
    BitBlt(dstDc, 0, 0, bm.bmWidth, bm.bmHeight, srcDc, 0, 0, SRCCOPY);
    SelectObject(srcDc, oldSrc);
    SelectObject(dstDc, oldDst);
    DeleteDC(srcDc);
    DeleteDC(dstDc);
    ReleaseDC(nullptr, screenDc);
    return clone;
}

HBITMAP ScreenCapture::CropBitmap(HBITMAP source, int srcX, int srcY, int width, int height, int screenX, int screenY)
{
    UNREFERENCED_PARAMETER(screenX);
    UNREFERENCED_PARAMETER(screenY);

    if (width <= 0 || height <= 0)
        return nullptr;

    HDC screenDc = GetDC(nullptr);
    HDC srcDc = CreateCompatibleDC(screenDc);
    HDC dstDc = CreateCompatibleDC(screenDc);
    HBITMAP cropped = CreateCompatibleBitmap(screenDc, width, height);
    HGDIOBJ oldSrc = SelectObject(srcDc, source);
    HGDIOBJ oldDst = SelectObject(dstDc, cropped);
    BitBlt(dstDc, 0, 0, width, height, srcDc, srcX, srcY, SRCCOPY);
    SelectObject(srcDc, oldSrc);
    SelectObject(dstDc, oldDst);
    DeleteDC(srcDc);
    DeleteDC(dstDc);
    ReleaseDC(nullptr, screenDc);
    return cropped;
}
