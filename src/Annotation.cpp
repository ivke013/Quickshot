// Author: Ivan Stojmenovic
#include "framework.h"
#include "Annotation.h"

using namespace Gdiplus;

namespace
{
    float Deg2Rad(float d) { return d * 3.14159265f / 180.f; }

    bool PtInRectI(RECT r, int x, int y)
    {
        r = AnnGeom::Norm(r);
        return x >= r.left && x < r.right && y >= r.top && y < r.bottom;
    }

    float DistPointSeg(int x, int y, POINT a, POINT b)
    {
        const float vx = (float)(b.x - a.x);
        const float vy = (float)(b.y - a.y);
        const float wx = (float)(x - a.x);
        const float wy = (float)(y - a.y);
        const float c1 = vx * wx + vy * wy;
        if (c1 <= 0) return std::sqrt(wx * wx + wy * wy);
        const float c2 = vx * vx + vy * vy;
        if (c2 <= c1)
        {
            const float dx = (float)(x - b.x), dy = (float)(y - b.y);
            return std::sqrt(dx * dx + dy * dy);
        }
        const float t = c1 / c2;
        const float px = a.x + t * vx, py = a.y + t * vy;
        const float dx = x - px, dy = y - py;
        return std::sqrt(dx * dx + dy * dy);
    }
}

RECT AnnGeom::Norm(RECT r)
{
    if (r.right < r.left) std::swap(r.left, r.right);
    if (r.bottom < r.top) std::swap(r.top, r.bottom);
    return r;
}

RECT AnnGeom::GetBounds(const Annotation& a)
{
    if ((a.type == ToolType::Line || a.type == ToolType::Arrow) && a.points.size() >= 2)
    {
        RECT r{ a.points[0].x, a.points[0].y, a.points[1].x, a.points[1].y };
        return Norm(r);
    }
    if ((a.type == ToolType::Pencil || a.type == ToolType::Highlight) && !a.points.empty())
    {
        RECT r{ a.points[0].x, a.points[0].y, a.points[0].x, a.points[0].y };
        for (const auto& p : a.points)
        {
            r.left = (std::min)(r.left, p.x);
            r.top = (std::min)(r.top, p.y);
            r.right = (std::max)(r.right, p.x);
            r.bottom = (std::max)(r.bottom, p.y);
        }
        if (r.right == r.left) r.right++;
        if (r.bottom == r.top) r.bottom++;
        return r;
    }
    return Norm(a.bounds);
}

POINT AnnGeom::GetCenter(const Annotation& a)
{
    RECT b = GetBounds(a);
    return { (b.left + b.right) / 2, (b.top + b.bottom) / 2 };
}

POINT AnnGeom::RotatePoint(POINT p, POINT center, float deg)
{
    const float rad = Deg2Rad(deg);
    const float c = std::cos(rad), s = std::sin(rad);
    const float dx = (float)(p.x - center.x);
    const float dy = (float)(p.y - center.y);
    return {
        (LONG)std::lround(center.x + dx * c - dy * s),
        (LONG)std::lround(center.y + dx * s + dy * c)
    };
}

POINT AnnGeom::UnrotatePoint(POINT p, POINT center, float deg)
{
    return RotatePoint(p, center, -deg);
}

RECT AnnGeom::GetWorldAabb(const Annotation& a)
{
    RECT b = GetBounds(a);
    if (std::fabs(a.rotation) < 0.01f)
        return b;

    POINT c = GetCenter(a);
    POINT corners[4] = {
        { b.left, b.top }, { b.right, b.top },
        { b.right, b.bottom }, { b.left, b.bottom }
    };
    RECT out{ LONG_MAX, LONG_MAX, LONG_MIN, LONG_MIN };
    for (auto p : corners)
    {
        p = RotatePoint(p, c, a.rotation);
        out.left = (std::min)(out.left, p.x);
        out.top = (std::min)(out.top, p.y);
        out.right = (std::max)(out.right, p.x);
        out.bottom = (std::max)(out.bottom, p.y);
    }
    return out;
}

void AnnGeom::Move(Annotation& a, int dx, int dy)
{
    if (dx == 0 && dy == 0)
        return;
    OffsetRect(&a.bounds, dx, dy);
    for (auto& p : a.points)
    {
        p.x += dx;
        p.y += dy;
    }
}

void AnnGeom::SetBoundsFromRect(Annotation& a, RECT r)
{
    r = Norm(r);
    if ((a.type == ToolType::Line || a.type == ToolType::Arrow) && a.points.size() >= 2)
    {
        // Keep orientation roughly: map old bounds corners to new
        a.points[0] = { r.left, r.top };
        a.points[1] = { r.right, r.bottom };
        a.bounds = r;
        return;
    }
    if ((a.type == ToolType::Pencil || a.type == ToolType::Highlight) && a.points.size() >= 2)
    {
        RECT old = GetBounds(a);
        const float ow = (float)(std::max)(1L, old.right - old.left);
        const float oh = (float)(std::max)(1L, old.bottom - old.top);
        const float nw = (float)(std::max)(1L, r.right - r.left);
        const float nh = (float)(std::max)(1L, r.bottom - r.top);
        for (auto& p : a.points)
        {
            float u = (p.x - old.left) / ow;
            float v = (p.y - old.top) / oh;
            p.x = r.left + (LONG)std::lround(u * nw);
            p.y = r.top + (LONG)std::lround(v * nh);
        }
        a.bounds = r;
        return;
    }
    a.bounds = r;
}

void AnnGeom::ScaleAboutCenter(Annotation& a, float sx, float sy)
{
    POINT c = GetCenter(a);
    RECT b = GetBounds(a);
    POINT p1{ b.left, b.top }, p2{ b.right, b.bottom };
    p1.x = c.x + (LONG)std::lround((p1.x - c.x) * sx);
    p1.y = c.y + (LONG)std::lround((p1.y - c.y) * sy);
    p2.x = c.x + (LONG)std::lround((p2.x - c.x) * sx);
    p2.y = c.y + (LONG)std::lround((p2.y - c.y) * sy);
    SetBoundsFromRect(a, RECT{ p1.x, p1.y, p2.x, p2.y });
}

bool AnnGeom::HitTest(const Annotation& a, int x, int y)
{
    POINT c = GetCenter(a);
    POINT local = UnrotatePoint({ x, y }, c, a.rotation);
    const int lx = local.x, ly = local.y;
    const float tol = (std::max)(6.f, a.thickness + 4.f);

    switch (a.type)
    {
    case ToolType::Line:
    case ToolType::Arrow:
        if (a.points.size() < 2) return false;
        return DistPointSeg(lx, ly, a.points[0], a.points[1]) <= tol;

    case ToolType::Pencil:
    case ToolType::Highlight:
    {
        const float markTol = (a.type == ToolType::Highlight)
            ? (std::max)(tol, a.thickness * 3.0f + 4.f)
            : tol;
        for (size_t i = 1; i < a.points.size(); ++i)
        {
            if (DistPointSeg(lx, ly, a.points[i - 1], a.points[i]) <= markTol)
                return true;
        }
        if (a.points.size() == 1)
        {
            const int dx = lx - a.points[0].x, dy = ly - a.points[0].y;
            return (dx * dx + dy * dy) <= (int)(markTol * markTol);
        }
        return false;
    }

    case ToolType::Rectangle:
    case ToolType::Text:
    {
        RECT b = GetBounds(a);
        InflateRect(&b, (int)tol / 2, (int)tol / 2);
        return PtInRectI(b, lx, ly);
    }

    case ToolType::Ellipse:
    {
        RECT b = GetBounds(a);
        const float cx = (b.left + b.right) * 0.5f;
        const float cy = (b.top + b.bottom) * 0.5f;
        const float rx = (std::max)(1.f, (b.right - b.left) * 0.5f);
        const float ry = (std::max)(1.f, (b.bottom - b.top) * 0.5f);
        const float nx = (lx - cx) / (rx + tol);
        const float ny = (ly - cy) / (ry + tol);
        return (nx * nx + ny * ny) <= 1.0f;
    }

    default:
        return false;
    }
}

void AnnotationDocument::ClearRedo()
{
    redo_.clear();
}

void AnnotationDocument::Clear()
{
    items_.clear();
    ClearRedo();
}

void AnnotationDocument::Push(Annotation ann)
{
    items_.push_back(std::move(ann));
    ClearRedo();
}

bool AnnotationDocument::Undo()
{
    if (items_.empty())
        return false;
    redo_.push_back(std::move(items_.back()));
    items_.pop_back();
    return true;
}

bool AnnotationDocument::Redo()
{
    if (redo_.empty())
        return false;
    items_.push_back(std::move(redo_.back()));
    redo_.pop_back();
    return true;
}

bool AnnotationDocument::RemoveAt(int index)
{
    if (index < 0 || index >= (int)items_.size())
        return false;
    items_.erase(items_.begin() + index);
    ClearRedo();
    return true;
}

bool AnnotationDocument::UpdateAt(int index, Annotation ann)
{
    if (index < 0 || index >= (int)items_.size())
        return false;
    items_[index] = std::move(ann);
    ClearRedo();
    return true;
}

int AnnotationDocument::BringToFront(int index)
{
    if (index < 0 || index >= (int)items_.size())
        return -1;
    if (index == (int)items_.size() - 1)
        return index;
    Annotation item = std::move(items_[index]);
    items_.erase(items_.begin() + index);
    items_.push_back(std::move(item));
    ClearRedo();
    return (int)items_.size() - 1;
}

int AnnotationDocument::SendToBack(int index)
{
    if (index < 0 || index >= (int)items_.size())
        return -1;
    if (index == 0)
        return index;
    Annotation item = std::move(items_[index]);
    items_.erase(items_.begin() + index);
    items_.insert(items_.begin(), std::move(item));
    ClearRedo();
    return 0;
}

int AnnotationDocument::BringForward(int index)
{
    if (index < 0 || index >= (int)items_.size() - 1)
        return -1;
    std::swap(items_[index], items_[index + 1]);
    ClearRedo();
    return index + 1;
}

int AnnotationDocument::SendBackward(int index)
{
    if (index <= 0 || index >= (int)items_.size())
        return -1;
    std::swap(items_[index], items_[index - 1]);
    ClearRedo();
    return index - 1;
}

Annotation* AnnotationDocument::GetMutable(int index)
{
    if (index < 0 || index >= (int)items_.size()) return nullptr;
    return &items_[index];
}

const Annotation* AnnotationDocument::Get(int index) const
{
    if (index < 0 || index >= (int)items_.size()) return nullptr;
    return &items_[index];
}

int AnnotationDocument::HitTestAny(int x, int y) const
{
    for (int i = (int)items_.size() - 1; i >= 0; --i)
    {
        if (AnnGeom::HitTest(items_[i], x, y))
            return i;
    }
    return -1;
}

int AnnotationDocument::HitTestText(int x, int y) const
{
    for (int i = (int)items_.size() - 1; i >= 0; --i)
    {
        if (items_[i].type == ToolType::Text && AnnGeom::HitTest(items_[i], x, y))
            return i;
    }
    return -1;
}

void AnnotationDocument::DrawOne(Graphics& g, const Annotation& a)
{
    GraphicsState state = g.Save();
    POINT c = AnnGeom::GetCenter(a);
    if (std::fabs(a.rotation) > 0.01f)
    {
        g.TranslateTransform((REAL)c.x, (REAL)c.y);
        g.RotateTransform(a.rotation);
        g.TranslateTransform((REAL)-c.x, (REAL)-c.y);
    }

    Color col(GetRValue(a.color), GetGValue(a.color), GetBValue(a.color));
    Pen pen(col, a.thickness);
    pen.SetLineCap(LineCapRound, LineCapRound, DashCapRound);
    pen.SetLineJoin(LineJoinRound);

    switch (a.type)
    {
    case ToolType::Pencil:
        if (a.points.size() >= 2)
        {
            std::vector<Point> pts;
            pts.reserve(a.points.size());
            for (const auto& p : a.points)
                pts.emplace_back(p.x, p.y);
            g.DrawLines(&pen, pts.data(), (INT)pts.size());
        }
        else if (a.points.size() == 1)
        {
            SolidBrush brush(col);
            g.FillEllipse(&brush, a.points[0].x - 1, a.points[0].y - 1, 3, 3);
        }
        break;

    case ToolType::Highlight:
        if (a.points.size() >= 2)
        {
            Color translucent(100, GetRValue(a.color), GetGValue(a.color), GetBValue(a.color));
            const REAL tw = (std::max)(8.f, a.thickness * 3.5f);
            Pen mark(translucent, tw);
            mark.SetLineCap(LineCapFlat, LineCapFlat, DashCapFlat);
            mark.SetLineJoin(LineJoinRound);
            std::vector<Point> pts;
            pts.reserve(a.points.size());
            for (const auto& p : a.points)
                pts.emplace_back(p.x, p.y);
            g.SetCompositingMode(CompositingModeSourceOver);
            g.DrawLines(&mark, pts.data(), (INT)pts.size());
        }
        else if (a.points.size() == 1)
        {
            Color translucent(100, GetRValue(a.color), GetGValue(a.color), GetBValue(a.color));
            const REAL tw = (std::max)(8.f, a.thickness * 3.5f);
            SolidBrush brush(translucent);
            const INT r = (INT)(tw * 0.5f);
            g.FillEllipse(&brush, a.points[0].x - r, a.points[0].y - r, r * 2, r * 2);
        }
        break;

    case ToolType::Line:
        if (a.points.size() >= 2)
        {
            g.DrawLine(&pen,
                (INT)a.points[0].x, (INT)a.points[0].y,
                (INT)a.points[1].x, (INT)a.points[1].y);
        }
        break;

    case ToolType::Arrow:
        if (a.points.size() >= 2)
        {
            const POINT& p0 = a.points[0];
            const POINT& p1 = a.points[1];
            const float dx = (float)(p1.x - p0.x);
            const float dy = (float)(p1.y - p0.y);
            const float len = std::sqrt(dx * dx + dy * dy);
            if (len < 1.f)
                break;

            const float ux = dx / len;
            const float uy = dy / len;
            const float px = -uy;
            const float py = ux;
            const float headLen = (std::max)(10.f, a.thickness * 4.0f);
            const float headHalf = (std::max)(6.f, a.thickness * 2.5f);
            const float tipInset = (std::min)(headLen * 0.15f, len * 0.1f);

            // Shorten shaft so it meets the filled tip cleanly
            const float shaftEnd = (std::max)(0.f, len - headLen + tipInset);
            Pen shaft(col, a.thickness);
            shaft.SetLineCap(LineCapRound, LineCapRound, DashCapRound);
            g.DrawLine(&shaft,
                (INT)p0.x, (INT)p0.y,
                (INT)std::lround(p0.x + ux * shaftEnd),
                (INT)std::lround(p0.y + uy * shaftEnd));

            Point tip((INT)p1.x, (INT)p1.y);
            Point left(
                (INT)std::lround(p1.x - ux * headLen + px * headHalf),
                (INT)std::lround(p1.y - uy * headLen + py * headHalf));
            Point right(
                (INT)std::lround(p1.x - ux * headLen - px * headHalf),
                (INT)std::lround(p1.y - uy * headLen - py * headHalf));
            Point tri[3] = { tip, left, right };
            SolidBrush brush(col);
            g.FillPolygon(&brush, tri, 3);
        }
        break;

    case ToolType::Rectangle:
    {
        RECT b = AnnGeom::GetBounds(a);
        INT w = b.right - b.left, h = b.bottom - b.top;
        if (w > 0 && h > 0)
            g.DrawRectangle(&pen, b.left, b.top, w, h);
        break;
    }

    case ToolType::Ellipse:
    {
        RECT b = AnnGeom::GetBounds(a);
        INT w = b.right - b.left, h = b.bottom - b.top;
        if (w > 0 && h > 0)
            g.DrawEllipse(&pen, b.left, b.top, w, h);
        break;
    }

    case ToolType::Text:
        if (!a.text.empty())
        {
            INT style = FontStyleRegular;
            if (a.textStyle.bold) style |= FontStyleBold;
            if (a.textStyle.italic) style |= FontStyleItalic;
            if (a.textStyle.underline) style |= FontStyleUnderline;
            FontFamily family(L"Segoe UI");
            Font font(&family, (REAL)a.textStyle.fontSize, style, UnitPixel);
            Color tc(GetRValue(a.textStyle.color), GetGValue(a.textStyle.color), GetBValue(a.textStyle.color));
            SolidBrush brush(tc);
            RECT b = AnnGeom::GetBounds(a);
            RectF layout((REAL)b.left, (REAL)b.top,
                (REAL)(std::max)(1L, b.right - b.left),
                (REAL)(std::max)(1L, b.bottom - b.top));
            StringFormat fmt;
            fmt.SetTrimming(StringTrimmingEllipsisWord);
            g.DrawString(a.text.c_str(), -1, &font, layout, &fmt, &brush);
        }
        break;

    default:
        break;
    }

    g.Restore(state);
}

void AnnotationDocument::DrawSelection(Graphics& g, const Annotation& a, float dpiScale)
{
    if (dpiScale <= 0.f)
        dpiScale = 1.f;

    GraphicsState state = g.Save();
    POINT c = AnnGeom::GetCenter(a);
    RECT b = AnnGeom::GetBounds(a);

    g.TranslateTransform((REAL)c.x, (REAL)c.y);
    g.RotateTransform(a.rotation);
    g.TranslateTransform((REAL)-c.x, (REAL)-c.y);

    const INT pad = (INT)std::lround(2.0 * dpiScale);
    const INT hs = (INT)std::lround(8.0 * dpiScale);
    const INT hh = hs / 2;
    const INT rotOff = (INT)std::lround(28.0 * dpiScale);
    const INT rotR = (INT)std::lround(6.0 * dpiScale);

    if (a.locked)
    {
        Pen border(Color(255, 255, 160, 0), 2.0f * dpiScale);
        g.DrawRectangle(&border,
            (INT)(b.left - pad), (INT)(b.top - pad),
            (INT)((b.right - b.left) + pad * 2), (INT)((b.bottom - b.top) + pad * 2));

        // Simple lock badge at top-left
        const INT lx = b.left - pad;
        const INT ly = b.top - pad - (INT)std::lround(16.0 * dpiScale);
        const INT bw = (INT)std::lround(12.0 * dpiScale);
        const INT bh = (INT)std::lround(10.0 * dpiScale);
        SolidBrush body(Color(255, 255, 160, 0));
        Pen shackle(Color(255, 255, 160, 0), 1.5f * dpiScale);
        g.DrawArc(&shackle, lx + bw / 4, ly, bw / 2, bh, 180.0f, 180.0f);
        g.FillRectangle(&body, lx, ly + bh / 2, bw, bh);
    }
    else
    {
        Pen border(Color(255, 0, 180, 255), 1.5f * dpiScale);
        border.SetDashStyle(DashStyleDash);
        g.DrawRectangle(&border,
            (INT)(b.left - pad), (INT)(b.top - pad),
            (INT)((b.right - b.left) + pad * 2), (INT)((b.bottom - b.top) + pad * 2));

        SolidBrush handle(Color(255, 255, 255));
        Pen handlePen(Color(255, 0, 120, 215), 1.0f * dpiScale);
        auto drawHandle = [&](int x, int y) {
            g.FillRectangle(&handle, (INT)(x - hh), (INT)(y - hh), hs, hs);
            g.DrawRectangle(&handlePen, (INT)(x - hh), (INT)(y - hh), hs, hs);
        };

        const int midX = (b.left + b.right) / 2;
        const int midY = (b.top + b.bottom) / 2;
        drawHandle(b.left, b.top);
        drawHandle(midX, b.top);
        drawHandle(b.right, b.top);
        drawHandle(b.left, midY);
        drawHandle(b.right, midY);
        drawHandle(b.left, b.bottom);
        drawHandle(midX, b.bottom);
        drawHandle(b.right, b.bottom);

        // Rotation handle above top-center
        const int rx = midX;
        const int ry = b.top - rotOff;
        Pen stem(Color(255, 0, 180, 255), 1.5f * dpiScale);
        g.DrawLine(&stem, midX, b.top - pad, rx, ry + rotR);
        SolidBrush rot(Color(255, 0, 180, 255));
        g.FillEllipse(&rot, rx - rotR, ry - rotR, rotR * 2, rotR * 2);
        Pen ring(Color(255, 255, 255), 1.5f * dpiScale);
        g.DrawEllipse(&ring, rx - rotR, ry - rotR, rotR * 2, rotR * 2);
    }

    g.Restore(state);
}

void AnnotationDocument::DrawAll(Graphics& g, int skipIndex) const
{
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
    for (int i = 0; i < (int)items_.size(); ++i)
    {
        if (i == skipIndex) continue;
        DrawOne(g, items_[i]);
    }
}
