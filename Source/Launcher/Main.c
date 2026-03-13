#include <Windows.h>

#include "AppShared.h"

int
WINAPI
wWinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nShowCmd)
{
    int exitCode = ERROR_SUCCESS;

    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    if (AppTryHandleCommandLine(&exitCode))
    {
        return exitCode;
    }

    AppLoadLocalizedStrings(hInstance);
    exitCode = AppRunUi(hInstance, nShowCmd);
    AppFreeLocalizedStrings();
    return exitCode;
}
