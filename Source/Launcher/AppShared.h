#pragma once

#include <Windows.h>

#ifdef __cplusplus
extern "C" {
#endif

#define APP_WINDOW_CLASS_NAME L"ClawSandboxLauncherWindow"
#define APP_WINDOW_TITLE L"ClawSandbox"
#define APP_SINGLE_INSTANCE_MUTEX_NAME L"Local\\ClawSandboxLauncherInstance"

enum
{
    APP_TRAY_ICON_ID = 1,
    APP_TRAY_CALLBACK_MESSAGE = WM_APP + 1,
    APP_WINDOW_STYLE = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
    APP_BASE_DEFAULT_CLIENT_WIDTH = 520,
    APP_BASE_DEFAULT_CLIENT_HEIGHT = 500,
    APP_BASE_BUTTON_HEIGHT = 42,
    APP_BASE_MARGIN = 12,
    APP_BASE_CONTROL_SPACING = 10,
    APP_BASE_WARNING_HEIGHT = 52,
    APP_BASE_MIN_WINDOW_WIDTH = 520,
    APP_BASE_MIN_WINDOW_HEIGHT = 360,
    APP_BASE_MIN_BUTTON_WIDTH = 190,
    APP_BASE_MAX_BUTTON_WIDTH = 280,
    APP_BASE_DISPLAY_ICON_SOURCE_SIZE = 256,
    APP_BASE_DISPLAY_IMAGE_SIZE = 256,
    APP_ID_TOGGLE_DRIVER = 1001,
    APP_ID_PROJECT_LINK = 1002
};

typedef struct AppImage AppImage;

typedef struct LocalizedStrings
{
    WCHAR* startSandbox;
    WCHAR* stopSandbox;
    WCHAR* startingSandbox;
    WCHAR* stoppingSandbox;
    WCHAR* deletingService;
    WCHAR* serviceMissing;
    WCHAR* serviceUnavailable;
    WCHAR* warningText;
    WCHAR* projectText;
    WCHAR* imageLoadFailure;
    WCHAR* actionStart;
    WCHAR* actionStop;
    WCHAR* actionPrivilegeSetup;
    WCHAR* actionStartupInstall;
    WCHAR* actionExitCleanup;
    WCHAR* titleStartedSuffix;
    WCHAR* titleStoppedSuffix;
    WCHAR* gdiplusInitFailed;
    WCHAR* windowRegistrationFailed;
    WCHAR* windowCreationFailed;
    WCHAR* actionFailedFormat;
    WCHAR* defaultOperation;
    WCHAR* unknownError;
} LocalizedStrings;

typedef struct AppState
{
    HWND hwnd;
    HWND toggleButton;
    HWND projectLink;
    HANDLE singleInstanceMutex;
    BOOL trayIconVisible;
    AppImage* image;
    HICON largeIcon;
    HICON smallIcon;
    HICON displayIcon;
    HFONT uiFont;
    HFONT warningFont;
    LocalizedStrings text;
    BOOL installedByLauncher;
} AppState;

extern AppState g_app;

UINT AppGetWindowDpi(HWND hwnd);
int AppScaleForDpi(int value, UINT dpi);
int AppGetMargin(HWND hwnd);
int AppGetControlSpacing(HWND hwnd);
int AppGetWarningHeight(HWND hwnd);
int AppGetButtonHeight(HWND hwnd);
int AppGetDisplayImageSize(HWND hwnd);
int AppGetMinWindowWidth(HWND hwnd);
int AppGetMinWindowHeight(HWND hwnd);
void AppLoadLocalizedStrings(HINSTANCE instance);
void AppFreeLocalizedStrings(void);
HFONT AppGetFallbackUiFont(void);
void AppDestroyAppIcons(void);
void AppLoadAppIcons(HINSTANCE instance);
void AppDestroyUiFonts(void);
void AppCreateUiFonts(void);
BOOL AppTryHandleCommandLine(int* exitCode);
int AppRunUi(HINSTANCE instance, int commandShow);

#ifdef __cplusplus
}
#endif
