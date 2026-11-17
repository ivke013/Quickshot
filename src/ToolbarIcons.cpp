// Author: Ivan Stojmenovic
#include "framework.h"
#include "ToolbarIcons.h"
#include "Resource.h"

using namespace Gdiplus;

namespace
{
    const int kRes64[] = {
        IDR_PNG64_PENCIL, IDR_PNG64_HIGHLIGHT, IDR_PNG64_LINE, IDR_PNG64_ARROW, IDR_PNG64_RECT, IDR_PNG64_ELLIPSE, IDR_PNG64_TEXT,
        IDR_PNG64_COLOR, IDR_PNG64_UNDO, IDR_PNG64_COPY, IDR_PNG64_SAVE, IDR_PNG64_UPLOAD, IDR_PNG64_SHARE, IDR_PNG64_CLOSE,
        IDR_PNG64_BOLD, IDR_PNG64_ITALIC, IDR_PNG64_UNDERLINE, IDR_PNG64_FONT_INC, IDR_PNG64_FONT_DEC
    };

    const int kRes32[] = {
        IDR_PNG32_PENCIL, IDR_PNG32_HIGHLIGHT, IDR_PNG32_LINE, IDR_PNG32_ARROW, IDR_PNG32_RECT, IDR_PNG32_ELLIPSE, IDR_PNG32_TEXT,
        IDR_PNG32_COLOR, IDR_PNG32_UNDO, IDR_PNG32_COPY, IDR_PNG32_SAVE, IDR_PNG32_UPLOAD, IDR_PNG32_SHARE, IDR_PNG32_CLOSE,
        IDR_PNG32_BOLD, IDR_PNG32_ITALIC, IDR_PNG32_UNDERLINE, IDR_PNG32_FONT_INC, IDR_PNG32_FONT_DEC
    };

    bool HasRealAlpha(Bitmap* src)
    {
        if (!src)
            return false;
        const INT w = (INT)src->GetWidth();
        const INT h = (INT)src->GetHeight();
        Rect rect(0, 0, w, h);
        BitmapData data{};
        if (src->LockBits(&rect, ImageLockModeRead, PixelFormat32bppARGB, &data) != Ok)
        {
            for (INT y = 0; y < h; y += (std::max)(1, h / 8))
            {
                for (INT x = 0; x < w; x += (std::max)(1, w / 8))
                {
                    Color c;
                    src->GetPixel(x, y, &c);
                    if (c.GetA() < 255)
                        return true;
                }
            }
            return false;
        }

        bool found = false;
        for (INT y = 0; y < h && !found; ++y)
        {
            auto* row = reinterpret_cast<BYTE*>(data.Scan0) + y * data.Stride;
            for (INT x = 0; x < w; ++x)
            {
                if (row[x * 4 + 3] < 255)
                {
                    found = true;
                    break;
                }
            }
        }
        src->UnlockBits(&data);
        return found;
    }

    // Fallback for old white-on-black assets without alpha.
    Bitmap* ToAlphaGlyph(Bitmap* src)
    {
        if (!src || src->GetLastStatus() != Ok)
            return nullptr;

        const INT w = (INT)src->GetWidth();
        const INT h = (INT)src->GetHeight();
        Bitmap* dst = new Bitmap(w, h, PixelFormat32bppARGB);
        if (!dst || dst->GetLastStatus() != Ok)
        {
            delete dst;
            return nullptr;
        }

        Rect rect(0, 0, w, h);
        BitmapData srcData{};
        BitmapData dstData{};
        if (src->LockBits(&rect, ImageLockModeRead, PixelFormat32bppARGB, &srcData) != Ok
            || dst->LockBits(&rect, ImageLockModeWrite, PixelFormat32bppARGB, &dstData) != Ok)
        {
            if (srcData.Scan0)
                src->UnlockBits(&srcData);
            delete dst;
            return nullptr;
        }

        for (INT y = 0; y < h; ++y)
        {
            auto* s = reinterpret_cast<BYTE*>(srcData.Scan0) + y * srcData.Stride;
            auto* d = reinterpret_cast<BYTE*>(dstData.Scan0) + y * dstData.Stride;
            for (INT x = 0; x < w; ++x)
            {
                const BYTE b = s[x * 4 + 0];
                const BYTE g = s[x * 4 + 1];
                const BYTE r = s[x * 4 + 2];
                const BYTE lum = (BYTE)((std::max)({ r, g, b }));
                d[x * 4 + 0] = 255;
                d[x * 4 + 1] = 255;
                d[x * 4 + 2] = 255;
                d[x * 4 + 3] = lum;
            }
        }

        dst->UnlockBits(&dstData);
        src->UnlockBits(&srcData);
        return dst;
    }

    Bitmap* CloneArgb(Bitmap* src)
    {
        if (!src)
            return nullptr;
        return src->Clone(0, 0, (INT)src->GetWidth(), (INT)src->GetHeight(), PixelFormat32bppARGB);
    }

    Bitmap* ScaleTo(Bitmap* src, INT size)
    {
        if (!src)
            return nullptr;
        if ((INT)src->GetWidth() == size && (INT)src->GetHeight() == size)
            return CloneArgb(src);

        Bitmap* dst = new Bitmap(size, size, PixelFormat32bppARGB);
        Graphics g(dst);
        g.Clear(Color(0, 0, 0, 0));
        g.SetCompositingMode(CompositingModeSourceOver);
        g.SetCompositingQuality(CompositingQualityHighQuality);
        // Prefer crisp glyphs; bicubic softens white line icons into looking "old"/blurry.
        const bool downscale = size < (INT)src->GetWidth();
        g.SetInterpolationMode(downscale
            ? InterpolationModeNearestNeighbor
            : InterpolationModeHighQualityBicubic);
        g.SetPixelOffsetMode(PixelOffsetModeHalf);
        g.DrawImage(src, Rect(0, 0, size, size),
            0, 0, (INT)src->GetWidth(), (INT)src->GetHeight(), UnitPixel);
        return dst;
    }
}

Bitmap* ToolbarIcons::LoadPngResource(HINSTANCE hInst, int resId)
{
    HRSRC hrsrc = FindResourceW(hInst, MAKEINTRESOURCEW(resId), RT_RCDATA);
    if (!hrsrc)
        return nullptr;

    HGLOBAL hglob = LoadResource(hInst, hrsrc);
    if (!hglob)
        return nullptr;

    void* data = LockResource(hglob);
    const DWORD size = SizeofResource(hInst, hrsrc);
    if (!data || size == 0)
        return nullptr;

    HGLOBAL hmem = GlobalAlloc(GMEM_MOVEABLE, size);
    if (!hmem)
        return nullptr;

    void* locked = GlobalLock(hmem);
    memcpy(locked, data, size);
    GlobalUnlock(hmem);

    IStream* stream = nullptr;
    if (FAILED(CreateStreamOnHGlobal(hmem, TRUE, &stream)) || !stream)
    {
        GlobalFree(hmem);
        return nullptr;
    }

    Bitmap* sourced = Bitmap::FromStream(stream);
    Bitmap* prepared = nullptr;
    if (sourced && sourced->GetLastStatus() == Ok)
    {
        if (HasRealAlpha(sourced))
            prepared = CloneArgb(sourced);
        else
            prepared = ToAlphaGlyph(sourced);
    }

    delete sourced;
    stream->Release();
    return prepared;
}

bool ToolbarIcons::Load(HINSTANCE hInst, int dpi)
{
    Unload();
    if (dpi <= 0)
        dpi = 96;

    // Prefer native 32×32 @96–120 DPI (1:1, sharp). Use 64×64 masters on higher DPI.
    const bool use64 = dpi >= 144;
    const INT glyph = use64
        ? (INT)std::lround(56.0 * dpi / 192.0)   // ~28@96-equivalent from 64 master
        : 32;
    const int* resIds = use64 ? kRes64 : kRes32;
    bool any = false;
    for (size_t i = 0; i < static_cast<size_t>(ToolIconId::Count); ++i)
    {
        Bitmap* prepared = LoadPngResource(hInst, resIds[i]);
        if (!prepared)
            continue;
        icons_[i] = ScaleTo(prepared, glyph);
        delete prepared;
        if (icons_[i])
            any = true;
    }
    loaded_ = any;
    return loaded_;
}

void ToolbarIcons::Unload()
{
    for (auto& p : icons_)
    {
        delete p;
        p = nullptr;
    }
    loaded_ = false;
}

Bitmap* ToolbarIcons::Get(ToolIconId id) const
{
    const size_t i = static_cast<size_t>(id);
    if (i >= static_cast<size_t>(ToolIconId::Count))
        return nullptr;
    return icons_[i];
}

void ToolbarIcons::DrawButton(Graphics& g, const RECT& rc, ToolIconId id, bool active, Color fill) const
{
    const int x = rc.left;
    const int y = rc.top;
    const int w = rc.right - rc.left;
    const int h = rc.bottom - rc.top;

    SolidBrush br(active ? Color(255, 0, 120, 215) : fill);
    Pen border(Color(220, 210, 210, 210), 1.0f);
    g.FillRectangle(&br, x, y, w, h);
    g.DrawRectangle(&border, x, y, w - 1, h - 1);

    Bitmap* icon = Get(id);
    if (!icon)
        return;

    const INT iw = (INT)icon->GetWidth();
    const INT ih = (INT)icon->GetHeight();
    const INT dx = x + (w - iw) / 2;
    const INT dy = y + (h - ih) / 2;

    g.SetInterpolationMode(InterpolationModeNearestNeighbor);
    g.SetPixelOffsetMode(PixelOffsetModeHalf);
    g.SetCompositingMode(CompositingModeSourceOver);
    g.DrawImage(icon, dx, dy, iw, ih);
}
