#include <Windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <strsafe.h>

#include "AppShared.h"
#include "GdiplusImage.h"
#include "ServiceControl.h"

static int AppMinInt(int left, int right)
{
    return left < right ? left : right;
}

static int AppMaxInt(int left, int right)
{
    return left > right ? left : right;
}

static const WCHAR* UiText(const WCHAR* text)
{
    return text != NULL ? text : L"";
}

static const WCHAR* UiTextOrFallback(const WCHAR* text, const WCHAR* fallback)
{
    return (text != NULL && text[0] != L'\0') ? text : fallback;
}

static HFONT UiGetPrimaryFont(void)
{
    if (g_app.uiFont != NULL)
    {
        return g_app.uiFont;
    }

    if (g_app.warningFont != NULL)
    {
        return g_app.warningFont;
    }

    return AppGetFallbackUiFont();
}

static HFONT UiGetWarningFont(void)
{
    if (g_app.warningFont != NULL)
    {
        return g_app.warningFont;
    }

    return UiGetPrimaryFont();
}

static void UiShowFailure(HWND hwnd, const WCHAR* action, const WCHAR* detail)
{
    WCHAR text[SERVICE_CONTROL_ERROR_CAPACITY + 64] = { 0 };
    const WCHAR* safeAction = UiTextOrFallback(action, UiTextOrFallback(g_app.text.defaultOperation, L"Operation"));
    const WCHAR* safeDetail = UiTextOrFallback(detail, UiTextOrFallback(g_app.text.unknownError, L"Unknown error"));
    const WCHAR* format = UiTextOrFallback(g_app.text.actionFailedFormat, L"%s\r\n%s");

    StringCchPrintfW(text, ARRAYSIZE(text), format, safeAction, safeDetail);
    MessageBoxW(hwnd, text, APP_WINDOW_TITLE, MB_OK | MB_ICONERROR);
}

static void UiBuildWindowTitle(WCHAR* title, size_t titleCount, BOOL sandboxStarted)
{
    StringCchPrintfW(
        title,
        titleCount,
        L"%s%s",
        APP_WINDOW_TITLE,
        sandboxStarted ? UiText(g_app.text.titleStartedSuffix) : UiText(g_app.text.titleStoppedSuffix));
}

static void UiUpdateTrayIconTip(BOOL sandboxStarted)
{
    NOTIFYICONDATAW data;

    if (!g_app.trayIconVisible || g_app.hwnd == NULL)
    {
        return;
    }

    ZeroMemory(&data, sizeof(data));
    data.cbSize = sizeof(data);
    data.hWnd = g_app.hwnd;
    data.uID = APP_TRAY_ICON_ID;
    data.uFlags = NIF_TIP;
    UiBuildWindowTitle(data.szTip, ARRAYSIZE(data.szTip), sandboxStarted);
    Shell_NotifyIconW(NIM_MODIFY, &data);
}

static void UiUpdateWindowTitle(BOOL sandboxStarted)
{
    WCHAR title[128] = { 0 };

    if (g_app.hwnd == NULL)
    {
        return;
    }

    UiBuildWindowTitle(title, ARRAYSIZE(title), sandboxStarted);
    SetWindowTextW(g_app.hwnd, title);
    UiUpdateTrayIconTip(sandboxStarted);
}

static void UiRemoveTrayIcon(void)
{
    NOTIFYICONDATAW data;

    if (!g_app.trayIconVisible || g_app.hwnd == NULL)
    {
        return;
    }

    ZeroMemory(&data, sizeof(data));
    data.cbSize = sizeof(data);
    data.hWnd = g_app.hwnd;
    data.uID = APP_TRAY_ICON_ID;
    Shell_NotifyIconW(NIM_DELETE, &data);
    g_app.trayIconVisible = FALSE;
}

static void UiAddTrayIcon(void)
{
    NOTIFYICONDATAW data;
    ServiceControlState state;

    if (g_app.trayIconVisible || g_app.hwnd == NULL)
    {
        return;
    }

    state = ServiceControlQueryState();

    ZeroMemory(&data, sizeof(data));
    data.cbSize = sizeof(data);
    data.hWnd = g_app.hwnd;
    data.uID = APP_TRAY_ICON_ID;
    data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    data.uCallbackMessage = APP_TRAY_CALLBACK_MESSAGE;
    data.hIcon = g_app.smallIcon != NULL ? g_app.smallIcon : g_app.largeIcon;
    UiBuildWindowTitle(data.szTip, ARRAYSIZE(data.szTip), state.running || state.startPending);

    if (Shell_NotifyIconW(NIM_ADD, &data))
    {
        g_app.trayIconVisible = TRUE;
    }
}

static void UiRestoreFromTray(void)
{
    UiRemoveTrayIcon();
    ShowWindow(g_app.hwnd, SW_RESTORE);
    SetForegroundWindow(g_app.hwnd);
}

static void UiMinimizeToTray(void)
{
    UiAddTrayIcon();
    ShowWindow(g_app.hwnd, SW_HIDE);
}

static BOOL UiEnsureSingleInstance(void)
{
    HWND existingWindow;

    g_app.singleInstanceMutex = CreateMutexW(NULL, TRUE, APP_SINGLE_INSTANCE_MUTEX_NAME);
    if (g_app.singleInstanceMutex == NULL)
    {
        return TRUE;
    }

    if (GetLastError() != ERROR_ALREADY_EXISTS)
    {
        return TRUE;
    }

    existingWindow = FindWindowW(APP_WINDOW_CLASS_NAME, NULL);
    if (existingWindow != NULL)
    {
        ShowWindow(existingWindow, IsIconic(existingWindow) ? SW_RESTORE : SW_SHOW);
        SetForegroundWindow(existingWindow);
    }

    CloseHandle(g_app.singleInstanceMutex);
    g_app.singleInstanceMutex = NULL;
    return FALSE;
}

static void UiCloseSingleInstanceMutex(void)
{
    if (g_app.singleInstanceMutex != NULL)
    {
        CloseHandle(g_app.singleInstanceMutex);
        g_app.singleInstanceMutex = NULL;
    }
}

static int UiMeasureWarningTextHeight(HWND hwnd, int availableWidth)
{
    HDC hdc;
    HGDIOBJ oldFont;
    RECT rect;
    int safeWidth = AppMaxInt(1, availableWidth);

    hdc = GetDC(hwnd);
    if (hdc == NULL)
    {
        return AppGetWarningHeight(hwnd);
    }

    oldFont = SelectObject(hdc, UiGetWarningFont());
    rect.left = 0;
    rect.top = 0;
    rect.right = safeWidth;
    rect.bottom = 0;
    DrawTextW(
        hdc,
        UiText(g_app.text.warningText),
        -1,
        &rect,
        DT_CALCRECT | DT_CENTER | DT_WORDBREAK | DT_NOPREFIX);
    SelectObject(hdc, oldFont);
    ReleaseDC(hwnd, hdc);
    return AppMaxInt(1, rect.bottom - rect.top);
}

static int UiMeasureProjectTextHeight(HWND hwnd, int availableWidth)
{
    SIZE idealSize;
    int safeWidth = AppMaxInt(1, availableWidth);
    LRESULT result;

    UNREFERENCED_PARAMETER(hwnd);

    if (g_app.projectLink == NULL)
    {
        return AppScaleForDpi(60, AppGetWindowDpi(hwnd));
    }

    ZeroMemory(&idealSize, sizeof(idealSize));
    result = SendMessageW(g_app.projectLink, LM_GETIDEALSIZE, (WPARAM)safeWidth, (LPARAM)&idealSize);
    if (idealSize.cy > 0)
    {
        return idealSize.cy;
    }

    return AppMaxInt(1, (int)result);
}

static int UiGetHeroTextGap(HWND hwnd)
{
    return AppScaleForDpi(4, AppGetWindowDpi(hwnd));
}

static int UiGetHeroImageDrawSize(HWND hwnd)
{
    UINT imageWidth;
    UINT imageHeight;

    if (g_app.image == NULL)
    {
        return 0;
    }

    imageWidth = GdiplusImageGetWidth(g_app.image);
    imageHeight = GdiplusImageGetHeight(g_app.image);
    if (imageWidth == 0 || imageHeight == 0)
    {
        return 0;
    }

    return AppMinInt(
        AppGetDisplayImageSize(hwnd),
        AppMinInt((int)imageWidth, (int)imageHeight));
}

static int UiGetBottomContentHeight(HWND hwnd, int clientWidth)
{
    UINT dpi = AppGetWindowDpi(hwnd);
    int margin = AppScaleForDpi(APP_BASE_MARGIN, dpi);
    int spacing = AppScaleForDpi(APP_BASE_CONTROL_SPACING, dpi);
    int buttonHeight = AppScaleForDpi(APP_BASE_BUTTON_HEIGHT, dpi);
    int availableWidth = AppMaxInt(1, clientWidth - (margin * 2));
    int warningHeight = UiMeasureWarningTextHeight(hwnd, availableWidth);
    int projectHeight = UiMeasureProjectTextHeight(hwnd, availableWidth);

    return UiGetHeroTextGap(hwnd) + warningHeight + spacing + buttonHeight + spacing + projectHeight + margin;
}

static void UiLoadHeroImage(void)
{
    if (g_app.image != NULL)
    {
        GdiplusImageDestroy(g_app.image);
        g_app.image = NULL;
    }

    if (g_app.displayIcon != NULL)
    {
        g_app.image = GdiplusImageCreateFromIcon(g_app.displayIcon);
    }
}

static ServiceControlState UiUpdateToggleButton(void)
{
    ServiceControlState state = ServiceControlQueryState();
    const WCHAR* buttonText = UiText(g_app.text.startSandbox);
    BOOL enableButton = FALSE;
    BOOL busy = state.startPending || state.stopPending || state.deletePending;

    if (!state.queryOk)
    {
        buttonText = UiText(g_app.text.serviceUnavailable);
    }
    else if (state.deletePending)
    {
        buttonText = UiText(g_app.text.deletingService);
    }
    else if (!state.installed)
    {
        buttonText = UiText(g_app.text.serviceMissing);
    }
    else if (state.startPending)
    {
        buttonText = UiText(g_app.text.startingSandbox);
    }
    else if (state.stopPending)
    {
        buttonText = UiText(g_app.text.stoppingSandbox);
    }
    else if (state.running)
    {
        buttonText = UiText(g_app.text.stopSandbox);
        enableButton = TRUE;
    }
    else
    {
        enableButton = TRUE;
    }

    if (g_app.toggleButton != NULL)
    {
        SetWindowTextW(g_app.toggleButton, buttonText);
        EnableWindow(g_app.toggleButton, enableButton && !busy);
    }

    UiUpdateWindowTitle(state.running || state.startPending);
    return state;
}

static void UiLayoutControls(int clientWidth)
{
    UINT dpi;
    int margin;
    int spacing;
    int buttonHeight;
    int availableWidth;
    int warningHeight;
    int minButtonWidth;
    int maxButtonWidth;
    int buttonWidth;
    int buttonLeft;
    int buttonTop;
    int projectTop;
    int projectHeight;

    if (g_app.toggleButton == NULL)
    {
        return;
    }

    dpi = AppGetWindowDpi(g_app.hwnd);
    margin = AppScaleForDpi(APP_BASE_MARGIN, dpi);
    spacing = AppScaleForDpi(APP_BASE_CONTROL_SPACING, dpi);
    buttonHeight = AppScaleForDpi(APP_BASE_BUTTON_HEIGHT, dpi);
    availableWidth = AppMaxInt(1, clientWidth - (margin * 2));
    warningHeight = UiMeasureWarningTextHeight(g_app.hwnd, availableWidth);
    minButtonWidth = AppScaleForDpi(APP_BASE_MIN_BUTTON_WIDTH, dpi);
    maxButtonWidth = AppScaleForDpi(APP_BASE_MAX_BUTTON_WIDTH, dpi);
    buttonWidth = AppMaxInt(minButtonWidth, AppMinInt(availableWidth, maxButtonWidth));
    buttonLeft = AppMaxInt(margin, (clientWidth - buttonWidth) / 2);
    buttonTop = UiGetHeroImageDrawSize(g_app.hwnd) + UiGetHeroTextGap(g_app.hwnd) + warningHeight + spacing;
    projectTop = buttonTop + buttonHeight + spacing;
    projectHeight = UiMeasureProjectTextHeight(g_app.hwnd, availableWidth);

    MoveWindow(g_app.toggleButton, buttonLeft, buttonTop, buttonWidth, buttonHeight, TRUE);
    if (g_app.projectLink != NULL)
    {
        MoveWindow(g_app.projectLink, margin, projectTop, availableWidth, projectHeight, TRUE);
    }
}

static void UiDrawImageArea(HDC hdc)
{
    RECT clientRect;
    RECT warningRect;
    RECT imageBandRect;
    HBRUSH heroBrush;
    HFONT oldFont;
    int drawSize;
    int availableWidth;
    int drawLeft;
    int margin;
    int warningWidth;
    int warningHeight;
    UINT imageWidth;
    UINT imageHeight;

    GetClientRect(g_app.hwnd, &clientRect);
    FillRect(hdc, &clientRect, GetSysColorBrush(COLOR_WINDOW));

    if (g_app.image == NULL)
    {
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
        DrawTextW(hdc, UiText(g_app.text.imageLoadFailure), -1, &clientRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return;
    }

    imageWidth = GdiplusImageGetWidth(g_app.image);
    imageHeight = GdiplusImageGetHeight(g_app.image);
    if (imageWidth == 0 || imageHeight == 0)
    {
        return;
    }

    drawSize = UiGetHeroImageDrawSize(g_app.hwnd);
    availableWidth = AppMaxInt(1, clientRect.right - clientRect.left);
    drawLeft = clientRect.left + ((availableWidth - drawSize) / 2);

    imageBandRect = clientRect;
    imageBandRect.bottom = imageBandRect.top + drawSize;
    heroBrush = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(hdc, &imageBandRect, heroBrush);
    DeleteObject(heroBrush);

    GdiplusImageDraw(g_app.image, hdc, drawLeft, clientRect.top, drawSize, drawSize);

    margin = AppScaleForDpi(APP_BASE_MARGIN, AppGetWindowDpi(g_app.hwnd));
    warningWidth = AppMaxInt(1, clientRect.right - clientRect.left - (margin * 2));
    warningHeight = UiMeasureWarningTextHeight(g_app.hwnd, warningWidth);
    warningRect.left = clientRect.left + margin;
    warningRect.top = clientRect.top + drawSize + UiGetHeroTextGap(g_app.hwnd);
    warningRect.right = clientRect.right - margin;
    warningRect.bottom = warningRect.top + warningHeight;

    oldFont = (HFONT)SelectObject(hdc, UiGetWarningFont());
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(180, 42, 42));
    DrawTextW(hdc, UiText(g_app.text.warningText), -1, &warningRect, DT_CENTER | DT_WORDBREAK | DT_NOPREFIX);
    SelectObject(hdc, oldFont);
}

static void UiHandleProjectLinkActivate(LPARAM lParam)
{
    const NMLINK* link = (const NMLINK*)lParam;

    if (link == NULL || link->item.szUrl[0] == L'\0')
    {
        return;
    }

    ShellExecuteW(NULL, L"open", link->item.szUrl, NULL, NULL, SW_SHOWNORMAL);
}

static void UiSizeWindowToContent(HWND hwnd)
{
    RECT workArea;
    RECT windowRect;
    UINT dpi;
    int workWidth;
    int workHeight;
    int screenPadding;
    int clientWidth = APP_BASE_DEFAULT_CLIENT_WIDTH;
    int clientHeight = APP_BASE_DEFAULT_CLIENT_HEIGHT;
    int windowWidth;
    int windowHeight;
    int left;
    int top;

    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);

    dpi = AppGetWindowDpi(hwnd);
    workWidth = workArea.right - workArea.left;
    workHeight = workArea.bottom - workArea.top;
    screenPadding = AppScaleForDpi(80, dpi);

    if (g_app.image != NULL &&
        GdiplusImageGetWidth(g_app.image) > 0 &&
        GdiplusImageGetHeight(g_app.image) > 0)
    {
        int heroHorizontalPadding = AppScaleForDpi(32, dpi);
        int requestedImageSize = UiGetHeroImageDrawSize(hwnd);
        int maxWidth;
        int maxHeight;

        clientWidth = AppMaxInt(AppGetMinWindowWidth(hwnd), requestedImageSize + (heroHorizontalPadding * 2));
        clientHeight = requestedImageSize + UiGetBottomContentHeight(hwnd, clientWidth);

        maxWidth = AppMaxInt(AppGetMinWindowWidth(hwnd), workWidth - screenPadding);
        maxHeight = AppMaxInt(AppGetMinWindowHeight(hwnd), workHeight - screenPadding);
        clientWidth = AppMinInt(clientWidth, maxWidth);
        clientHeight = AppMinInt(clientHeight, maxHeight);
    }
    else
    {
        int minWindowWidth = AppGetMinWindowWidth(hwnd);
        int minWindowHeight = AppGetMinWindowHeight(hwnd);
        int maxWidth = AppMaxInt(minWindowWidth, workWidth - screenPadding);
        int maxHeight = AppMaxInt(minWindowHeight, workHeight - screenPadding);
        double widthScale = (double)maxWidth / (double)AppMaxInt(1, clientWidth);
        double heightScale = (double)maxHeight / (double)AppMaxInt(1, clientHeight);
        double scale = widthScale < heightScale ? widthScale : heightScale;

        if (scale > 1.0)
        {
            scale = 1.0;
        }

        clientWidth = AppMaxInt(minWindowWidth, (int)(clientWidth * scale));
        clientHeight = AppMaxInt(minWindowHeight, (int)(clientHeight * scale));
    }

    windowRect.left = 0;
    windowRect.top = 0;
    windowRect.right = clientWidth;
    windowRect.bottom = clientHeight;
    AdjustWindowRectExForDpi(&windowRect, APP_WINDOW_STYLE, FALSE, 0, dpi);

    windowWidth = windowRect.right - windowRect.left;
    windowHeight = windowRect.bottom - windowRect.top;
    left = workArea.left + ((workArea.right - workArea.left - windowWidth) / 2);
    top = workArea.top + ((workArea.bottom - workArea.top - windowHeight) / 2);
    SetWindowPos(hwnd, NULL, left, top, windowWidth, windowHeight, SWP_NOZORDER | SWP_NOACTIVATE);
}

static void UiHandleToggleButton(HWND hwnd)
{
    ServiceControlState state = UiUpdateToggleButton();
    WCHAR errorMessage[SERVICE_CONTROL_ERROR_CAPACITY] = { 0 };

    if (state.running || state.startPending)
    {
        if (ServiceControlStop(errorMessage, ARRAYSIZE(errorMessage)) != ERROR_SUCCESS)
        {
            UiShowFailure(hwnd, g_app.text.actionStop, errorMessage);
        }
    }
    else
    {
        if (ServiceControlStart(errorMessage, ARRAYSIZE(errorMessage)) != ERROR_SUCCESS)
        {
            UiShowFailure(hwnd, g_app.text.actionStart, errorMessage);
        }
    }

    UiUpdateToggleButton();
    InvalidateRect(hwnd, NULL, FALSE);
}

static LRESULT CALLBACK UiWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        case WM_CREATE:
        {
            CREATESTRUCTW* create = (CREATESTRUCTW*)lParam;
            WCHAR errorMessage[SERVICE_CONTROL_ERROR_CAPACITY] = { 0 };

            g_app.hwnd = hwnd;
            AppCreateUiFonts();

            g_app.toggleButton = CreateWindowExW(
                0,
                WC_BUTTONW,
                UiText(g_app.text.startSandbox),
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                0,
                0,
                0,
                0,
                hwnd,
                (HMENU)(INT_PTR)APP_ID_TOGGLE_DRIVER,
                create->hInstance,
                NULL);
            SendMessageW(g_app.toggleButton, WM_SETFONT, (WPARAM)UiGetPrimaryFont(), FALSE);

            g_app.projectLink = CreateWindowExW(
                0,
                WC_LINK,
                UiText(g_app.text.projectText),
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | LWS_TRANSPARENT,
                0,
                0,
                0,
                0,
                hwnd,
                (HMENU)(INT_PTR)APP_ID_PROJECT_LINK,
                create->hInstance,
                NULL);
            if (g_app.projectLink != NULL)
            {
                SendMessageW(g_app.projectLink, WM_SETFONT, (WPARAM)UiGetPrimaryFont(), FALSE);
            }

            UiLoadHeroImage();

            if (ServiceControlInitialize(errorMessage, ARRAYSIZE(errorMessage)) != ERROR_SUCCESS)
            {
                UiShowFailure(hwnd, g_app.text.actionPrivilegeSetup, errorMessage);
            }

            if (ServiceControlEnsureInstalledOnStartup(&g_app.installedByLauncher, errorMessage, ARRAYSIZE(errorMessage)) != ERROR_SUCCESS)
            {
                UiShowFailure(hwnd, g_app.text.actionStartupInstall, errorMessage);
            }

            UiUpdateToggleButton();
            return 0;
        }

        case WM_SIZE:
            if (wParam == SIZE_MINIMIZED)
            {
                UiMinimizeToTray();
                return 0;
            }

            UiLayoutControls(LOWORD(lParam));
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;

        case WM_COMMAND:
            if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == APP_ID_TOGGLE_DRIVER)
            {
                UiHandleToggleButton(hwnd);
            }
            return 0;

        case WM_NOTIFY:
        {
            const NMHDR* notify = (const NMHDR*)lParam;

            if (notify != NULL &&
                notify->idFrom == APP_ID_PROJECT_LINK &&
                (notify->code == NM_CLICK || notify->code == NM_RETURN))
            {
                UiHandleProjectLinkActivate(lParam);
                return 0;
            }
            break;
        }

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;

        case APP_TRAY_CALLBACK_MESSAGE:
            if (lParam == WM_LBUTTONDBLCLK || lParam == WM_LBUTTONUP)
            {
                UiRestoreFromTray();
                return 0;
            }
            break;

        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT:
        {
            PAINTSTRUCT paint;
            HDC hdc = BeginPaint(hwnd, &paint);
            UiDrawImageArea(hdc);
            EndPaint(hwnd, &paint);
            return 0;
        }

        case WM_DESTROY:
        {
            WCHAR errorMessage[SERVICE_CONTROL_ERROR_CAPACITY] = { 0 };

            UiRemoveTrayIcon();
            if (ServiceControlCleanupOnExit(g_app.installedByLauncher, errorMessage, ARRAYSIZE(errorMessage)) != ERROR_SUCCESS)
            {
                UiShowFailure(hwnd, g_app.text.actionExitCleanup, errorMessage);
            }

            g_app.hwnd = NULL;
            g_app.toggleButton = NULL;
            g_app.projectLink = NULL;
            PostQuitMessage(0);
            return 0;
        }
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

int AppRunUi(HINSTANCE instance, int commandShow)
{
    INITCOMMONCONTROLSEX controls;
    WNDCLASSEXW windowClass;
    HWND hwnd;
    MSG message;

    ZeroMemory(&controls, sizeof(controls));
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_STANDARD_CLASSES | ICC_LINK_CLASS;
    InitCommonControlsEx(&controls);

    if (!UiEnsureSingleInstance())
    {
        return 0;
    }

    if (!GdiplusImageInitialize())
    {
        UiCloseSingleInstanceMutex();
        MessageBoxW(NULL, UiText(g_app.text.gdiplusInitFailed), APP_WINDOW_TITLE, MB_OK | MB_ICONERROR);
        return 1;
    }

    AppLoadAppIcons(instance);

    ZeroMemory(&windowClass, sizeof(windowClass));
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = UiWindowProc;
    windowClass.hInstance = instance;
    windowClass.hIcon = g_app.largeIcon;
    windowClass.hIconSm = g_app.smallIcon;
    windowClass.hCursor = LoadCursorW(NULL, IDC_ARROW);
    windowClass.hbrBackground = NULL;
    windowClass.lpszClassName = APP_WINDOW_CLASS_NAME;

    if (RegisterClassExW(&windowClass) == 0)
    {
        AppDestroyAppIcons();
        GdiplusImageShutdown();
        UiCloseSingleInstanceMutex();
        MessageBoxW(NULL, UiText(g_app.text.windowRegistrationFailed), APP_WINDOW_TITLE, MB_OK | MB_ICONERROR);
        return 1;
    }

    hwnd = CreateWindowExW(
        0,
        APP_WINDOW_CLASS_NAME,
        APP_WINDOW_TITLE,
        APP_WINDOW_STYLE,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        APP_BASE_DEFAULT_CLIENT_WIDTH,
        APP_BASE_DEFAULT_CLIENT_HEIGHT,
        NULL,
        NULL,
        instance,
        NULL);
    if (hwnd == NULL)
    {
        UnregisterClassW(APP_WINDOW_CLASS_NAME, instance);
        AppDestroyAppIcons();
        GdiplusImageShutdown();
        UiCloseSingleInstanceMutex();
        MessageBoxW(NULL, UiText(g_app.text.windowCreationFailed), APP_WINDOW_TITLE, MB_OK | MB_ICONERROR);
        return 1;
    }

    if (g_app.largeIcon != NULL)
    {
        SendMessageW(hwnd, WM_SETICON, ICON_BIG, (LPARAM)g_app.largeIcon);
    }

    if (g_app.smallIcon != NULL)
    {
        SendMessageW(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)g_app.smallIcon);
    }

    UiSizeWindowToContent(hwnd);
    ShowWindow(hwnd, commandShow);
    UpdateWindow(hwnd);

    ZeroMemory(&message, sizeof(message));
    while (GetMessageW(&message, NULL, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    if (g_app.image != NULL)
    {
        GdiplusImageDestroy(g_app.image);
        g_app.image = NULL;
    }

    AppDestroyUiFonts();
    AppDestroyAppIcons();
    GdiplusImageShutdown();
    UiCloseSingleInstanceMutex();
    UnregisterClassW(APP_WINDOW_CLASS_NAME, instance);
    return (int)message.wParam;
}
