// Author: Ivan Stojmenovic
#include "framework.h"
#include "CaptureOverlay.h"
#include "Settings.h"
#include "Exporter.h"
#include "Loc.h"
#include "Uploader.h"
#include "Resource.h"

using namespace Gdiplus;

namespace
{
    CaptureOverlay* g_overlay = nullptr;

    RECT NormRect(RECT r)
    {
        if (r.right < r.left) std::swap(r.left, r.right);
        if (r.bottom < r.top) std::swap(r.top, r.bottom);
        return r;
    }

    bool PtIn(const RECT& r, int x, int y)
    {
        return x >= r.left && x < r.right && y >= r.top && y < r.bottom;
    }

}

CaptureOverlay::CaptureOverlay(HWND owner, HINSTANCE hInst)
    : owner_(owner), hInst_(hInst)
{
    auto& s = Settings::Instance().Data();
    color_ = s.drawColor;
    strokeWidth_ = s.strokeWidth;
    if (strokeWidth_ < 1.0f) strokeWidth_ = 1.0f;
    if (strokeWidth_ > 12.0f) strokeWidth_ = 12.0f;
    textStyle_.bold = s.fontBold;
    textStyle_.italic = s.fontItalic;
    textStyle_.underline = s.fontUnderline;
    textStyle_.fontSize = s.fontSize;
    textStyle_.color = s.drawColor;
}

CaptureOverlay::~CaptureOverlay()
{
    if (textBoxActive_ || textEdit_)
        FinishTextEdit(false);
    if (hwnd_)
        DestroyWindow(hwnd_);
    icons_.Unload();
    screen_.Release();
}

OverlayResult CaptureOverlay::Run(HWND owner, HINSTANCE hInst)
{
    CaptureOverlay overlay(owner, hInst);
    if (!overlay.Create())
        return OverlayResult::Cancelled;
    return overlay.ModalLoop();
}

bool CaptureOverlay::Create()
{
    auto& settings = Settings::Instance().Data();
    screen_ = ScreenCapture::CaptureVirtualScreen(settings.captureCursor);
    if (!screen_.bitmap)
        return false;

    WNDCLASSEXW wc{ sizeof(wc) };
    wc.lpfnWndProc = CaptureOverlay::WndProc;
    wc.hInstance = hInst_;
    wc.hCursor = LoadCursor(nullptr, IDC_CROSS);
    wc.lpszClassName = L"QuickshotOverlay";
    wc.hbrBackground = nullptr;
    RegisterClassExW(&wc);

    hwnd_ = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        L"QuickshotOverlay",
        L"",
        WS_POPUP,
        screen_.x, screen_.y, screen_.width, screen_.height,
        owner_, nullptr, hInst_, this);

    if (!hwnd_)
        return false;

    int dpi = 96;
    if (HDC screenDc = GetDC(nullptr))
    {
        dpi = GetDeviceCaps(screenDc, LOGPIXELSX);
        ReleaseDC(nullptr, screenDc);
    }
    // Prefer window DPI (per-monitor) when available.
    const int windowDpi = (int)GetDpiForWindow(hwnd_);
    if (windowDpi > 0)
        dpi = windowDpi;
    ApplyDpiMetrics(dpi);
    icons_.Load(hInst_, dpi_);

    if (settings.keepSelectedArea && settings.hasLastSelection)
    {
        selection_ = settings.lastSelection;
        hasSelection_ = true;
        UpdateToolbarsLayout();
    }

    ShowWindow(hwnd_, SW_SHOW);
    SetForegroundWindow(hwnd_);
    UpdateWindow(hwnd_);
    return true;
}

OverlayResult CaptureOverlay::ModalLoop()
{
    MSG msg;
    while (!done_ && GetMessageW(&msg, nullptr, 0, 0))
    {
        if (textEdit_ && IsDialogMessageW(hwnd_, &msg))
            continue;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return result_;
}

LRESULT CALLBACK CaptureOverlay::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    CaptureOverlay* self = nullptr;
    if (msg == WM_NCCREATE)
    {
        auto cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<CaptureOverlay*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->hwnd_ = hwnd;
        g_overlay = self;
    }
    else
    {
        self = reinterpret_cast<CaptureOverlay*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (!self)
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    return self->HandleMessage(msg, wParam, lParam);
}

RECT CaptureOverlay::NormalizeSelection() const
{
    return NormRect(selection_);
}

bool CaptureOverlay::HasSelection() const
{
    if (!hasSelection_)
        return false;
    RECT r = NormalizeSelection();
    return (r.right - r.left) > 2 && (r.bottom - r.top) > 2;
}

void CaptureOverlay::UpdateToolbarsLayout()
{
    if (!HasSelection())
    {
        vertToolbar_ = {};
        horizToolbar_ = {};
        return;
    }

    RECT sel = NormalizeSelection();
    const int tools = 11; // pencil,highlight,line,arrow,rect,ellipse,text,color,undo,stroke-,stroke+
    int vh = tools * uiToolBtn_ + (tools - 1) * uiToolGap_;
    int vw = uiToolBtn_;

    const int gap = DpiScale(8);
    vertToolbar_.left = sel.right + gap;
    if (vertToolbar_.left + vw > screen_.x + screen_.width)
        vertToolbar_.left = sel.left - gap - vw;
    vertToolbar_.top = sel.top;
    if (vertToolbar_.top + vh > screen_.y + screen_.height)
        vertToolbar_.top = screen_.y + screen_.height - vh - DpiScale(4);
    vertToolbar_.right = vertToolbar_.left + vw;
    vertToolbar_.bottom = vertToolbar_.top + vh;

    const int actions = 4; // copy, save, share, close
    int hw = actions * uiToolBtn_ + (actions - 1) * uiToolGap_;
    int hh = uiToolBtn_;
    horizToolbar_.right = sel.right;
    horizToolbar_.left = horizToolbar_.right - hw;
    horizToolbar_.top = sel.bottom + gap;
    if (horizToolbar_.top + hh > screen_.y + screen_.height)
        horizToolbar_.top = sel.top - gap - hh;
    if (horizToolbar_.left < screen_.x)
        horizToolbar_.left = screen_.x + DpiScale(4);
    horizToolbar_.right = horizToolbar_.left + hw;
    horizToolbar_.bottom = horizToolbar_.top + hh;
}

int CaptureOverlay::HitTestHandle(int x, int y) const
{
    if (!HasSelection())
        return HitNone;

    RECT sel = NormalizeSelection();
    auto hit = [&](int cx, int cy) {
        RECT hr{ cx - uiHandle_ / 2, cy - uiHandle_ / 2, cx + uiHandle_ / 2 + 1, cy + uiHandle_ / 2 + 1 };
        return PtIn(hr, x, y);
    };

    int midX = (sel.left + sel.right) / 2;
    int midY = (sel.top + sel.bottom) / 2;

    if (hit(sel.left, sel.top)) return HitResizeTL;
    if (hit(midX, sel.top)) return HitResizeT;
    if (hit(sel.right, sel.top)) return HitResizeTR;
    if (hit(sel.left, midY)) return HitResizeL;
    if (hit(sel.right, midY)) return HitResizeR;
    if (hit(sel.left, sel.bottom)) return HitResizeBL;
    if (hit(midX, sel.bottom)) return HitResizeB;
    if (hit(sel.right, sel.bottom)) return HitResizeBR;
    if (PtIn(sel, x, y)) return HitMove;
    return HitNone;
}

bool CaptureOverlay::HitTestToolbar(int x, int y, int* outAction) const
{
    if (!HasSelection() || !PtIn(horizToolbar_, x, y))
        return false;
    int idx = (x - horizToolbar_.left) / (uiToolBtn_ + uiToolGap_);
    if (idx < 0 || idx > 3)
        return false;
    *outAction = ActCopy + idx;
    return true;
}

bool CaptureOverlay::HitTestVerticalTools(int x, int y, int* outTool) const
{
    if (!HasSelection() || !PtIn(vertToolbar_, x, y))
        return false;
    int idx = (y - vertToolbar_.top) / (uiToolBtn_ + uiToolGap_);
    if (idx < 0 || idx > 10)
        return false;
    *outTool = idx;
    return true;
}

void CaptureOverlay::DrawHandles(HDC hdc, const RECT& sel)
{
    auto draw = [&](int cx, int cy) {
        RECT hr{ cx - uiHandle_ / 2, cy - uiHandle_ / 2, cx + uiHandle_ / 2, cy + uiHandle_ / 2 };
        FillRect(hdc, &hr, (HBRUSH)GetStockObject(WHITE_BRUSH));
        FrameRect(hdc, &hr, (HBRUSH)GetStockObject(BLACK_BRUSH));
    };
    int midX = (sel.left + sel.right) / 2;
    int midY = (sel.top + sel.bottom) / 2;
    draw(sel.left, sel.top);
    draw(midX, sel.top);
    draw(sel.right, sel.top);
    draw(sel.left, midY);
    draw(sel.right, midY);
    draw(sel.left, sel.bottom);
    draw(midX, sel.bottom);
    draw(sel.right, sel.bottom);
}

void CaptureOverlay::DrawSizeLabel(HDC hdc, const RECT& sel)
{
    wchar_t buf[64];
    wsprintfW(buf, L"%dx%d", sel.right - sel.left, sel.bottom - sel.top);
    SIZE sz{};
    GetTextExtentPoint32W(hdc, buf, lstrlenW(buf), &sz);
    RECT bg{ sel.left, sel.top - sz.cy - 6, sel.left + sz.cx + 8, sel.top - 2 };
    if (bg.top < screen_.y)
    {
        bg.top = sel.top + 2;
        bg.bottom = bg.top + sz.cy + 4;
    }
    HBRUSH br = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(hdc, &bg, br);
    DeleteObject(br);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 255, 255));
    RECT tr = bg;
    InflateRect(&tr, -4, -2);
    DrawTextW(hdc, buf, -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

void CaptureOverlay::DrawToolbars(HDC hdc)
{
    if (!HasSelection())
        return;

    Graphics g(hdc);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);

    const ToolIconId toolIcons[] = {
        ToolIconId::Pencil, ToolIconId::Highlight, ToolIconId::Line, ToolIconId::Arrow,
        ToolIconId::Rectangle, ToolIconId::Ellipse, ToolIconId::Text, ToolIconId::Color, ToolIconId::Undo
    };
    const ToolType toolTypes[] = {
        ToolType::Pencil, ToolType::Highlight, ToolType::Line, ToolType::Arrow,
        ToolType::Rectangle, ToolType::Ellipse, ToolType::Text, ToolType::None, ToolType::None
    };

    for (int i = 0; i < 9; ++i)
    {
        RECT rc{
            vertToolbar_.left,
            vertToolbar_.top + i * (uiToolBtn_ + uiToolGap_),
            vertToolbar_.right,
            vertToolbar_.top + i * (uiToolBtn_ + uiToolGap_) + uiToolBtn_
        };
        bool active = (i < 7) && (currentTool_ == toolTypes[i]);
        icons_.DrawButton(g, rc, toolIcons[i], active);

        // Live color preview on the color button
        if (i == 7)
        {
            const int m = DpiScale(8);
            RECT sw{ rc.left + m, rc.top + m, rc.right - m, rc.bottom - m };
            HBRUSH br = CreateSolidBrush(color_);
            FillRect(hdc, &sw, br);
            DeleteObject(br);
            FrameRect(hdc, &sw, (HBRUSH)GetStockObject(WHITE_BRUSH));
        }
    }

    // Stroke - / + with thickness preview line
    for (int i = 0; i < 2; ++i)
    {
        RECT rc{
            vertToolbar_.left,
            vertToolbar_.top + (9 + i) * (uiToolBtn_ + uiToolGap_),
            vertToolbar_.right,
            vertToolbar_.top + (9 + i) * (uiToolBtn_ + uiToolGap_) + uiToolBtn_
        };
        SolidBrush fill(Color(230, 35, 35, 35));
        Pen border(Color(220, 210, 210, 210), 1.0f);
        g.FillRectangle(&fill, (INT)rc.left, (INT)rc.top, (INT)(rc.right - rc.left), (INT)(rc.bottom - rc.top));
        g.DrawRectangle(&border, (INT)rc.left, (INT)rc.top, (INT)(rc.right - rc.left - 1), (INT)(rc.bottom - rc.top - 1));

        const INT cx = (INT)((rc.left + rc.right) / 2);
        const INT cy = (INT)((rc.top + rc.bottom) / 2);
        Pen white(Color(255, 255, 255), 2.0f);
        white.SetLineCap(LineCapRound, LineCapRound, DashCapRound);
        const INT arm = (INT)DpiScale(8);
        if (i == 0)
        {
            g.DrawLine(&white, cx - arm, cy, cx + arm, cy);
        }
        else
        {
            g.DrawLine(&white, cx - arm, cy, cx + arm, cy);
            g.DrawLine(&white, cx, cy - arm, cx, cy + arm);
        }
        const REAL tw = (std::min)(10.0f, strokeWidth_);
        Pen tip(Color(200, 255, 255, 255), tw);
        tip.SetLineCap(LineCapRound, LineCapRound, DashCapRound);
        const INT pad = (INT)DpiScale(6);
        g.DrawLine(&tip, (INT)(rc.left + pad), (INT)(rc.bottom - pad), (INT)(rc.right - pad), (INT)(rc.bottom - pad));
    }

    const ToolIconId actIcons[] = { ToolIconId::Copy, ToolIconId::Save, ToolIconId::Share, ToolIconId::Close };
    for (int i = 0; i < 4; ++i)
    {
        RECT rc{
            horizToolbar_.left + i * (uiToolBtn_ + uiToolGap_),
            horizToolbar_.top,
            horizToolbar_.left + i * (uiToolBtn_ + uiToolGap_) + uiToolBtn_,
            horizToolbar_.bottom
        };
        Color fill = (i == 3) ? Color(230, 160, 40, 40) : Color(230, 35, 35, 35);
        icons_.DrawButton(g, rc, actIcons[i], false, fill);
    }
}

void CaptureOverlay::OnPaint()
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd_, &ps);

    HDC memDc = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, screen_.width, screen_.height);
    HGDIOBJ oldBmp = SelectObject(memDc, memBmp);

    HDC srcDc = CreateCompatibleDC(hdc);
    HGDIOBJ oldSrc = SelectObject(srcDc, screen_.bitmap);
    BitBlt(memDc, 0, 0, screen_.width, screen_.height, srcDc, 0, 0, SRCCOPY);
    SelectObject(srcDc, oldSrc);
    DeleteDC(srcDc);

    // Dim overlay via GDI+
    {
        Graphics g(memDc);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        SolidBrush dim(Color(120, 0, 0, 0));

        if (HasSelection() || selecting_)
        {
            RECT sel = selecting_ ? NormRect(selection_) : NormalizeSelection();
            // Convert to client (window is at virtual screen origin)
            RECT local = sel;
            local.left -= screen_.x;
            local.right -= screen_.x;
            local.top -= screen_.y;
            local.bottom -= screen_.y;

            // Draw dim as four rectangles around selection
            const INT lx = (INT)local.left;
            const INT ly = (INT)local.top;
            const INT lr = (INT)local.right;
            const INT lb = (INT)local.bottom;
            const INT sw = screen_.width;
            const INT sh = screen_.height;
            Rect top(0, 0, sw, (std::max)(0, ly));
            Rect bottom(0, lb, sw, (std::max)(0, sh - lb));
            Rect left(0, ly, (std::max)(0, lx), (std::max)(0, lb - ly));
            Rect right(lr, ly, (std::max)(0, sw - lr), (std::max)(0, lb - ly));
            g.FillRectangle(&dim, top);
            g.FillRectangle(&dim, bottom);
            g.FillRectangle(&dim, left);
            g.FillRectangle(&dim, right);

            // Selection border
            Pen border(Color(255, 255, 255), 1.5f);
            border.SetDashStyle(DashStyleDash);
            g.DrawRectangle(&border, lx, ly, lr - lx, lb - ly);

            // Annotations in screen coords relative to window
            for (int ai = 0; ai < doc_.Count(); ++ai)
            {
                if (ai == editingTextIndex_ && textBoxActive_)
                    continue;
                Annotation localAnn = *doc_.Get(ai);
                for (auto& p : localAnn.points)
                {
                    p.x -= screen_.x;
                    p.y -= screen_.y;
                }
                localAnn.bounds.left -= screen_.x;
                localAnn.bounds.right -= screen_.x;
                localAnn.bounds.top -= screen_.y;
                localAnn.bounds.bottom -= screen_.y;
                AnnotationDocument::DrawOne(g, localAnn);
            }
            if (drafting_)
            {
                Annotation localAnn = draft_;
                for (auto& p : localAnn.points)
                {
                    p.x -= screen_.x;
                    p.y -= screen_.y;
                }
                localAnn.bounds.left -= screen_.x;
                localAnn.bounds.right -= screen_.x;
                localAnn.bounds.top -= screen_.y;
                localAnn.bounds.bottom -= screen_.y;
                AnnotationDocument::DrawOne(g, localAnn);
            }

            if (selectedAnn_ >= 0 && selectedAnn_ != editingTextIndex_)
            {
                const Annotation* selA = doc_.Get(selectedAnn_);
                if (selA)
                {
                    Annotation localAnn = *selA;
                    for (auto& p : localAnn.points)
                    {
                        p.x -= screen_.x;
                        p.y -= screen_.y;
                    }
                    localAnn.bounds.left -= screen_.x;
                    localAnn.bounds.right -= screen_.x;
                    localAnn.bounds.top -= screen_.y;
                    localAnn.bounds.bottom -= screen_.y;
                    AnnotationDocument::DrawSelection(g, localAnn, dpiScale_);
                }
            }
        }
        else
        {
            g.FillRectangle(&dim, 0, 0, screen_.width, screen_.height);
        }
    }

    if (HasSelection() || selecting_)
    {
        RECT sel = selecting_ ? NormRect(selection_) : NormalizeSelection();
        RECT local = sel;
        OffsetRect(&local, -screen_.x, -screen_.y);
        DrawSizeLabel(memDc, local);
        if (!selecting_)
        {
            DrawHandles(memDc, local);
            RECT oldV = vertToolbar_, oldH = horizToolbar_;
            OffsetRect(&vertToolbar_, -screen_.x, -screen_.y);
            OffsetRect(&horizToolbar_, -screen_.x, -screen_.y);
            DrawToolbars(memDc);
            vertToolbar_ = oldV;
            horizToolbar_ = oldH;
        }
    }

    if (textBoxActive_ || creatingTextBox_)
        DrawTextBoxFrame(memDc);

    if (textBoxActive_ && textFmtBar_.right > textFmtBar_.left)
    {
        Graphics tg(memDc);
        tg.SetSmoothingMode(SmoothingModeAntiAlias);
        tg.SetInterpolationMode(InterpolationModeNearestNeighbor);
        const ToolIconId fmtIcons[] = {
            ToolIconId::Bold, ToolIconId::Italic, ToolIconId::Underline,
            ToolIconId::FontInc, ToolIconId::FontDec
        };
        const int slot = uiFmtSlot_;
        const int btn = (std::max)(1, slot - DpiScale(4));
        for (int i = 0; i < 5; ++i)
        {
            RECT rc{
                textFmtBar_.left + i * slot,
                textFmtBar_.top,
                textFmtBar_.left + i * slot + btn,
                textFmtBar_.bottom
            };
            bool active = (i == 0 && textStyle_.bold) || (i == 1 && textStyle_.italic) || (i == 2 && textStyle_.underline);
            icons_.DrawButton(tg, rc, fmtIcons[i], active);
        }

        // Text color swatch (independent of stroke color_)
        RECT crc{
            textFmtBar_.left + 5 * slot,
            textFmtBar_.top,
            textFmtBar_.left + 5 * slot + btn,
            textFmtBar_.bottom
        };
        SolidBrush bg(Color(230, 35, 35, 35));
        Pen border(Color(220, 210, 210, 210), 1.0f);
        tg.FillRectangle(&bg, (INT)crc.left, (INT)crc.top, (INT)(crc.right - crc.left), (INT)(crc.bottom - crc.top));
        tg.DrawRectangle(&border, (INT)crc.left, (INT)crc.top, (INT)(crc.right - crc.left - 1), (INT)(crc.bottom - crc.top - 1));
        const int m = DpiScale(6);
        HBRUSH sw = CreateSolidBrush(textStyle_.color);
        RECT inner{ crc.left + m, crc.top + m, crc.right - m, crc.bottom - m };
        FillRect(memDc, &inner, sw);
        DeleteObject(sw);
        FrameRect(memDc, &inner, (HBRUSH)GetStockObject(WHITE_BRUSH));
    }

    DrawSnapGuides(memDc);
    DrawTooltip(memDc);

    BitBlt(hdc, 0, 0, screen_.width, screen_.height, memDc, 0, 0, SRCCOPY);
    SelectObject(memDc, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDc);
    EndPaint(hwnd_, &ps);
}

void CaptureOverlay::UpdateToolbarTooltip(int clientX, int clientY)
{
    const int sx = clientX + screen_.x;
    const int sy = clientY + screen_.y;
    std::wstring next;
    POINT nextPos{ clientX + DpiScale(16), clientY + DpiScale(20) };

    int vIdx = -1;
    if (HitTestVerticalTools(sx, sy, &vIdx))
    {
        static const StrId tips[] = {
            StrId::TipPencil, StrId::TipHighlight, StrId::TipLine, StrId::TipArrow,
            StrId::TipRectangle, StrId::TipEllipse, StrId::TipText, StrId::TipStrokeColor,
            StrId::TipUndo, StrId::TipThinner, StrId::TipThicker
        };
        if (vIdx >= 0 && vIdx < 11)
            next = Loc::T(tips[vIdx]);
    }

    int aIdx = 0;
    if (next.empty() && HitTestToolbar(sx, sy, &aIdx))
    {
        if (aIdx == ActCopy) next = Loc::T(StrId::TipCopy);
        else if (aIdx == ActSave) next = Loc::T(StrId::TipSave);
        else if (aIdx == ActShare) next = Loc::T(StrId::TipShare);
        else if (aIdx == ActClose) next = Loc::T(StrId::TipClose);
    }

    if (next.empty() && textBoxActive_ && PtInRect(&textFmtBar_, { clientX, clientY }))
    {
        const int idx = (clientX - textFmtBar_.left) / uiFmtSlot_;
        static const StrId fmtTips[] = {
            StrId::TipBold, StrId::TipItalic, StrId::TipUnderline,
            StrId::TipFontInc, StrId::TipFontDec, StrId::TipTextColor
        };
        if (idx >= 0 && idx < 6)
            next = Loc::T(fmtTips[idx]);
    }

    const bool changed = (tipVisible_ != !next.empty()) || (tipText_ != next)
        || (tipVisible_ && (tipPos_.x != nextPos.x || tipPos_.y != nextPos.y));
    tipText_ = std::move(next);
    tipVisible_ = !tipText_.empty();
    tipPos_ = nextPos;
    if (changed)
        InvalidateRect(hwnd_, nullptr, FALSE);
}

void CaptureOverlay::DrawTooltip(HDC hdc) const
{
    if (!tipVisible_ || tipText_.empty())
        return;

    SIZE sz{};
    HFONT font = CreateFontW(DpiScale(16), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    HGDIOBJ oldFont = SelectObject(hdc, font);
    GetTextExtentPoint32W(hdc, tipText_.c_str(), (int)tipText_.size(), &sz);

    RECT rc{
        tipPos_.x,
        tipPos_.y,
        tipPos_.x + sz.cx + DpiScale(12),
        tipPos_.y + sz.cy + DpiScale(8)
    };
    if (rc.right > screen_.width)
        OffsetRect(&rc, screen_.width - rc.right - 4, 0);
    if (rc.bottom > screen_.height)
        OffsetRect(&rc, 0, screen_.height - rc.bottom - 4);
    if (rc.left < 0) OffsetRect(&rc, -rc.left, 0);
    if (rc.top < 0) OffsetRect(&rc, 0, -rc.top);

    HBRUSH br = CreateSolidBrush(RGB(30, 30, 30));
    FillRect(hdc, &rc, br);
    DeleteObject(br);
    FrameRect(hdc, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH));
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 255, 255));
    RECT tr = rc;
    InflateRect(&tr, -DpiScale(6), -DpiScale(4));
    DrawTextW(hdc, tipText_.c_str(), -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, oldFont);
    DeleteObject(font);
}

void CaptureOverlay::Notify(const wchar_t* text)
{
    if (Settings::Instance().Data().showNotifications && owner_)
        PostMessageW(owner_, WM_APP + 2, 0, reinterpret_cast<LPARAM>(_wcsdup(text)));
}

HBITMAP CaptureOverlay::BuildExportBitmap()
{
    if (textBoxActive_)
        FinishTextEdit(true);
    return Exporter::ComposeRegion(screen_.bitmap, screen_.x, screen_.y, NormalizeSelection(), doc_);
}

RECT CaptureOverlay::ClampTextBoxToSelection(RECT box) const
{
    box = NormRect(box);
    RECT sel = NormalizeSelection();
    const int w = (std::max)(uiMinTextW_, (int)(box.right - box.left));
    const int h = (std::max)(uiMinTextH_, (int)(box.bottom - box.top));
    if (box.left < sel.left) box.left = sel.left;
    if (box.top < sel.top) box.top = sel.top;
    box.right = box.left + w;
    box.bottom = box.top + h;
    if (box.right > sel.right)
    {
        box.right = sel.right;
        box.left = (std::max)(sel.left, box.right - w);
    }
    if (box.bottom > sel.bottom)
    {
        box.bottom = sel.bottom;
        box.top = (std::max)(sel.top, box.bottom - h);
    }
    return box;
}

void CaptureOverlay::UpdateTextChrome()
{
    if (!textBoxActive_)
    {
        textFmtBar_ = {};
        return;
    }
    RECT b = NormRect(textBox_);
    const int slot = uiFmtSlot_;
    const int barH = DpiScale(30);
    const int gap = DpiScale(4);
    textFmtBar_ = {
        b.left - screen_.x,
        b.top - screen_.y - slot,
        b.left - screen_.x + 6 * slot,
        b.top - screen_.y - gap
    };
    if (textFmtBar_.top < 0)
    {
        textFmtBar_.top = b.bottom - screen_.y + gap;
        textFmtBar_.bottom = textFmtBar_.top + barH;
    }
}

void CaptureOverlay::ApplyTextFont()
{
    if (!textEdit_)
        return;
    HFONT old = (HFONT)GetWindowLongPtrW(textEdit_, GWLP_USERDATA);
    HFONT font = CreateFontW(
        DpiScale(textStyle_.fontSize + 4), 0, 0, 0,
        textStyle_.bold ? FW_BOLD : FW_NORMAL,
        textStyle_.italic ? TRUE : FALSE,
        textStyle_.underline ? TRUE : FALSE,
        FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    SendMessageW(textEdit_, WM_SETFONT, (WPARAM)font, TRUE);
    SetWindowLongPtrW(textEdit_, GWLP_USERDATA, (LONG_PTR)font);
    if (old)
        DeleteObject(old);
}

void CaptureOverlay::LayoutTextEditor()
{
    if (!textEdit_ || !textBoxActive_)
        return;
    RECT b = NormRect(textBox_);
    SetWindowPos(textEdit_, nullptr,
        b.left - screen_.x + 2,
        b.top - screen_.y + 2,
        (std::max)(20, (int)(b.right - b.left) - 4),
        (std::max)(18, (int)(b.bottom - b.top) - 4),
        SWP_NOZORDER);
    UpdateTextChrome();
}

void CaptureOverlay::DrawTextBoxFrame(HDC hdc)
{
    RECT b = NormRect(textBox_);
    OffsetRect(&b, -screen_.x, -screen_.y);

    HPEN pen = CreatePen(PS_DOT, 1, RGB(0, 180, 255));
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    HGDIOBJ oldBr = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, b.left, b.top, b.right, b.bottom);
    SelectObject(hdc, oldBr);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);

    if (textBoxActive_)
    {
        // Move grip (top bar) + resize handle (bottom-right)
        const int gripH = DpiScale(10);
        RECT grip{ b.left, b.top - gripH, b.right, b.top };
        HBRUSH gbr = CreateSolidBrush(RGB(0, 140, 220));
        FillRect(hdc, &grip, gbr);
        DeleteObject(gbr);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));
        DrawTextW(hdc, Loc::T(StrId::TextDragGrip), -1, &grip, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        const int hs = uiHandle_;
        RECT hr{ b.right - hs, b.bottom - hs, b.right + 1, b.bottom + 1 };
        FillRect(hdc, &hr, (HBRUSH)GetStockObject(WHITE_BRUSH));
        FrameRect(hdc, &hr, (HBRUSH)GetStockObject(BLACK_BRUSH));
    }
}

int CaptureOverlay::HitTestTextBoxHandle(int x, int y) const
{
    if (!textBoxActive_)
        return TextHitNone;
    RECT b = NormRect(textBox_);
    const int gripH = DpiScale(10);
    const int hs = DpiScale(10);
    RECT grip{ b.left, b.top - gripH, b.right, b.top + DpiScale(4) };
    RECT corner{ b.right - hs, b.bottom - hs, b.right + DpiScale(4), b.bottom + DpiScale(4) };
    if (PtIn(corner, x, y))
        return TextHitResizeBR;
    if (PtIn(grip, x, y) || (PtIn(b, x, y) && !textEdit_))
        return TextHitMove;
    // Border move (not deep inside edit)
    RECT inner = b;
    InflateRect(&inner, -6, -6);
    if (PtIn(b, x, y) && !PtIn(inner, x, y))
        return TextHitMove;
    return TextHitNone;
}

void CaptureOverlay::BeginTextBox(const RECT& boxScreen, int editIndex, const std::wstring& initial)
{
    FinishTextEdit(false);

    textBox_ = ClampTextBoxToSelection(boxScreen);
    textBoxActive_ = true;
    editingTextIndex_ = editIndex;
    creatingTextBox_ = false;

    RECT b = NormRect(textBox_);
    textEdit_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", initial.c_str(),
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL,
        b.left - screen_.x + 2, b.top - screen_.y + 2,
        (std::max)(20, (int)(b.right - b.left) - 4),
        (std::max)(18, (int)(b.bottom - b.top) - 4),
        hwnd_, nullptr, hInst_, nullptr);

    ApplyTextFont();
    SendMessageW(textEdit_, EM_SETSEL, 0, -1);
    SetFocus(textEdit_);
    UpdateTextChrome();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void CaptureOverlay::EditExistingText(int index)
{
    const Annotation* a = doc_.Get(index);
    if (!a || a->type != ToolType::Text || a->locked)
        return;
    textStyle_ = a->textStyle;
    color_ = a->textStyle.color;
    BeginTextBox(a->bounds, index, a->text);
}

void CaptureOverlay::BeginTextAt(int x, int y)
{
    RECT box{ x, y, x + 180, y + 48 };
    BeginTextBox(box, -1, L"");
}

void CaptureOverlay::FinishTextEdit(bool commit)
{
    if (!textBoxActive_ && !textEdit_)
        return;

    if (commit && textEdit_)
    {
        int len = GetWindowTextLengthW(textEdit_);
        std::wstring text(len + 1, L'\0');
        GetWindowTextW(textEdit_, text.data(), len + 1);
        text.resize(len);

        if (!text.empty())
        {
            Annotation a;
            a.type = ToolType::Text;
            a.text = text;
            a.textStyle = textStyle_;
            a.textStyle.color = color_;
            a.color = color_;
            a.bounds = NormRect(textBox_);

            if (editingTextIndex_ >= 0)
            {
                doc_.UpdateAt(editingTextIndex_, std::move(a));
                SelectAnnotation(editingTextIndex_);
            }
            else
            {
                doc_.Push(std::move(a));
                SelectAnnotation(doc_.Count() - 1);
            }
        }
        else if (editingTextIndex_ >= 0)
        {
            doc_.RemoveAt(editingTextIndex_);
            ClearAnnSelection();
        }
    }

    if (textEdit_)
    {
        HFONT font = (HFONT)GetWindowLongPtrW(textEdit_, GWLP_USERDATA);
        DestroyWindow(textEdit_);
        textEdit_ = nullptr;
        if (font)
            DeleteObject(font);
    }

    textBoxActive_ = false;
    creatingTextBox_ = false;
    draggingTextBox_ = false;
    editingTextIndex_ = -1;
    textBox_ = {};
    textFmtBar_ = {};
    InvalidateRect(hwnd_, nullptr, FALSE);
}

int CaptureOverlay::DpiScale(int value) const
{
    return (int)std::lround(value * dpiScale_);
}

void CaptureOverlay::ApplyDpiMetrics(int dpi)
{
    dpi_ = (dpi > 0) ? dpi : 96;
    dpiScale_ = dpi_ / 96.0f;
    uiHandle_ = DpiScale(8);
    uiToolBtn_ = DpiScale(36);
    uiToolGap_ = DpiScale(5);
    uiFmtSlot_ = DpiScale(34);
    uiMinTextW_ = DpiScale(80);
    uiMinTextH_ = DpiScale(28);
}

void CaptureOverlay::PersistStroke() const
{
    Settings::Instance().Data().strokeWidth = strokeWidth_;
    Settings::Instance().Save();
}

void CaptureOverlay::AdjustStroke(float delta)
{
    strokeWidth_ += delta;
    if (strokeWidth_ < 1.0f) strokeWidth_ = 1.0f;
    if (strokeWidth_ > 12.0f) strokeWidth_ = 12.0f;
    // Snap to 0.5 steps
    strokeWidth_ = std::round(strokeWidth_ * 2.0f) / 2.0f;
    PersistStroke();

    if (selectedAnn_ >= 0)
    {
        Annotation* a = doc_.GetMutable(selectedAnn_);
        if (a && !a->locked && a->type != ToolType::Text)
        {
            a->thickness = strokeWidth_;
        }
    }
}

void CaptureOverlay::SelectAnnotation(int index)
{
    if (index < 0 || index >= doc_.Count())
    {
        ClearAnnSelection();
        return;
    }
    selectedAnn_ = index;
    if (const Annotation* a = doc_.Get(index))
    {
        if (a->type != ToolType::Text)
            strokeWidth_ = a->thickness;
    }
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void CaptureOverlay::ClearAnnSelection()
{
    selectedAnn_ = -1;
    transformingAnn_ = false;
    annXform_ = AnnNone;
}

void CaptureOverlay::NudgeSelected(int dx, int dy)
{
    Annotation* a = doc_.GetMutable(selectedAnn_);
    if (!a || a->locked)
        return;
    AnnGeom::Move(*a, dx, dy);
    InvalidateRect(hwnd_, nullptr, FALSE);
}

bool CaptureOverlay::IsSelectedLocked() const
{
    const Annotation* a = doc_.Get(selectedAnn_);
    return a && a->locked;
}

void CaptureOverlay::ToggleSelectedLock()
{
    Annotation* a = doc_.GetMutable(selectedAnn_);
    if (!a || textBoxActive_)
        return;
    a->locked = !a->locked;
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void CaptureOverlay::DuplicateSelected()
{
    if (selectedAnn_ < 0 || textBoxActive_)
        return;
    const Annotation* src = doc_.Get(selectedAnn_);
    if (!src)
        return;

    Annotation copy = *src;
    const int off = DpiScale(12);
    AnnGeom::Move(copy, off, off);
    doc_.Push(std::move(copy));
    SelectAnnotation(doc_.Count() - 1);
}

void CaptureOverlay::BringSelectedToFront()
{
    if (selectedAnn_ < 0 || textBoxActive_)
        return;
    const int next = doc_.BringToFront(selectedAnn_);
    if (next >= 0)
        selectedAnn_ = next;
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void CaptureOverlay::SendSelectedToBack()
{
    if (selectedAnn_ < 0 || textBoxActive_)
        return;
    const int next = doc_.SendToBack(selectedAnn_);
    if (next >= 0)
        selectedAnn_ = next;
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void CaptureOverlay::BringSelectedForward()
{
    if (selectedAnn_ < 0 || textBoxActive_)
        return;
    const int next = doc_.BringForward(selectedAnn_);
    if (next >= 0)
        selectedAnn_ = next;
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void CaptureOverlay::SendSelectedBackward()
{
    if (selectedAnn_ < 0 || textBoxActive_)
        return;
    const int next = doc_.SendBackward(selectedAnn_);
    if (next >= 0)
        selectedAnn_ = next;
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void CaptureOverlay::ClearSnapGuides()
{
    snapGuideX_ = INT_MIN;
    snapGuideY_ = INT_MIN;
}

POINT CaptureOverlay::SnapAnnotationDelta(const Annotation& moved, int skipIndex, POINT delta)
{
    ClearSnapGuides();

    // Hold Alt to temporarily disable snapping.
    if (GetKeyState(VK_MENU) & 0x8000)
        return delta;

    Annotation probe = moved;
    AnnGeom::Move(probe, delta.x, delta.y);
    RECT box = AnnGeom::GetWorldAabb(probe);

    std::vector<int> xs;
    std::vector<int> ys;
    xs.reserve(32);
    ys.reserve(32);

    auto addRect = [&](RECT r) {
        r = AnnGeom::Norm(r);
        xs.push_back(r.left);
        xs.push_back((r.left + r.right) / 2);
        xs.push_back(r.right);
        ys.push_back(r.top);
        ys.push_back((r.top + r.bottom) / 2);
        ys.push_back(r.bottom);
    };

    if (HasSelection())
        addRect(NormalizeSelection());

    for (int i = 0; i < doc_.Count(); ++i)
    {
        if (i == skipIndex)
            continue;
        if (const Annotation* other = doc_.Get(i))
            addRect(AnnGeom::GetWorldAabb(*other));
    }

    const int thresh = DpiScale(8);
    const int mx[3] = { box.left, (box.left + box.right) / 2, box.right };
    const int my[3] = { box.top, (box.top + box.bottom) / 2, box.bottom };

    auto bestAxis = [&](const int moving[3], const std::vector<int>& guides, int* outGuide) -> int {
        int best = 0;
        int bestAbs = thresh + 1;
        int guide = INT_MIN;
        for (int i = 0; i < 3; ++i)
        {
            const int m = moving[i];
            for (int g : guides)
            {
                const int d = g - m;
                const int ad = std::abs(d);
                if (ad < bestAbs)
                {
                    bestAbs = ad;
                    best = d;
                    guide = g;
                }
            }
        }
        if (bestAbs <= thresh)
        {
            *outGuide = guide;
            return best;
        }
        *outGuide = INT_MIN;
        return 0;
    };

    delta.x += bestAxis(mx, xs, &snapGuideX_);
    delta.y += bestAxis(my, ys, &snapGuideY_);
    return delta;
}

void CaptureOverlay::DrawSnapGuides(HDC hdc) const
{
    if (!HasSelection())
        return;
    RECT sel = NormalizeSelection();
    HPEN pen = CreatePen(PS_DOT, 1, RGB(0, 200, 255));
    HGDIOBJ old = SelectObject(hdc, pen);
    if (snapGuideX_ != INT_MIN)
    {
        const int x = snapGuideX_ - screen_.x;
        MoveToEx(hdc, x, sel.top - screen_.y, nullptr);
        LineTo(hdc, x, sel.bottom - screen_.y);
    }
    if (snapGuideY_ != INT_MIN)
    {
        const int y = snapGuideY_ - screen_.y;
        MoveToEx(hdc, sel.left - screen_.x, y, nullptr);
        LineTo(hdc, sel.right - screen_.x, y);
    }
    SelectObject(hdc, old);
    DeleteObject(pen);
}

int CaptureOverlay::HitTestAnnTransform(int x, int y) const
{
    if (selectedAnn_ < 0)
        return AnnNone;
    const Annotation* a = doc_.Get(selectedAnn_);
    if (!a || a->locked)
        return AnnNone;

    POINT c = AnnGeom::GetCenter(*a);
    POINT local = AnnGeom::UnrotatePoint({ x, y }, c, a->rotation);
    RECT b = AnnGeom::GetBounds(*a);
    const int midX = (b.left + b.right) / 2;
    const int midY = (b.top + b.bottom) / 2;

    const int ht = (std::max)(7, uiHandle_ / 2 + 1);
    auto hit = [&](int hx, int hy) {
        return std::abs(local.x - hx) <= ht && std::abs(local.y - hy) <= ht;
    };

    // Rotation handle
    if (std::abs(local.x - midX) <= uiHandle_ && std::abs(local.y - (b.top - DpiScale(28))) <= uiHandle_)
        return AnnRotate;

    if (hit(b.left, b.top)) return AnnResizeTL;
    if (hit(midX, b.top)) return AnnResizeT;
    if (hit(b.right, b.top)) return AnnResizeTR;
    if (hit(b.left, midY)) return AnnResizeL;
    if (hit(b.right, midY)) return AnnResizeR;
    if (hit(b.left, b.bottom)) return AnnResizeBL;
    if (hit(midX, b.bottom)) return AnnResizeB;
    if (hit(b.right, b.bottom)) return AnnResizeBR;

    if (AnnGeom::HitTest(*a, x, y))
        return AnnMove;

    return AnnNone;
}

void CaptureOverlay::OnLButtonDown(int x, int y)
{
    // Convert to screen coords (window client is virtual-screen relative)
    int sx = x + screen_.x;
    int sy = y + screen_.y;

    // Text format bar
    if (textBoxActive_ && PtInRect(&textFmtBar_, { x, y }))
    {
        int idx = (x - textFmtBar_.left) / uiFmtSlot_;
        if (idx == 0) textStyle_.bold = !textStyle_.bold;
        if (idx == 1) textStyle_.italic = !textStyle_.italic;
        if (idx == 2) textStyle_.underline = !textStyle_.underline;
        if (idx == 3) textStyle_.fontSize = (std::min)(72, textStyle_.fontSize + 2);
        if (idx == 4) textStyle_.fontSize = (std::max)(8, textStyle_.fontSize - 2);
        if (idx == 5)
        {
            CHOOSECOLORW cc{};
            static COLORREF cust[16]{};
            cc.lStructSize = sizeof(cc);
            cc.hwndOwner = hwnd_;
            cc.lpCustColors = cust;
            cc.rgbResult = textStyle_.color;
            cc.Flags = CC_FULLOPEN | CC_RGBINIT;
            if (ChooseColorW(&cc))
            {
                textStyle_.color = cc.rgbResult;
                if (textEdit_)
                    InvalidateRect(textEdit_, nullptr, TRUE);
            }
        }
        auto& s = Settings::Instance().Data();
        s.fontBold = textStyle_.bold;
        s.fontItalic = textStyle_.italic;
        s.fontUnderline = textStyle_.underline;
        s.fontSize = textStyle_.fontSize;
        ApplyTextFont();
        InvalidateRect(hwnd_, nullptr, FALSE);
        return;
    }

    // Move / resize active text box
    if (textBoxActive_)
    {
        int th = HitTestTextBoxHandle(sx, sy);
        if (th != TextHitNone)
        {
            draggingTextBox_ = true;
            textHitZone_ = th;
            dragStart_ = { sx, sy };
            textBoxAtDrag_ = NormRect(textBox_);
            SetCapture(hwnd_);
            return;
        }

        RECT b = NormRect(textBox_);
        if (PtIn(b, sx, sy))
        {
            // Click inside editor � let EDIT handle it
            SetFocus(textEdit_);
            return;
        }

        // Outside text box ? commit, then continue with click
        FinishTextEdit(true);
    }

    // Transform handles / move of selected annotation
    if (selectedAnn_ >= 0)
    {
        int ax = HitTestAnnTransform(sx, sy);
        if (ax != AnnNone)
        {
            if (ax == AnnMove && currentTool_ == ToolType::Text)
            {
                const Annotation* a = doc_.Get(selectedAnn_);
                if (a && a->type == ToolType::Text)
                {
                    EditExistingText(selectedAnn_);
                    return;
                }
            }
            transformingAnn_ = true;
            annXform_ = ax;
            dragStart_ = { sx, sy };
            annAtDrag_ = *doc_.Get(selectedAnn_);
            if (ax == AnnRotate)
            {
                POINT c = AnnGeom::GetCenter(annAtDrag_);
                rotateStartAngle_ = annAtDrag_.rotation;
                rotatePointerStart_ = atan2f((float)(sy - c.y), (float)(sx - c.x)) * 180.f / 3.14159265f;
            }
            SetCapture(hwnd_);
            return;
        }
    }

    int action = 0;
    if (HitTestToolbar(sx, sy, &action))
    {
        if (textBoxActive_)
            FinishTextEdit(true);
        if (action == ActClose)
        {
            result_ = OverlayResult::Cancelled;
            done_ = true;
            DestroyWindow(hwnd_);
            return;
        }
        if (action == ActCopy || action == ActSave || action == ActShare)
        {
            if (!HasSelection())
                return;

            auto& settings = Settings::Instance().Data();
            settings.lastSelection = NormalizeSelection();
            settings.hasLastSelection = true;
            Settings::Instance().Save();

            if (action == ActCopy)
                DoCopySelection();
            else if (action == ActSave)
                DoSaveSelection();
            else
                ShowShareMenu(x, y);
        }
        return;
    }

    int toolIdx = 0;
    if (HitTestVerticalTools(sx, sy, &toolIdx))
    {
        switch (toolIdx)
        {
        case 0: currentTool_ = ToolType::Pencil; break;
        case 1: currentTool_ = ToolType::Highlight; break;
        case 2: currentTool_ = ToolType::Line; break;
        case 3: currentTool_ = ToolType::Arrow; break;
        case 4: currentTool_ = ToolType::Rectangle; break;
        case 5: currentTool_ = ToolType::Ellipse; break;
        case 6: currentTool_ = ToolType::Text; break;
        case 7:
        {
            CHOOSECOLORW cc{};
            static COLORREF cust[16]{};
            cc.lStructSize = sizeof(cc);
            cc.hwndOwner = hwnd_;
            cc.lpCustColors = cust;
            cc.rgbResult = color_;
            cc.Flags = CC_FULLOPEN | CC_RGBINIT;
            if (ChooseColorW(&cc))
            {
                color_ = cc.rgbResult;
                textStyle_.color = color_;
                Settings::Instance().Data().drawColor = color_;
            }
            break;
        }
        case 8:
            // Click = Undo; Ctrl+Click = Redo
            if (GetKeyState(VK_CONTROL) & 0x8000)
            {
                if (doc_.Redo() && doc_.Count() > 0)
                    SelectAnnotation(doc_.Count() - 1);
                else
                    ClearAnnSelection();
            }
            else
            {
                doc_.Undo();
                ClearAnnSelection();
            }
            break;
        case 9:
            AdjustStroke(-0.5f);
            break;
        case 10:
            AdjustStroke(+0.5f);
            break;
        }
        InvalidateRect(hwnd_, nullptr, FALSE);
        return;
    }

    int hit = HitTestHandle(sx, sy);
    if (hit >= HitResizeTL && hit <= HitResizeBR)
    {
        dragging_ = true;
        hitZone_ = hit;
        dragStart_ = { sx, sy };
        selectionAtDrag_ = selection_;
        SetCapture(hwnd_);
        return;
    }

    // Thin border (~6px) moves selection; interior uses drawing tools.
    if (HasSelection())
    {
        RECT sel = NormalizeSelection();
        RECT inner = sel;
        InflateRect(&inner, -6, -6);
        bool onBorder = PtIn(sel, sx, sy) && !PtIn(inner, sx, sy);
        if (onBorder || (hit == HitMove && (GetKeyState(VK_SPACE) & 0x8000)))
        {
            dragging_ = true;
            hitZone_ = HitMove;
            dragStart_ = { sx, sy };
            selectionAtDrag_ = selection_;
            SetCapture(hwnd_);
            return;
        }

        if (PtIn(sel, sx, sy))
        {
            // Click existing shape ? select (double path: Text tool opens editor)
            int hitAnn = doc_.HitTestAny(sx, sy);
            if (hitAnn >= 0)
            {
                SelectAnnotation(hitAnn);
                const Annotation* hitA = doc_.Get(hitAnn);
                if (hitA && hitA->locked)
                    return;
                if (currentTool_ == ToolType::Text && hitA && hitA->type == ToolType::Text)
                {
                    EditExistingText(hitAnn);
                    return;
                }
                // Start move immediately
                transformingAnn_ = true;
                annXform_ = AnnMove;
                dragStart_ = { sx, sy };
                annAtDrag_ = *hitA;
                SetCapture(hwnd_);
                return;
            }

            ClearAnnSelection();

            if (currentTool_ == ToolType::Text)
            {
                creatingTextBox_ = true;
                textBoxActive_ = false;
                textBox_ = { sx, sy, sx, sy };
                SetCapture(hwnd_);
                InvalidateRect(hwnd_, nullptr, FALSE);
                return;
            }

            if (currentTool_ != ToolType::None)
            {
                drafting_ = true;
                draft_ = {};
                draft_.type = currentTool_;
                draft_.color = color_;
                draft_.thickness = strokeWidth_;
                draft_.points.push_back({ sx, sy });
                if (currentTool_ == ToolType::Line || currentTool_ == ToolType::Arrow)
                    draft_.points.push_back({ sx, sy });
                draft_.bounds = { sx, sy, sx, sy };
                SetCapture(hwnd_);
                return;
            }
        }
    }

    // Start new selection
    selecting_ = true;
    hasSelection_ = true;
    hitZone_ = HitCreating;
    selection_ = { sx, sy, sx, sy };
    doc_.Clear();
    ClearAnnSelection();
    SetCapture(hwnd_);
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void CaptureOverlay::OnMouseMove(int x, int y)
{
    int sx = x + screen_.x;
    int sy = y + screen_.y;

    const bool busy = creatingTextBox_ || transformingAnn_ || draggingTextBox_
        || drafting_ || selecting_ || dragging_;
    if (!busy)
        UpdateToolbarTooltip(x, y);
    else if (tipVisible_)
    {
        tipVisible_ = false;
        tipText_.clear();
    }

    if (creatingTextBox_)
    {
        textBox_.right = sx;
        textBox_.bottom = sy;
        InvalidateRect(hwnd_, nullptr, FALSE);
        return;
    }

    if (transformingAnn_ && selectedAnn_ >= 0)
    {
        Annotation* a = doc_.GetMutable(selectedAnn_);
        if (!a)
            return;

        if (annXform_ == AnnRotate)
        {
            POINT c = AnnGeom::GetCenter(annAtDrag_);
            float ang = atan2f((float)(sy - c.y), (float)(sx - c.x)) * 180.f / 3.14159265f;
            a->rotation = rotateStartAngle_ + (ang - rotatePointerStart_);
            // Keep geometry from drag start
            a->bounds = annAtDrag_.bounds;
            a->points = annAtDrag_.points;
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        }

        int dx = sx - dragStart_.x;
        int dy = sy - dragStart_.y;

        if (annXform_ == AnnMove)
        {
            *a = annAtDrag_;
            POINT snapped = SnapAnnotationDelta(annAtDrag_, selectedAnn_, { dx, dy });
            AnnGeom::Move(*a, snapped.x, snapped.y);
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        }

        // Resize in local (unrotated) space of the original annotation
        POINT c = AnnGeom::GetCenter(annAtDrag_);
        POINT localStart = AnnGeom::UnrotatePoint(dragStart_, c, annAtDrag_.rotation);
        POINT localNow = AnnGeom::UnrotatePoint({ sx, sy }, c, annAtDrag_.rotation);
        int ldx = localNow.x - localStart.x;
        int ldy = localNow.y - localStart.y;

        RECT r = AnnGeom::GetBounds(annAtDrag_);
        switch (annXform_)
        {
        case AnnResizeTL: r.left += ldx; r.top += ldy; break;
        case AnnResizeT: r.top += ldy; break;
        case AnnResizeTR: r.right += ldx; r.top += ldy; break;
        case AnnResizeL: r.left += ldx; break;
        case AnnResizeR: r.right += ldx; break;
        case AnnResizeBL: r.left += ldx; r.bottom += ldy; break;
        case AnnResizeB: r.bottom += ldy; break;
        case AnnResizeBR: r.right += ldx; r.bottom += ldy; break;
        default: break;
        }
        r = AnnGeom::Norm(r);
        if (r.right - r.left < 8) r.right = r.left + 8;
        if (r.bottom - r.top < 8) r.bottom = r.top + 8;

        *a = annAtDrag_;
        AnnGeom::SetBoundsFromRect(*a, r);
        InvalidateRect(hwnd_, nullptr, FALSE);
        return;
    }

    if (draggingTextBox_)
    {
        int dx = sx - dragStart_.x;
        int dy = sy - dragStart_.y;
        RECT r = textBoxAtDrag_;
        if (textHitZone_ == TextHitMove)
            OffsetRect(&r, dx, dy);
        else if (textHitZone_ == TextHitResizeBR)
        {
            r.right = textBoxAtDrag_.right + dx;
            r.bottom = textBoxAtDrag_.bottom + dy;
        }
        textBox_ = ClampTextBoxToSelection(r);
        LayoutTextEditor();
        InvalidateRect(hwnd_, nullptr, FALSE);
        return;
    }

    if (drafting_)
    {
        if (draft_.type == ToolType::Pencil || draft_.type == ToolType::Highlight)
        {
            draft_.points.push_back({ sx, sy });
        }
        else if (draft_.type == ToolType::Line || draft_.type == ToolType::Arrow)
        {
            if (draft_.points.size() < 2)
                draft_.points.push_back({ sx, sy });
            else
                draft_.points[1] = { sx, sy };
        }
        else
        {
            draft_.bounds.right = sx;
            draft_.bounds.bottom = sy;
        }
        InvalidateRect(hwnd_, nullptr, FALSE);
        return;
    }

    if (selecting_)
    {
        selection_.right = sx;
        selection_.bottom = sy;
        InvalidateRect(hwnd_, nullptr, FALSE);
        return;
    }

    if (dragging_)
    {
        int dx = sx - dragStart_.x;
        int dy = sy - dragStart_.y;
        RECT r = selectionAtDrag_;
        switch (hitZone_)
        {
        case HitMove:
            OffsetRect(&r, dx, dy);
            break;
        case HitResizeTL: r.left += dx; r.top += dy; break;
        case HitResizeT: r.top += dy; break;
        case HitResizeTR: r.right += dx; r.top += dy; break;
        case HitResizeL: r.left += dx; break;
        case HitResizeR: r.right += dx; break;
        case HitResizeBL: r.left += dx; r.bottom += dy; break;
        case HitResizeB: r.bottom += dy; break;
        case HitResizeBR: r.right += dx; r.bottom += dy; break;
        }
        selection_ = r;
        UpdateToolbarsLayout();
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void CaptureOverlay::OnLButtonUp(int x, int y)
{
    int sx = x + screen_.x;
    int sy = y + screen_.y;

    if (creatingTextBox_)
    {
        creatingTextBox_ = false;
        ReleaseCapture();
        RECT box = NormRect(textBox_);
        if ((box.right - box.left) < uiMinTextW_ || (box.bottom - box.top) < uiMinTextH_)
            box = { sx, sy, sx + 180, sy + 48 };
        BeginTextBox(box, -1, L"");
        // Will select after commit
        return;
    }

    if (transformingAnn_)
    {
        transformingAnn_ = false;
        annXform_ = AnnNone;
        ClearSnapGuides();
        ReleaseCapture();
        InvalidateRect(hwnd_, nullptr, FALSE);
        return;
    }

    if (draggingTextBox_)
    {
        draggingTextBox_ = false;
        textHitZone_ = TextHitNone;
        ReleaseCapture();
        textBox_ = ClampTextBoxToSelection(textBox_);
        LayoutTextEditor();
        InvalidateRect(hwnd_, nullptr, FALSE);
        return;
    }

    if (drafting_)
    {
        drafting_ = false;
        ReleaseCapture();
        bool valid = true;
        if (draft_.type == ToolType::Pencil || draft_.type == ToolType::Highlight)
            valid = draft_.points.size() >= 1;
        else if (draft_.type == ToolType::Line || draft_.type == ToolType::Arrow)
            valid = draft_.points.size() >= 2;
        else
        {
            RECT n = NormRect(draft_.bounds);
            valid = (n.right - n.left) > 1 || (n.bottom - n.top) > 1;
            draft_.bounds = n;
        }
        if (valid)
        {
            doc_.Push(std::move(draft_));
            SelectAnnotation(doc_.Count() - 1);
        }
        draft_ = {};
        InvalidateRect(hwnd_, nullptr, FALSE);
        return;
    }

    if (selecting_)
    {
        selecting_ = false;
        ReleaseCapture();
        selection_ = NormalizeSelection();
        hasSelection_ = HasSelection();
        UpdateToolbarsLayout();
        InvalidateRect(hwnd_, nullptr, FALSE);
        return;
    }

    if (dragging_)
    {
        dragging_ = false;
        ReleaseCapture();
        selection_ = NormalizeSelection();
        UpdateToolbarsLayout();
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void CaptureOverlay::OnRButtonDown(int x, int y)
{
    if (textBoxActive_)
        FinishTextEdit(true);
    ShowShortcutMenu(x, y);
}

void CaptureOverlay::ShowShortcutMenu(int clientX, int clientY)
{
    HMENU menu = CreatePopupMenu();
    if (!menu)
        return;

    const bool hasSel = HasSelection();
    const bool hasAnn = selectedAnn_ >= 0 && !textBoxActive_;
    const bool locked = IsSelectedLocked();
    const bool canEditAnn = hasAnn && !locked;

    auto add = [&](UINT id, const wchar_t* text, bool enabled) {
        AppendMenuW(menu, MF_STRING | (enabled ? MF_ENABLED : MF_GRAYED), id, text);
    };
    auto sep = [&]() { AppendMenuW(menu, MF_SEPARATOR, 0, nullptr); };
    auto hint = [&](const wchar_t* text) {
        AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, text);
    };

    add(IDM_OV_COPY, Loc::T(StrId::MenuCopy), hasSel);
    add(IDM_OV_SAVE, Loc::T(StrId::MenuSave), hasSel);
    add(IDM_OV_PRINT, Loc::T(StrId::MenuPrint), hasSel);

    HMENU share = CreatePopupMenu();
    const bool canUpload = hasSel && Settings::Instance().Data().enableUpload;
    const bool hasLink = !Settings::Instance().Data().lastUploadUrl.empty();
    AppendMenuW(share, MF_STRING | (canUpload ? MF_ENABLED : MF_GRAYED),
        IDM_OV_UPLOAD, Loc::T(StrId::MenuUpload));
    AppendMenuW(share, MF_STRING | (hasLink ? MF_ENABLED : MF_GRAYED),
        IDM_OV_COPY_LINK, Loc::T(StrId::MenuCopyLink));
    AppendMenuW(share, MF_STRING | (hasLink ? MF_ENABLED : MF_GRAYED),
        IDM_OV_OPEN_LINK, Loc::T(StrId::MenuOpenLink));
    AppendMenuW(share, MF_STRING | (canUpload ? MF_ENABLED : MF_GRAYED),
        IDM_OV_SEARCH_IMAGE, Loc::T(StrId::MenuSearchImage));
    AppendMenuW(menu, MF_POPUP | (hasSel ? MF_ENABLED : MF_GRAYED), (UINT_PTR)share, Loc::T(StrId::MenuShare));
    sep();
    add(IDM_OV_UNDO, Loc::T(StrId::MenuUndo), doc_.Count() > 0);
    add(IDM_OV_REDO, Loc::T(StrId::MenuRedo), doc_.CanRedo());
    sep();
    add(IDM_OV_DUPLICATE, Loc::T(StrId::MenuDuplicate), hasAnn);
    add(IDM_OV_LOCK, locked ? Loc::T(StrId::MenuUnlock) : Loc::T(StrId::MenuLock), hasAnn);
    add(IDM_OV_DELETE, Loc::T(StrId::MenuDelete), hasAnn);
    sep();
    add(IDM_OV_BRING_FORWARD, Loc::T(StrId::MenuBringForward), hasAnn);
    add(IDM_OV_SEND_BACKWARD, Loc::T(StrId::MenuSendBackward), hasAnn);
    add(IDM_OV_BRING_FRONT, Loc::T(StrId::MenuBringFront), hasAnn);
    add(IDM_OV_SEND_BACK, Loc::T(StrId::MenuSendBack), hasAnn);
    sep();
    add(IDM_OV_ROTATE_LEFT, Loc::T(StrId::MenuRotateLeft), canEditAnn);
    add(IDM_OV_ROTATE_RIGHT, Loc::T(StrId::MenuRotateRight), canEditAnn);
    sep();
    hint(Loc::T(StrId::MenuHintNudge));
    hint(Loc::T(StrId::MenuHintCycle));
    hint(Loc::T(StrId::MenuHintSnap));
    hint(Loc::T(StrId::MenuHintCommitText));
    sep();
    add(IDM_OV_CANCEL, Loc::T(StrId::MenuCancel), true);

    POINT pt{ clientX, clientY };
    ClientToScreen(hwnd_, &pt);
    const UINT cmd = TrackPopupMenu(
        menu,
        TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_LEFTALIGN | TPM_TOPALIGN,
        pt.x, pt.y, 0, hwnd_, nullptr);
    DestroyMenu(menu);

    if (cmd)
        OnContextCommand(cmd);
}

bool CaptureOverlay::DoCopySelection()
{
    if (!HasSelection())
        return false;
    HBITMAP out = BuildExportBitmap();
    if (!out)
        return false;
    bool ok = false;
    if (Exporter::CopyBitmapToClipboard(hwnd_, out))
    {
        Notify(Loc::T(StrId::NotifyCopied));
        result_ = OverlayResult::Copied;
        done_ = true;
        DestroyWindow(hwnd_);
        ok = true;
    }
    DeleteObject(out);
    return ok;
}

bool CaptureOverlay::DoSaveSelection()
{
    if (!HasSelection())
        return false;
    HBITMAP out = BuildExportBitmap();
    if (!out)
        return false;
    bool ok = false;
    if (Exporter::SaveBitmapToFile(hwnd_, out, Settings::Instance().DefaultSaveFolder().c_str()))
    {
        Notify(Loc::T(StrId::NotifySaved));
        result_ = OverlayResult::Saved;
        done_ = true;
        DestroyWindow(hwnd_);
        ok = true;
    }
    DeleteObject(out);
    return ok;
}

void CaptureOverlay::ShowShareMenu(int clientX, int clientY)
{
    HMENU menu = CreatePopupMenu();
    if (!menu)
        return;

    const bool hasSel = HasSelection();
    const bool canUpload = hasSel && Settings::Instance().Data().enableUpload;
    const bool hasLink = !Settings::Instance().Data().lastUploadUrl.empty();

    AppendMenuW(menu, MF_STRING | (canUpload ? MF_ENABLED : MF_GRAYED),
        IDM_OV_UPLOAD, Loc::T(StrId::MenuUpload));
    AppendMenuW(menu, MF_STRING | (hasLink ? MF_ENABLED : MF_GRAYED),
        IDM_OV_COPY_LINK, Loc::T(StrId::MenuCopyLink));
    AppendMenuW(menu, MF_STRING | (hasLink ? MF_ENABLED : MF_GRAYED),
        IDM_OV_OPEN_LINK, Loc::T(StrId::MenuOpenLink));
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING | (canUpload ? MF_ENABLED : MF_GRAYED),
        IDM_OV_SEARCH_IMAGE, Loc::T(StrId::MenuSearchImage));
    AppendMenuW(menu, MF_STRING | (hasSel ? MF_ENABLED : MF_GRAYED),
        IDM_OV_PRINT, Loc::T(StrId::MenuPrint));

    POINT pt{ clientX, clientY };
    ClientToScreen(hwnd_, &pt);
    const UINT cmd = TrackPopupMenu(
        menu,
        TPM_RETURNCMD | TPM_LEFTBUTTON | TPM_LEFTALIGN | TPM_TOPALIGN,
        pt.x, pt.y, 0, hwnd_, nullptr);
    DestroyMenu(menu);
    if (cmd)
        OnContextCommand(cmd);
}

bool CaptureOverlay::DoUploadSelection()
{
    if (!HasSelection())
        return false;
    if (!Settings::Instance().Data().enableUpload)
    {
        Notify(Loc::T(StrId::NotifyUploadDisabled));
        return false;
    }

    HBITMAP out = BuildExportBitmap();
    if (!out)
        return false;

    std::wstring url, err;
    const bool ok = Uploader::UploadBitmap(out, &url, &err);
    DeleteObject(out);
    if (!ok)
    {
        Notify(err.empty() ? Loc::T(StrId::NotifyUploadFailed) : err.c_str());
        return false;
    }

    Settings::Instance().Data().lastUploadUrl = url;
    Settings::Instance().Save();

    const bool copied = Exporter::CopyTextToClipboard(hwnd_, url.c_str());
    std::wstring msg = std::wstring(Loc::T(StrId::NotifyUploaded)) + L"\n" + url;
    if (copied)
        msg += std::wstring(L"\n") + Loc::T(StrId::NotifyLinkCopied);
    Notify(msg.c_str());
    result_ = OverlayResult::Uploaded;
    done_ = true;
    DestroyWindow(hwnd_);
    return true;
}

bool CaptureOverlay::DoCopyLastLink()
{
    const auto& url = Settings::Instance().Data().lastUploadUrl;
    if (url.empty())
        return false;
    if (!Exporter::CopyTextToClipboard(hwnd_, url.c_str()))
        return false;
    Notify(Loc::T(StrId::NotifyLinkCopied));
    return true;
}

bool CaptureOverlay::DoOpenLastLink()
{
    const auto& url = Settings::Instance().Data().lastUploadUrl;
    if (url.empty())
        return false;
    const HINSTANCE r = ShellExecuteW(hwnd_, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    return reinterpret_cast<INT_PTR>(r) > 32;
}

bool CaptureOverlay::DoPrintSelection()
{
    if (!HasSelection())
        return false;
    HBITMAP out = BuildExportBitmap();
    if (!out)
        return false;
    const bool ok = Exporter::PrintBitmap(hwnd_, out);
    DeleteObject(out);
    if (ok)
        Notify(Loc::T(StrId::NotifyPrinted));
    return ok;
}

bool CaptureOverlay::DoSearchByImage()
{
    if (!HasSelection())
        return false;
    if (!Settings::Instance().Data().enableUpload)
    {
        Notify(Loc::T(StrId::NotifyUploadDisabled));
        return false;
    }

    HBITMAP out = BuildExportBitmap();
    if (!out)
        return false;

    std::wstring url, err;
    const bool ok = Uploader::UploadBitmap(out, &url, &err);
    DeleteObject(out);
    if (!ok)
    {
        Notify(err.empty() ? Loc::T(StrId::NotifyUploadFailed) : err.c_str());
        return false;
    }

    Settings::Instance().Data().lastUploadUrl = url;
    Settings::Instance().Save();

    auto encode = [](const std::wstring& s) {
        std::wstring o;
        o.reserve(s.size() * 3);
        for (wchar_t c : s)
        {
            if ((c >= L'A' && c <= L'Z') || (c >= L'a' && c <= L'z') ||
                (c >= L'0' && c <= L'9') || c == L'-' || c == L'_' || c == L'.' || c == L'~')
            {
                o.push_back(c);
            }
            else if (c < 128)
            {
                wchar_t buf[8] = {};
                wsprintfW(buf, L"%%%02X", (unsigned)(unsigned char)c);
                o += buf;
            }
        }
        return o;
    };

    const std::wstring searchUrl = L"https://lens.google.com/uploadbyurl?url=" + encode(url);
    const HINSTANCE r = ShellExecuteW(hwnd_, L"open", searchUrl.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(r) <= 32)
        return false;

    Notify(Loc::T(StrId::NotifySearchOpened));
    return true;
}

void CaptureOverlay::DoUndo()
{
    doc_.Undo();
    ClearAnnSelection();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void CaptureOverlay::DoRedo()
{
    if (doc_.Redo())
    {
        ClearAnnSelection();
        if (doc_.Count() > 0)
            SelectAnnotation(doc_.Count() - 1);
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void CaptureOverlay::DoDeleteSelected()
{
    if (selectedAnn_ < 0 || textBoxActive_)
        return;
    int idx = selectedAnn_;
    ClearAnnSelection();
    doc_.RemoveAt(idx);
    if (doc_.Count() > 0)
        SelectAnnotation((std::min)(idx, doc_.Count() - 1));
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void CaptureOverlay::DoRotateSelected(float deltaDeg)
{
    if (IsSelectedLocked())
        return;
    if (Annotation* a = doc_.GetMutable(selectedAnn_))
    {
        a->rotation += deltaDeg;
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void CaptureOverlay::OnContextCommand(UINT cmd)
{
    switch (cmd)
    {
    case IDM_OV_COPY: DoCopySelection(); break;
    case IDM_OV_SAVE: DoSaveSelection(); break;
    case IDM_OV_PRINT: DoPrintSelection(); break;
    case IDM_OV_UPLOAD: DoUploadSelection(); break;
    case IDM_OV_COPY_LINK: DoCopyLastLink(); break;
    case IDM_OV_OPEN_LINK: DoOpenLastLink(); break;
    case IDM_OV_SEARCH_IMAGE: DoSearchByImage(); break;
    case IDM_OV_UNDO: DoUndo(); break;
    case IDM_OV_REDO: DoRedo(); break;
    case IDM_OV_DUPLICATE: DuplicateSelected(); break;
    case IDM_OV_LOCK: ToggleSelectedLock(); break;
    case IDM_OV_DELETE: DoDeleteSelected(); break;
    case IDM_OV_BRING_FORWARD: BringSelectedForward(); break;
    case IDM_OV_SEND_BACKWARD: SendSelectedBackward(); break;
    case IDM_OV_BRING_FRONT: BringSelectedToFront(); break;
    case IDM_OV_SEND_BACK: SendSelectedToBack(); break;
    case IDM_OV_ROTATE_LEFT: DoRotateSelected(-5.f); break;
    case IDM_OV_ROTATE_RIGHT: DoRotateSelected(+5.f); break;
    case IDM_OV_CANCEL:
        result_ = OverlayResult::Cancelled;
        done_ = true;
        DestroyWindow(hwnd_);
        break;
    default:
        break;
    }
}

bool CaptureOverlay::OnKeyDown(WPARAM vk)
{
    if (vk == VK_ESCAPE)
    {
        if (textEdit_)
        {
            FinishTextEdit(false);
            return true;
        }
        if (selectedAnn_ >= 0)
        {
            ClearAnnSelection();
            InvalidateRect(hwnd_, nullptr, FALSE);
            return true;
        }
        result_ = OverlayResult::Cancelled;
        done_ = true;
        DestroyWindow(hwnd_);
        return true;
    }
    if (vk == VK_RETURN && textEdit_ && (GetKeyState(VK_CONTROL) & 0x8000))
    {
        FinishTextEdit(true);
        return true;
    }
    if (vk == VK_TAB && doc_.Count() > 0 && !textBoxActive_)
    {
        int next = selectedAnn_;
        if (GetKeyState(VK_SHIFT) & 0x8000)
            next = (next <= 0) ? doc_.Count() - 1 : next - 1;
        else
            next = (next + 1) % doc_.Count();
        SelectAnnotation(next);
        return true;
    }
    if ((vk == VK_DELETE || vk == VK_BACK) && selectedAnn_ >= 0 && !textBoxActive_)
    {
        DoDeleteSelected();
        return true;
    }
    if (vk == VK_DELETE && textBoxActive_ && editingTextIndex_ >= 0
        && (GetKeyState(VK_SHIFT) & 0x8000))
    {
        int idx = editingTextIndex_;
        FinishTextEdit(false);
        doc_.RemoveAt(idx);
        ClearAnnSelection();
        InvalidateRect(hwnd_, nullptr, FALSE);
        return true;
    }
    if (selectedAnn_ >= 0 && !textBoxActive_)
    {
        if (vk == VK_LEFT) { NudgeSelected(-1, 0); return true; }
        if (vk == VK_RIGHT) { NudgeSelected(1, 0); return true; }
        if (vk == VK_UP) { NudgeSelected(0, -1); return true; }
        if (vk == VK_DOWN) { NudgeSelected(0, 1); return true; }
        if ((vk == 'Q' || vk == 'q') && !IsSelectedLocked())
        {
            DoRotateSelected(-5.f);
            return true;
        }
        if ((vk == 'E' || vk == 'e') && !IsSelectedLocked())
        {
            DoRotateSelected(+5.f);
            return true;
        }
    }
    if ((vk == 'Z') && (GetKeyState(VK_CONTROL) & 0x8000))
    {
        if (GetKeyState(VK_SHIFT) & 0x8000)
        {
            DoRedo();
            return true;
        }
        DoUndo();
        return true;
    }
    if ((vk == 'Y') && (GetKeyState(VK_CONTROL) & 0x8000))
    {
        DoRedo();
        return true;
    }
    if ((vk == 'D') && (GetKeyState(VK_CONTROL) & 0x8000))
    {
        DuplicateSelected();
        return true;
    }
    if ((vk == 'U') && (GetKeyState(VK_CONTROL) & 0x8000) && HasSelection())
    {
        DoUploadSelection();
        return true;
    }
    if ((vk == 'L') && (GetKeyState(VK_CONTROL) & 0x8000))
    {
        ToggleSelectedLock();
        return true;
    }
    // Z-order: Ctrl+] / Ctrl+[ one step; Ctrl+Shift+] / Ctrl+Shift+[ extreme
    if ((GetKeyState(VK_CONTROL) & 0x8000) && selectedAnn_ >= 0 && !textBoxActive_)
    {
        const bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        if (vk == VK_OEM_6) // ]
        {
            if (shift) BringSelectedToFront();
            else BringSelectedForward();
            return true;
        }
        if (vk == VK_OEM_4) // [
        {
            if (shift) SendSelectedToBack();
            else SendSelectedBackward();
            return true;
        }
        if (vk == VK_PRIOR) // Page Up
        {
            if (shift) BringSelectedToFront();
            else BringSelectedForward();
            return true;
        }
        if (vk == VK_NEXT) // Page Down
        {
            if (shift) SendSelectedToBack();
            else SendSelectedBackward();
            return true;
        }
    }
    if ((vk == 'C') && (GetKeyState(VK_CONTROL) & 0x8000) && HasSelection())
    {
        DoCopySelection();
        return true;
    }
    if ((vk == 'S') && (GetKeyState(VK_CONTROL) & 0x8000) && HasSelection())
    {
        DoSaveSelection();
        return true;
    }
    if ((vk == 'P') && (GetKeyState(VK_CONTROL) & 0x8000) && HasSelection())
    {
        DoPrintSelection();
        return true;
    }
    return false;
}

LRESULT CaptureOverlay::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_PAINT:
        OnPaint();
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_LBUTTONDOWN:
        OnLButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    case WM_LBUTTONUP:
        OnLButtonUp(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    case WM_MOUSEMOVE:
        OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    case WM_RBUTTONDOWN:
        OnRButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    case WM_COMMAND:
        if (HIWORD(wParam) == 0)
            OnContextCommand(LOWORD(wParam));
        return 0;
    case WM_KEYDOWN:
        if (OnKeyDown(wParam))
            return 0;
        break;
    case WM_CHAR:
        if (textEdit_)
            break;
        return 0;
    case WM_DESTROY:
        hwnd_ = nullptr;
        done_ = true;
        if (g_overlay == this)
            g_overlay = nullptr;
        return 0;
    case WM_SETCURSOR:
        SetCursor(LoadCursor(nullptr, IDC_CROSS));
        return TRUE;
    case WM_CTLCOLOREDIT:
        if (textEdit_ && (HWND)lParam == textEdit_)
        {
            HDC hdcEdit = (HDC)wParam;
            SetTextColor(hdcEdit, textStyle_.color);
            SetBkColor(hdcEdit, RGB(255, 255, 255));
            return (LRESULT)GetStockObject(WHITE_BRUSH);
        }
        break;
    }
    return DefWindowProcW(hwnd_, msg, wParam, lParam);
}
