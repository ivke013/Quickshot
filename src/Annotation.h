// Author: Ivan Stojmenovic
#pragma once
#include "framework.h"

enum class ToolType
{
    None,
    Pencil,
    Highlight,
    Line,
    Arrow,
    Rectangle,
    Ellipse,
    Text
};

struct TextStyle
{
    bool bold = false;
    bool italic = false;
    bool underline = false;
    int fontSize = 16;
    COLORREF color = RGB(255, 0, 0);
};

struct Annotation
{
    ToolType type = ToolType::None;
    COLORREF color = RGB(255, 0, 0);
    float thickness = 2.5f;
    std::vector<POINT> points;
    RECT bounds{};
    std::wstring text;
    TextStyle textStyle;
    float rotation = 0.f; // degrees, around geometric center
    bool locked = false;
};

namespace AnnGeom
{
    RECT Norm(RECT r);
    RECT GetBounds(const Annotation& a);          // local AABB before rotation
    POINT GetCenter(const Annotation& a);
    RECT GetWorldAabb(const Annotation& a);       // AABB after rotation
    void Move(Annotation& a, int dx, int dy);
    void SetBoundsFromRect(Annotation& a, RECT r); // for rect/ellipse/text resize
    void ScaleAboutCenter(Annotation& a, float sx, float sy);
    bool HitTest(const Annotation& a, int x, int y);
    POINT RotatePoint(POINT p, POINT center, float deg);
    POINT UnrotatePoint(POINT p, POINT center, float deg);
}

class AnnotationDocument
{
public:
    void Clear();
    void Push(Annotation ann);
    bool Undo();
    bool Redo();
    bool CanRedo() const { return !redo_.empty(); }
    bool RemoveAt(int index);
    bool UpdateAt(int index, Annotation ann);
    // Z-order: returns new index, or -1 if unchanged/invalid.
    int BringToFront(int index);
    int SendToBack(int index);
    int BringForward(int index);
    int SendBackward(int index);
    int HitTestAny(int x, int y) const; // topmost, or -1
    int HitTestText(int x, int y) const;
    Annotation* GetMutable(int index);
    const Annotation* Get(int index) const;
    int Count() const { return (int)items_.size(); }
    const std::vector<Annotation>& Items() const { return items_; }

    void DrawAll(Gdiplus::Graphics& g, int skipIndex = -1) const;
    static void DrawOne(Gdiplus::Graphics& g, const Annotation& a);
    static void DrawSelection(Gdiplus::Graphics& g, const Annotation& a, float dpiScale = 1.0f);

private:
    void ClearRedo();

    std::vector<Annotation> items_;
    std::vector<Annotation> redo_;
};
