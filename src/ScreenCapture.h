// Author: Ivan Stojmenovic
#pragma once
#include "framework.h"

struct CapturedScreen
{
    HBITMAP bitmap = nullptr;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    void Release()
    {
        if (bitmap)
        {
            DeleteObject(bitmap);
            bitmap = nullptr;
        }
    }
};

class ScreenCapture
{
public:
    static CapturedScreen CaptureVirtualScreen(bool includeCursor);
    static HBITMAP CloneBitmap(HBITMAP source);
    static HBITMAP CropBitmap(HBITMAP source, int srcX, int srcY, int width, int height, int screenX, int screenY);
};
