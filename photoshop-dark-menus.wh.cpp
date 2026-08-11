// ==WindhawkMod==
// @id            photoshop-dark-menus
// @name          Photoshop Dark Menus
// @description   Enables dark mode and custom separator colors for all menus in Adobe Photoshop.
// @version       1.2.0
// @author        Saber Naeemi
// @include       Photoshop.exe
// @compilerOptions -lUser32 -lGdi32 -lAdvapi32
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Photoshop Dark Menus

This Windhawk mod enables dark menus (top-bar dropdowns and context menus) in Adobe Photoshop on Windows 11, along with custom color settings.

This mod dynamically updates the active Windows session palette and intercepts GDI line drawing calls to force dark menu styling without making permanent modifications to registry keys on disk.

## Screenshots

![Top Bar Menu Dropdown](https://raw.githubusercontent.com/sabergraphics/photoshop-dark-menus/main/images/photoshop-dark-menu-screenshot-1.png)

![Context Menu](https://raw.githubusercontent.com/sabergraphics/photoshop-dark-menus/main/images/photoshop-dark-menu-screenshot-2.png)

## Why a Standalone Mod?

While the [Dark mode context menus](https://github.com/MGGSK/DarkMenus) (`dark-menus`) Windhawk mod provides system-wide dark Win32 menus, it does not function correctly for Photoshop due to how Adobe implements its legacy UI. This standalone mod was created out of necessity to address that gap, while offering two Photoshop-specific advantages:

1. Photoshop draws its menu separators using legacy Win32 GDI functions (`PatBlt` and `FillRect`). To reliably color or hide these specific separators, this mod strictly targets device contexts belonging to active menu windows (class `#32768` or modal `GUI_INMENUMODE`). Merging these highly specific, application-tailored GDI hooks into a global, system-wide mod (`@include *`) would risk causing visual glitches and false-positive artifacts in other applications.

2. Because Photoshop relies on legacy Win32 standard menus for its top bar, which are drawn globally by the Windows kernel, normal user-mode color hooks fail to theme their backgrounds. This mod uses `SetSysColors` as the only way to successfully force the kernel to theme Photoshop's menus, but employs strict safeguards (including registry-backed color backups and `ExitProcess` teardown hooks) to ensure the changes only persist while Photoshop is actively running.

## Important Note on Hard Crashes
Because Photoshop's proprietary engine ignores standard Windows process-local theming, this mod relies on updating global Windows system colors to force the menus to be dark. The mod uses `ExitProcess` teardown hooks to safely restore your original system colors the moment you close the app. 

However, if Photoshop experiences a **hard crash** (e.g., from a faulty plugin, or if you Force Quit it via Task Manager), normal exit routines are bypassed and your Windows desktop may remain in the dark color scheme. If this happens, simply launch Photoshop again and close it normally to immediately restore your original colors.

## Options
The following settings can be customized in the Windhawk mod panel:
- **Menu Background Color**: Background color for all menu popups (Default: `#282828`).
- **Menu Text Color**: Text color for active items (Default: `#DCDCDC`).
- **Highlight Background Color**: Color when hovering over an item (Default: `#505050`).
- **Highlight Text Color**: Text color when hovering over an item (Default: `#FFFFFF`).
- **Separator Line Color**: Color for separator lines. Set to match the background color to hide them completely (Default: `#383838`).
- **Disabled Text Color**: Text color for disabled menu items (Default: `#808080`).
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
- HighlightTextColor: "#FFFFFF"
  $name: "Highlight Text Color"
- SeparatorColor: "#383838"
  $name: "Separator Line Color"
- GrayTextColor: "#808080"
  $name: "Disabled Text Color"
*/
// ==/WindhawkModSettings==

#include <windows.h>
#include <windhawk_api.h>
#include <windhawk_utils.h>
#include <wchar.h>
#include <vector>
#include <mutex>

constexpr int NUM_ELEMENTS = 10;
const INT g_sysElements[NUM_ELEMENTS] = {
    COLOR_MENU, COLOR_MENUTEXT, COLOR_HIGHLIGHT, COLOR_HIGHLIGHTTEXT,
    COLOR_BTNSHADOW, COLOR_GRAYTEXT, COLOR_BTNHIGHLIGHT, 
    COLOR_3DDKSHADOW, COLOR_3DLIGHT, COLOR_MENUBAR
};

COLORREF g_origColors[NUM_ELEMENTS] = {};
bool g_hasSavedOrigColors = false;

HBRUSH g_hSeparatorBrush = nullptr;
std::vector<HBRUSH> g_oldBrushes;
std::mutex g_brushMutex;

COLORREF ParseHexColor(const PCWSTR hexStr, COLORREF defaultColor) {
    if (!hexStr) {
        Wh_Log(L"ParseHexColor: Null string, using default.");
        return defaultColor;
    }
    
    const wchar_t* p = hexStr;
    if (*p == L'#') p++; // allow with or without #
    
    size_t len = wcslen(p);
    unsigned int r, g, b;
    
    if (len == 6 && swscanf_s(p, L"%02x%02x%02x", &r, &g, &b) == 3) {
        return RGB(r, g, b);
    } else if (len == 3 && swscanf_s(p, L"%1x%1x%1x", &r, &g, &b) == 3) {
        return RGB(r * 17, g * 17, b * 17); // expand short hex
    }
    
    Wh_Log(L"ParseHexColor: Invalid format '%s', using default.", hexStr);
    return defaultColor;
}

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
            DWORD bufferSize = sizeof(buffer) - sizeof(WCHAR); 
            DWORD type = 0;
            
            if (RegQueryValueExW(hKey, valueName, nullptr, &type, (LPBYTE)buffer, &bufferSize) == ERROR_SUCCESS) {
                if (type == REG_SZ) {
                    buffer[bufferSize / sizeof(WCHAR)] = L'\0'; 
                    int r, g, b;
                    if (swscanf_s(buffer, L"%d %d %d", &r, &g, &b) == 3) {
                        RegCloseKey(hKey);
                        return RGB(r, g, b);
                    }
                }
            }
            RegCloseKey(hKey);
        }
    }
    Wh_Log(L"Failed to read %s from registry. Falling back to live color.", valueName ? valueName : L"unknown");
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

    COLORREF colMenu      = ParseHexColor(WindhawkUtils::StringSetting::make(L"MenuBgColor").get(), RGB(40, 40, 40));
    COLORREF colText      = ParseHexColor(WindhawkUtils::StringSetting::make(L"MenuTextColor").get(), RGB(220, 220, 220));
    COLORREF colHighlight = ParseHexColor(WindhawkUtils::StringSetting::make(L"HighlightBgColor").get(), RGB(80, 80, 80));
    COLORREF colHiText    = ParseHexColor(WindhawkUtils::StringSetting::make(L"HighlightTextColor").get(), RGB(255, 255, 255));
    COLORREF colSep       = ParseHexColor(WindhawkUtils::StringSetting::make(L"SeparatorColor").get(), RGB(56, 56, 56));
    COLORREF colGray      = ParseHexColor(WindhawkUtils::StringSetting::make(L"GrayTextColor").get(), RGB(128, 128, 128));

    HBRUSH hNewSepBrush = CreateSolidBrush(colSep);
    HBRUSH hOldSepBrush = (HBRUSH)InterlockedExchangePointer((PVOID*)&g_hSeparatorBrush, hNewSepBrush);
    
    if (hOldSepBrush) {
        std::lock_guard<std::mutex> lock(g_brushMutex);
        g_oldBrushes.push_back(hOldSepBrush);
    }

    COLORREF darkColors[NUM_ELEMENTS] = {
        colMenu, colText, colHighlight, colHiText,
        colMenu, colGray, colMenu, colMenu, colMenu, colMenu
    };

    SetSysColors(NUM_ELEMENTS, g_sysElements, darkColors);
}

void RestoreOriginalColors() {
    if (g_hasSavedOrigColors) {
        SetSysColors(NUM_ELEMENTS, g_sysElements, g_origColors);
    }
}

bool IsMenuContext(HDC hdc) {
    HWND hWnd = WindowFromDC(hdc);
    if (hWnd) {
        WCHAR cls[16];
        if (GetClassNameW(hWnd, cls, ARRAYSIZE(cls)) && wcscmp(cls, L"#32768") == 0) return true;
        return false;
    } 
    if (GetObjectType(hdc) == OBJ_MEMDC) {
        GUITHREADINFO gti = { sizeof(GUITHREADINFO) };
        if (GetGUIThreadInfo(GetCurrentThreadId(), &gti)) {
            if (gti.flags & GUI_INMENUMODE) return true;
        }
    }
    return false;
}

// Hooks
decltype(&PatBlt) PatBlt_Original;
BOOL WINAPI PatBlt_Hook(HDC hdc, int x, int y, int w, int h, DWORD rop) {
    HBRUSH hBrush = g_hSeparatorBrush; 
    if (rop == PATCOPY && (h == 1 || h == 2) && w > 20 && hBrush) {
        if (IsMenuContext(hdc)) {
            HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hBrush);
            BOOL bRes = PatBlt_Original(hdc, x, y, w, h, rop);
            SelectObject(hdc, hOldBrush);
            return bRes;
        }
    }
    return PatBlt_Original(hdc, x, y, w, h, rop);
}

decltype(&FillRect) FillRect_Original;
int WINAPI FillRect_Hook(HDC hdc, const RECT *lprc, HBRUSH hbr) {
    HBRUSH hBrush = g_hSeparatorBrush; 
    if (lprc && hBrush) {
        int h = lprc->bottom - lprc->top;
        int w = lprc->right - lprc->left;
        if ((h == 1 || h == 2) && w > 20) {
            if (IsMenuContext(hdc)) {
                return FillRect_Original(hdc, lprc, hBrush);
            }
        }
    }
    return FillRect_Original(hdc, lprc, hbr);
}

decltype(&ExitProcess) ExitProcess_Original;
__declspec(noreturn) void WINAPI ExitProcess_Hook(UINT uExitCode) {
    RestoreOriginalColors();
    ExitProcess_Original(uExitCode);
}

// Windhawk Events
void Wh_ModSettingsChanged() { ApplyDarkSystemColors(); }

BOOL Wh_ModInit() {
    ApplyDarkSystemColors();

    if (!WindhawkUtils::SetFunctionHook(PatBlt, PatBlt_Hook, &PatBlt_Original)) Wh_Log(L"Failed to hook PatBlt");
    if (!WindhawkUtils::SetFunctionHook(FillRect, FillRect_Hook, &FillRect_Original)) Wh_Log(L"Failed to hook FillRect");
    if (!WindhawkUtils::SetFunctionHook(ExitProcess, ExitProcess_Hook, &ExitProcess_Original)) Wh_Log(L"Failed to hook ExitProcess");

    return TRUE;
}

void Wh_ModUninit() {
    RestoreOriginalColors();
    
    if (g_hSeparatorBrush) DeleteObject(g_hSeparatorBrush);
    std::lock_guard<std::mutex> lock(g_brushMutex);
    for (HBRUSH b : g_oldBrushes) DeleteObject(b);
    g_oldBrushes.clear();
}