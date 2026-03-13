#include "Driver.h"

static
BOOLEAN
PathComponentEqualsAtInsensitive(
    _In_ PCUNICODE_STRING Path,
    _In_ USHORT StartCch,
    _In_ PCUNICODE_STRING Component)
{
    UNICODE_STRING Slice;
    USHORT PathCch, ComponentCch;

    if (Path == NULL || Component == NULL || Component->Length == 0)
    {
        return FALSE;
    }

    PathCch = Path->Length / sizeof(WCHAR);
    ComponentCch = Component->Length / sizeof(WCHAR);
    if (StartCch + ComponentCch > PathCch)
    {
        return FALSE;
    }

    if ((StartCch != 0 && Path->Buffer[StartCch - 1] != L'\\') ||
        (StartCch + ComponentCch != PathCch && Path->Buffer[StartCch + ComponentCch] != L'\\'))
    {
        return FALSE;
    }

    Slice.Buffer = Path->Buffer + StartCch;
    Slice.Length = Component->Length;
    Slice.MaximumLength = Component->Length;
    return RtlEqualUnicodeString(&Slice, Component, TRUE);
}

BOOLEAN
StringContainsInSensetive(
    _In_ PCUNICODE_STRING String,
    _In_ PCUNICODE_STRING Token)
{
    USHORT StringCch, TokenCch, iStart, i;

    if (String == NULL || Token == NULL || Token->Length == 0 || String->Length < Token->Length)
    {
        return FALSE;
    }

    StringCch = String->Length / sizeof(WCHAR);
    TokenCch = Token->Length / sizeof(WCHAR);
    for (iStart = 0; iStart + TokenCch <= StringCch; iStart++)
    {
        for (i = 0; i < TokenCch; i++)
        {
            if (RtlUpcaseUnicodeChar(String->Buffer[iStart + i]) != RtlUpcaseUnicodeChar(Token->Buffer[i]))
            {
                break;
            }
        }
        if (i == TokenCch)
        {
            return TRUE;
        }
    }

    return FALSE;
}

BOOLEAN
PathEndsWithComponentInsensitive(
    _In_ PCUNICODE_STRING Path,
    _In_ PCUNICODE_STRING FileName)
{
    if (Path == NULL || FileName == NULL || Path->Length < FileName->Length)
    {
        return FALSE;
    }

    return PathComponentEqualsAtInsensitive(
        Path,
        (USHORT)((Path->Length - FileName->Length) / sizeof(WCHAR)),
        FileName);
}

BOOLEAN
PathContainsComponentInsensitive(
    _In_ PCUNICODE_STRING Path,
    _In_ PCUNICODE_STRING Component)
{
    USHORT PathCch, ComponentCch, iStart;

    if (Path == NULL || Component == NULL || Component->Length == 0 || Path->Length < Component->Length)
    {
        return FALSE;
    }

    PathCch = Path->Length / sizeof(WCHAR);
    ComponentCch = Component->Length / sizeof(WCHAR);
    for (iStart = 0; iStart + ComponentCch <= PathCch; iStart++)
    {
        if (PathComponentEqualsAtInsensitive(Path, iStart, Component))
        {
            return TRUE;
        }
    }

    return FALSE;
}
