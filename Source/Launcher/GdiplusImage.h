#pragma once

#include <Windows.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AppImage AppImage;

BOOL GdiplusImageInitialize(void);
void GdiplusImageShutdown(void);
AppImage* GdiplusImageCreateFromIcon(HICON icon);
void GdiplusImageDestroy(AppImage* image);
UINT GdiplusImageGetWidth(const AppImage* image);
UINT GdiplusImageGetHeight(const AppImage* image);
BOOL GdiplusImageDraw(const AppImage* image, HDC hdc, int left, int top, int width, int height);

#ifdef __cplusplus
}
#endif
