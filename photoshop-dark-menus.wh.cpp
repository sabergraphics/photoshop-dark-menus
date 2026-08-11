// ==WindhawkMod==
// @id            photoshop-dark-menus
// @name          Photoshop Dark Menus
// @description   Enables dark mode and custom separator colors for all menus in Adobe Photoshop.
// @version       1.1
// @author        Saber Naeemi
// @github        https://github.com/sabergraphics
// @twitter       https://x.com/SaberNaeemi
// @homepage      https://www.sabernaeemi.com
// @include       Photoshop.exe
// @compilerOptions -lUser32 -lGdi32 -lAdvapi32
// @license       MIT
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Photoshop Dark Menus

Forces dark mode for all context and top menu bar dropdowns in Adobe Photoshop without permanently altering registry keys on disk.

### Features
- Dark backgrounds and customizable text colors across all menus.
- Independent separator line color control (set to match menu background to hide separators).
- Legible disabled item text styling.
- Automatic restoration of default Windows system colors upon exiting Photoshop.

### Screenshots
![Photoshop Dark Menu Dropdown](https://raw.githubusercontent.com/sabergraphics/photoshop-dark-menus/main/images/photoshop-dark-menu-screenshot-1.png)

![Photoshop Dark Context Menu](https://raw.githubusercontent.com/sabergraphics/photoshop-dark-menus/main/images/photoshop-dark-menu-screenshot-2.png)
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- MenuBgColor: "#282828"
  $name: "Menu Background Color"
- MenuTextColor: "#DCDCDC"
  $name: "Menu Text Color"
- HighlightBgColor: "#505050"
  $name: "Highlight Background Color"
- SeparatorColor: "#383838"
  $name: "Separator Line Color (Set same as Menu Background Color to hide)"
- GrayTextColor: "#808080"
  $name: "Disabled Text Color"
*/
// ==/WindhawkModSettings==

#include <windows.h>
#include <windhawk_api.h>
#include <wchar.h>

constexpr int NUM_ELEMENTS = 10;

const INT g_sysElements[NUM_ELEMENTS] = {
    COLOR_MENU,
    COLOR_MENUTEXT,
    COLOR_HIGHLIGHT,
    COLOR_HIGHLIGHTTEXT,
    COLOR_BTNSHADOW,
    COLOR_GRAYTEXT,
    COLOR_BTNHIGHLIGHT,
    COLOR_3DDKSHADOW,
    COLOR_3DLIGHT,
    COLOR_MENUBAR
};

COLORREF g_origColors[NUM_ELEMENTS] = {};
bool g_hasSavedOrigColors = false;

HBRUSH g_hSeparatorBrush = nullptr;

COLORREF ParseHexColor(const PCWSTR hexStr, COLORREF defaultColor) {
    if (!hexStr || wcslen(hexStr) < 7 || hexStr[0] != L'#')
        return defaultColor;

    unsigned int r = 0, g = 0, b = 0;
    if (swscanf_s(hexStr + 1, L"%02x%02x%02x", &r, &g, &b) == 3) {
        return RGB(r, g, b);
    }
    return defaultColor;
}

// Safely fetches the true system colors from the registry, bypassing corrupted memory
COLORREF GetColorFromRegistry(int sysElement, COLORREF liveColorFallback) {
    const wchar_t* valueName = nullptr;
    switch (sysElement) {
        case COLOR_MENU: valueName = L"Menu"; break;
        case COLOR_MENUTEXT: valueName = L"MenuText"; break;
        case COLOR_HIGHLIGHT: valueName = L"Hilight"; break;
        case COLOR_HIGHLIGHTTEXT: valueName = L"HilightText"; break;
        case COLOR_BTNSHADOW: valueName = L"ButtonShadow"; break;
        case COLOR_GRAYTEXT: valueName = L"GrayText"; break;
        case COLOR_BTNHIGHLIGHT: valueName = L"ButtonHilight"; break;
        case COLOR_3DDKSHADOW: valueName = L"ButtonDkShadow"; break;
        case COLOR_3DLIGHT: valueName = L"ButtonLight"; break;
        case COLOR_MENUBAR: valueName = L"MenuBar"; break;
    }

    if (valueName) {
        HKEY hKey;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Control Panel\\Colors", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            wchar_t buffer[64] = {0};
            DWORD bufferSize = sizeof(buffer);
            if (RegQueryValueExW(hKey, valueName, nullptr, nullptr, (LPBYTE)buffer, &bufferSize) == ERROR_SUCCESS) {
                int r, g, b;
                if (swscanf_s(buffer, L"%d %d %d", &r, &g, &b) == 3) {
                    RegCloseKey(hKey);
                    return RGB(r, g, b);
                }
            }
            RegCloseKey(hKey);
        }
    }
    return liveColorFallback;
}

void SaveOriginalColors() {
    if (g_hasSavedOrigColors) return;

    for (int i = 0; i < NUM_ELEMENTS; i++) {
        g_origColors[i] = GetColorFromRegistry(g_sysElements[i], GetSysColor(g_sysElements[i]));
    }
    g_hasSavedOrigColors = true;
}

void ApplyDarkSystemColors() {
    SaveOriginalColors();

    const PCWSTR bgStr        = Wh_GetStringSetting(L"MenuBgColor");
    const PCWSTR textStr      = Wh_GetStringSetting(L"MenuTextColor");
    const PCWSTR highlightStr = Wh_GetStringSetting(L"HighlightBgColor");
    const PCWSTR sepStr       = Wh_GetStringSetting(L"SeparatorColor");
    const PCWSTR grayStr      = Wh_GetStringSetting(L"GrayTextColor");

    COLORREF colMenu      = ParseHexColor(bgStr,        RGB(40, 40, 40));
    COLORREF colText      = ParseHexColor(textStr,      RGB(220, 220, 220));
    COLORREF colHighlight = ParseHexColor(highlightStr, RGB(80, 80, 80));
    COLORREF colSep       = ParseHexColor(sepStr,       RGB(56, 56, 56));
    COLORREF colGray      = ParseHexColor(grayStr,      RGB(128, 128, 128));

    Wh_FreeStringSetting(bgStr);
    Wh_FreeStringSetting(textStr);
    Wh_FreeStringSetting(highlightStr);
    Wh_FreeStringSetting(sepStr);
    Wh_FreeStringSetting(grayStr);

    HBRUSH hNewSepBrush = CreateSolidBrush(colSep);
    HBRUSH hOldSepBrush = (HBRUSH)InterlockedExchangePointer((PVOID*)&g_hSeparatorBrush, hNewSepBrush);
    if (hOldSepBrush) DeleteObject(hOldSepBrush);

    COLORREF darkColors[NUM_ELEMENTS] = {
        colMenu,
        colText,
        colHighlight,
        RGB(255, 255, 255),
        colMenu,
        colGray,
        colMenu,
        colMenu,
        colMenu,
        colMenu
    };

    SetSysColors(NUM_ELEMENTS, g_sysElements, darkColors);
}

void RestoreOriginalColors() {
    if (g_hasSavedOrigColors) {
        SetSysColors(NUM_ELEMENTS, g_sysElements, g_origColors);
    }
    
    HBRUSH hOldBrush = (HBRUSH)InterlockedExchangePointer((PVOID*)&g_hSeparatorBrush, nullptr);
    if (hOldBrush) {
        DeleteObject(hOldBrush);
    }
}

// -------------------------------------------------------------------------
// Hooks
// -------------------------------------------------------------------------

using PatBlt_t = BOOL (WINAPI*)(HDC hdc, int x, int y, int w, int h, DWORD rop);
PatBlt_t PatBlt_Original = nullptr;

BOOL WINAPI PatBlt_Hook(HDC hdc, int x, int y, int w, int h, DWORD rop) {
    HBRUSH hBrush = g_hSeparatorBrush; 
    if ((h == 1 || h == 2) && w > 20 && hBrush) {
        HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hBrush);
        BOOL bRes = PatBlt_Original(hdc, x, y, w, h, rop);
        SelectObject(hdc, hOldBrush);
        return bRes;
    }
    return PatBlt_Original(hdc, x, y, w, h, rop);
}

using FillRect_t = int (WINAPI*)(HDC hdc, const RECT *lprc, HBRUSH hbr);
FillRect_t FillRect_Original = nullptr;

int WINAPI FillRect_Hook(HDC hdc, const RECT *lprc, HBRUSH hbr) {
    HBRUSH hBrush = g_hSeparatorBrush; 
    if (lprc && hBrush) {
        int h = lprc->bottom - lprc->top;
        int w = lprc->right - lprc->left;
        if ((h == 1 || h == 2) && w > 20) {
            return FillRect_Original(hdc, lprc, hBrush);
        }
    }
    return FillRect_Original(hdc, lprc, hbr);
}

// Intercept application exit to restore colors safely
using ExitProcess_t = void (WINAPI*)(UINT uExitCode);
ExitProcess_t ExitProcess_Original = nullptr;

void WINAPI ExitProcess_Hook(UINT uExitCode) {
    Wh_Log(L"Photoshop is exiting, restoring original system colors...");
    RestoreOriginalColors();
    ExitProcess_Original(uExitCode);
}

// -------------------------------------------------------------------------
// Windhawk Events
// -------------------------------------------------------------------------

void Wh_ModSettingsChanged() {
    ApplyDarkSystemColors();
}

BOOL Wh_ModInit() {
    Wh_Log(L"Initializing Photoshop Dark Menus");
    ApplyDarkSystemColors();

    HMODULE hGdi32 = GetModuleHandleW(L"gdi32.dll");
    if (hGdi32) {
        void* pPatBlt = (void*)GetProcAddress(hGdi32, "PatBlt");
        if (pPatBlt) Wh_SetFunctionHook(pPatBlt, (void*)PatBlt_Hook, (void**)&PatBlt_Original);
    }

    HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
    if (hUser32) {
        void* pFillRect = (void*)GetProcAddress(hUser32, "FillRect");
        if (pFillRect) Wh_SetFunctionHook(pFillRect, (void*)FillRect_Hook, (void**)&FillRect_Original);
    }

    HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
    if (hKernel32) {
        void* pExitProcess = (void*)GetProcAddress(hKernel32, "ExitProcess");
        if (pExitProcess) Wh_SetFunctionHook(pExitProcess, (void*)ExitProcess_Hook, (void**)&ExitProcess_Original);
    }

    return TRUE;
}

void Wh_ModUninit() {
    Wh_Log(L"Restoring original system palette colors");
    RestoreOriginalColors();
}