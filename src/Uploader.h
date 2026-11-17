// Author: Ivan Stojmenovic
#pragma once
#include "framework.h"

class Uploader
{
public:
    // Uploads bitmap using Settings upload URL. On success fills outUrl.
    static bool UploadBitmap(HBITMAP bitmap, std::wstring* outUrl, std::wstring* outError = nullptr);
};
