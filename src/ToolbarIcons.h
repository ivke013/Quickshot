// Author: Ivan Stojmenovic
#pragma once
#include "framework.h"

enum class ToolIconId
{
    Pencil = 0,
    Highlight,
    Line,
    Arrow,
    Rectangle,
    Ellipse,
    Text,
    Color,
    Undo,
    Copy,
    Save,
    Upload,
    Share,
    Close,
    Bold,
    Italic,
    Underline,
    FontInc,
    FontDec,
    Count
};

class ToolbarIcons
{
public:
    bool Load(HINSTANCE hInst, int dpi = 96);
    void Unload();
    Gdiplus::Bitmap* Get(ToolIconId id) const;
    void DrawButton(Gdiplus::Graphics& g, const RECT& rc, ToolIconId id, bool active,
        Gdiplus::Color fill = Gdiplus::Color(230, 35, 35, 35)) const;

private:
    Gdiplus::Bitmap* LoadPngResource(HINSTANCE hInst, int resId);
    Gdiplus::Bitmap* icons_[static_cast<size_t>(ToolIconId::Count)]{};
    bool loaded_ = false;
};
