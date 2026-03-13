#include "AppShared.h"

#include "Resource.h"

AppState g_app;

static void AppFreeString(WCHAR** text)
{
    if (text != NULL && *text != NULL)
    {
        LocalFree(*text);
        *text = NULL;
    }
}

static WCHAR* AppDuplicateResourceString(HINSTANCE instance, UINT id)
{
    LPCWSTR resourceText = NULL;
    int length;
    WCHAR* copy;

    length = LoadStringW(instance, id, (LPWSTR)&resourceText, 0);
    if (length <= 0)
    {
        copy = (WCHAR*)LocalAlloc(LMEM_FIXED, sizeof(WCHAR));
        if (copy != NULL)
        {
            copy[0] = L'\0';
        }
        return copy;
    }

    copy = (WCHAR*)LocalAlloc(LMEM_FIXED, ((size_t)length + 1) * sizeof(WCHAR));
    if (copy == NULL)
    {
        return NULL;
    }

    CopyMemory(copy, resourceText, (size_t)length * sizeof(WCHAR));
    copy[length] = L'\0';
    return copy;
}

static void AppLoadLocalizedString(WCHAR** target, HINSTANCE instance, UINT id)
{
    AppFreeString(target);
    *target = AppDuplicateResourceString(instance, id);
}

UINT AppGetWindowDpi(HWND hwnd)
{
    UINT dpi = hwnd != NULL ? GetDpiForWindow(hwnd) : USER_DEFAULT_SCREEN_DPI;
    return dpi != 0 ? dpi : USER_DEFAULT_SCREEN_DPI;
}

int AppScaleForDpi(int value, UINT dpi)
{
    return MulDiv(value, (int)dpi, USER_DEFAULT_SCREEN_DPI);
}

int AppGetMargin(HWND hwnd)
{
    return AppScaleForDpi(APP_BASE_MARGIN, AppGetWindowDpi(hwnd));
}

int AppGetControlSpacing(HWND hwnd)
{
    return AppScaleForDpi(APP_BASE_CONTROL_SPACING, AppGetWindowDpi(hwnd));
}

int AppGetWarningHeight(HWND hwnd)
{
    return AppScaleForDpi(APP_BASE_WARNING_HEIGHT, AppGetWindowDpi(hwnd));
}

int AppGetButtonHeight(HWND hwnd)
{
    return AppScaleForDpi(APP_BASE_BUTTON_HEIGHT, AppGetWindowDpi(hwnd));
}

int AppGetDisplayImageSize(HWND hwnd)
{
    return AppScaleForDpi(APP_BASE_DISPLAY_IMAGE_SIZE, AppGetWindowDpi(hwnd));
}

int AppGetMinWindowWidth(HWND hwnd)
{
    return AppScaleForDpi(APP_BASE_MIN_WINDOW_WIDTH, AppGetWindowDpi(hwnd));
}

int AppGetMinWindowHeight(HWND hwnd)
{
    return AppScaleForDpi(APP_BASE_MIN_WINDOW_HEIGHT, AppGetWindowDpi(hwnd));
}

void AppFreeLocalizedStrings(void)
{
    AppFreeString(&g_app.text.startSandbox);
    AppFreeString(&g_app.text.stopSandbox);
    AppFreeString(&g_app.text.startingSandbox);
    AppFreeString(&g_app.text.stoppingSandbox);
    AppFreeString(&g_app.text.deletingService);
    AppFreeString(&g_app.text.serviceMissing);
    AppFreeString(&g_app.text.serviceUnavailable);
    AppFreeString(&g_app.text.warningText);
    AppFreeString(&g_app.text.projectText);
    AppFreeString(&g_app.text.imageLoadFailure);
    AppFreeString(&g_app.text.actionStart);
    AppFreeString(&g_app.text.actionStop);
    AppFreeString(&g_app.text.actionPrivilegeSetup);
    AppFreeString(&g_app.text.actionStartupInstall);
    AppFreeString(&g_app.text.actionExitCleanup);
    AppFreeString(&g_app.text.titleStartedSuffix);
    AppFreeString(&g_app.text.titleStoppedSuffix);
    AppFreeString(&g_app.text.gdiplusInitFailed);
    AppFreeString(&g_app.text.windowRegistrationFailed);
    AppFreeString(&g_app.text.windowCreationFailed);
    AppFreeString(&g_app.text.actionFailedFormat);
    AppFreeString(&g_app.text.defaultOperation);
    AppFreeString(&g_app.text.unknownError);
}

void AppLoadLocalizedStrings(HINSTANCE instance)
{
    AppLoadLocalizedString(&g_app.text.startSandbox, instance, IDS_START_SANDBOX);
    AppLoadLocalizedString(&g_app.text.stopSandbox, instance, IDS_STOP_SANDBOX);
    AppLoadLocalizedString(&g_app.text.startingSandbox, instance, IDS_STARTING_SANDBOX);
    AppLoadLocalizedString(&g_app.text.stoppingSandbox, instance, IDS_STOPPING_SANDBOX);
    AppLoadLocalizedString(&g_app.text.deletingService, instance, IDS_DELETING_SERVICE);
    AppLoadLocalizedString(&g_app.text.serviceMissing, instance, IDS_SERVICE_MISSING);
    AppLoadLocalizedString(&g_app.text.serviceUnavailable, instance, IDS_SERVICE_UNAVAILABLE);
    AppLoadLocalizedString(&g_app.text.warningText, instance, IDS_WARNING_TEXT);
    AppLoadLocalizedString(&g_app.text.projectText, instance, IDS_PROJECT_TEXT);
    AppLoadLocalizedString(&g_app.text.imageLoadFailure, instance, IDS_IMAGE_LOAD_FAILURE);
    AppLoadLocalizedString(&g_app.text.actionStart, instance, IDS_ACTION_START);
    AppLoadLocalizedString(&g_app.text.actionStop, instance, IDS_ACTION_STOP);
    AppLoadLocalizedString(&g_app.text.actionPrivilegeSetup, instance, IDS_ACTION_PRIVILEGE_SETUP);
    AppLoadLocalizedString(&g_app.text.actionStartupInstall, instance, IDS_ACTION_STARTUP_INSTALL);
    AppLoadLocalizedString(&g_app.text.actionExitCleanup, instance, IDS_ACTION_EXIT_CLEANUP);
    AppLoadLocalizedString(&g_app.text.titleStartedSuffix, instance, IDS_TITLE_STARTED_SUFFIX);
    AppLoadLocalizedString(&g_app.text.titleStoppedSuffix, instance, IDS_TITLE_STOPPED_SUFFIX);
    AppLoadLocalizedString(&g_app.text.gdiplusInitFailed, instance, IDS_GDIPLUS_INIT_FAILED);
    AppLoadLocalizedString(&g_app.text.windowRegistrationFailed, instance, IDS_WINDOW_REG_FAILED);
    AppLoadLocalizedString(&g_app.text.windowCreationFailed, instance, IDS_WINDOW_CREATE_FAILED);
    AppLoadLocalizedString(&g_app.text.actionFailedFormat, instance, IDS_ACTION_FAILED_FORMAT);
    AppLoadLocalizedString(&g_app.text.defaultOperation, instance, IDS_DEFAULT_OPERATION);
    AppLoadLocalizedString(&g_app.text.unknownError, instance, IDS_UNKNOWN_ERROR);
}

HFONT AppGetFallbackUiFont(void)
{
    return (HFONT)GetStockObject(DEFAULT_GUI_FONT);
}

void AppDestroyAppIcons(void)
{
    if (g_app.largeIcon != NULL)
    {
        DestroyIcon(g_app.largeIcon);
        g_app.largeIcon = NULL;
    }

    if (g_app.smallIcon != NULL)
    {
        DestroyIcon(g_app.smallIcon);
        g_app.smallIcon = NULL;
    }

    if (g_app.displayIcon != NULL)
    {
        DestroyIcon(g_app.displayIcon);
        g_app.displayIcon = NULL;
    }
}

void AppLoadAppIcons(HINSTANCE instance)
{
    AppDestroyAppIcons();

    g_app.largeIcon = (HICON)LoadImageW(
        instance,
        MAKEINTRESOURCEW(IDI_MAIN_ICON),
        IMAGE_ICON,
        GetSystemMetrics(SM_CXICON),
        GetSystemMetrics(SM_CYICON),
        LR_DEFAULTCOLOR);

    g_app.smallIcon = (HICON)LoadImageW(
        instance,
        MAKEINTRESOURCEW(IDI_MAIN_ICON),
        IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON),
        GetSystemMetrics(SM_CYSMICON),
        LR_DEFAULTCOLOR);

    g_app.displayIcon = (HICON)LoadImageW(
        instance,
        MAKEINTRESOURCEW(IDI_MAIN_ICON),
        IMAGE_ICON,
        APP_BASE_DISPLAY_ICON_SOURCE_SIZE,
        APP_BASE_DISPLAY_ICON_SOURCE_SIZE,
        LR_DEFAULTCOLOR);
}

void AppDestroyUiFonts(void)
{
    if (g_app.uiFont != NULL)
    {
        DeleteObject(g_app.uiFont);
        g_app.uiFont = NULL;
    }

    if (g_app.warningFont != NULL)
    {
        DeleteObject(g_app.warningFont);
        g_app.warningFont = NULL;
    }
}

void AppCreateUiFonts(void)
{
    NONCLIENTMETRICSW metrics;
    LOGFONTW boldFont;

    AppDestroyUiFonts();

    ZeroMemory(&metrics, sizeof(metrics));
    metrics.cbSize = sizeof(metrics);
    if (!SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0))
    {
        return;
    }

    g_app.uiFont = CreateFontIndirectW(&metrics.lfMessageFont);

    boldFont = metrics.lfMessageFont;
    boldFont.lfWeight = FW_BOLD;
    boldFont.lfHeight = MulDiv(metrics.lfMessageFont.lfHeight, 6, 5);
    g_app.warningFont = CreateFontIndirectW(&boldFont);
}
