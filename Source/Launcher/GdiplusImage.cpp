#define NOMINMAX
#include "GdiplusImage.h"

#include <gdiplus.h>
#include <malloc.h>

struct AppImage
{
    Gdiplus::Bitmap* bitmap;
};

static ULONG_PTR g_gdiplusToken = 0;
static BOOL g_gdiplusInitialized = FALSE;

extern "C" BOOL GdiplusImageInitialize(void)
{
    Gdiplus::GdiplusStartupInput startupInput = {};

    if (g_gdiplusInitialized)
    {
        return TRUE;
    }

    startupInput.GdiplusVersion = 1;
    if (Gdiplus::GdiplusStartup(&g_gdiplusToken, &startupInput, nullptr) != Gdiplus::Ok)
    {
        return FALSE;
    }

    g_gdiplusInitialized = TRUE;
    return TRUE;
}

extern "C" void GdiplusImageShutdown(void)
{
    if (!g_gdiplusInitialized)
    {
        return;
    }

    Gdiplus::GdiplusShutdown(g_gdiplusToken);
    g_gdiplusToken = 0;
    g_gdiplusInitialized = FALSE;
}

extern "C" AppImage* GdiplusImageCreateFromIcon(HICON icon)
{
    AppImage* image;
    Gdiplus::Bitmap* bitmap;

    if (!g_gdiplusInitialized || icon == nullptr)
    {
        return nullptr;
    }

    bitmap = Gdiplus::Bitmap::FromHICON(icon);
    if (bitmap == nullptr)
    {
        return nullptr;
    }

    if (bitmap->GetLastStatus() != Gdiplus::Ok || bitmap->GetWidth() == 0 || bitmap->GetHeight() == 0)
    {
        delete bitmap;
        return nullptr;
    }

    image = (AppImage*)malloc(sizeof(*image));
    if (image == nullptr)
    {
        delete bitmap;
        return nullptr;
    }

    image->bitmap = bitmap;
    return image;
}

extern "C" void GdiplusImageDestroy(AppImage* image)
{
    if (image == nullptr)
    {
        return;
    }

    delete image->bitmap;
    free(image);
}

extern "C" UINT GdiplusImageGetWidth(const AppImage* image)
{
    return image != nullptr && image->bitmap != nullptr ? image->bitmap->GetWidth() : 0;
}

extern "C" UINT GdiplusImageGetHeight(const AppImage* image)
{
    return image != nullptr && image->bitmap != nullptr ? image->bitmap->GetHeight() : 0;
}

extern "C" BOOL GdiplusImageDraw(const AppImage* image, HDC hdc, int left, int top, int width, int height)
{
    Gdiplus::Graphics graphics(hdc);

    if (image == nullptr || image->bitmap == nullptr || hdc == nullptr || width <= 0 || height <= 0)
    {
        return FALSE;
    }

    graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    return graphics.DrawImage(image->bitmap, left, top, width, height) == Gdiplus::Ok;
}
