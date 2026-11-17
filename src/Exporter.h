// Author: Ivan Stojmenovic
#pragma once
#include "framework.h"
#include "Annotation.h"

class Exporter
{
public:
    static bool CopyBitmapToClipboard(HWND hwnd, HBITMAP bitmap);
    static bool CopyTextToClipboard(HWND hwnd, const wchar_t* text);
    static bool SaveBitmapToFile(HWND hwnd, HBITMAP bitmap, const wchar_t* suggestedFolder);
    static bool SaveBitmapAuto(HBITMAP bitmap, const std::wstring& folder, std::wstring* outPath = nullptr);
    static bool PrintBitmap(HWND hwnd, HBITMAP bitmap);
    static HBITMAP ComposeRegion(HBITMAP screenBmp, int screenX, int screenY,
        const RECT& selection, const AnnotationDocument& doc);
    static CLSID EncoderClsid(const wchar_t* mime);
};
