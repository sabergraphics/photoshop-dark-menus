// ==WindhawkMod==
// @id            photoshop-dark-menus
// @name          Photoshop Dark Menus
// @description   Enables dark mode and custom colors for the menus and dropdown lists in Adobe Photoshop.
// @version       1.1.0
// @author        Saber Naeemi
// @github        https://github.com/sabergraphics
// @twitter       https://x.com/SaberNaeemi
// @homepage      https://www.sabernaeemi.com
// @include       Photoshop.exe
// @compilerOptions -lUser32 -lGdi32 -lComctl32
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Photoshop Dark Menus

This Windhawk mod enables dark menus (top-bar dropdowns and context menus) and
dark dropdown lists in Adobe Photoshop on Windows, with fully customizable
colors.

Everything happens inside the Photoshop process. No system colors are changed
and no other application is affected.

## Screenshots

![Top Bar Menu Dropdown](https://raw.githubusercontent.com/sabergraphics/photoshop-dark-menus/main/images/photoshop-dark-menu-screenshot-1.png)

![Context Menu](https://raw.githubusercontent.com/sabergraphics/photoshop-dark-menus/main/images/photoshop-dark-menu-screenshot-2.png)

## How it works

Photoshop's menus look like classic unthemed Win32 menus, but they are not:
Photoshop **owner-draws every menu item itself** through `AdobeOwl.dll`, using
plain GDI, inside its own process. Instrumenting a live Photoshop shows the
menu popups going up via `TrackPopupMenuEx` and every item being painted with:

- `FillRect(hdc, &rc, (HBRUSH)(COLOR_MENU + 1))` for the item background, and
  `(HBRUSH)(COLOR_HIGHLIGHT + 1)` for the hovered item. These **system color
  pseudo-handles** are resolved inside `user32` from shared memory, so they
  bypass `GetSysColor` and `GetSysColorBrush` hooks entirely - which is why
  earlier per-process attempts appeared to do nothing.
- `GetSysColor(COLOR_MENUTEXT / COLOR_GRAYTEXT / COLOR_HIGHLIGHTTEXT)` for text.
- Its own 1px `FillRect` for separator lines.

So every pixel of a menu item is reachable from inside the process:

- `FillRect` / `PatBlt` are hooked and, while a menu item is being painted,
  sys-color pseudo-handles are re-resolved through this mod's own color table,
  and 1px fills inside a menu DC are recolored to the separator color.
- `GetSysColor` / `GetSysColorBrush` are hooked for the menu color indices, but
  **only while a menu item is actually being painted** - that is, inside a
  `WM_DRAWITEM` of type `ODT_MENU` - so Photoshop's dialogs and panels keep the
  system colors. "While a menu is open" would be far too broad: a menu runs a
  modal message loop that keeps dispatching to every other window on the thread,
  and an unrelated repaint would pick the menu colors up.
- `MENUINFO::hbrBack` is stamped on each popup (and its submenus) at
  `WM_INITMENUPOPUP`, covering the popup background that the system paints
  around the owner-drawn items.
- The popup's non-client frame is the one part drawn kernel-side, from the 3D
  colors, where no user-mode hook can reach it - so the popup is subclassed at
  creation and the frame is repainted with the border color after it. Set
  **Border Color** empty to leave the system frame alone.

The dropdown lists - the Layers panel blend mode list, the options bar lists,
the Glyphs panel lists - are stock Win32 combo boxes whose list is user32's own
`ComboLBox` window. Photoshop owner-draws the rows but takes their colors from
the system, and not consistently: one list reads `COLOR_WINDOWTEXT` for its row
text where another reads `COLOR_MENUTEXT`, and one fills its rows from a color it
asks for where another fills them with a `(HBRUSH)(COLOR_X + 1)` pseudo-handle.
Both routes are therefore covered, over a table that maps every background index
to the background color and every text index to the text color. The list window
is subclassed so that all of it applies only while that list is painting, which
leaves every other control reading those same colors alone.

## Relationship to the `dark-menus` mod

The system-wide [Dark mode context menus](https://windhawk.net/mods/dark-menus)
(`dark-menus`) mod does **not** cover the menus this one covers. The two are
complementary rather than alternatives, and it is worth running both.

`dark-menus` puts a process into uxtheme's dark menu theme with
`SetPreferredAppMode` + `FlushMenuThemes`, which reaches the menus uxtheme
itself draws. Photoshop's own menus are not among them: it owner-draws every
item with plain GDI, so there is nothing for that theme to attach to. This mod
intercepts Photoshop's own drawing instead, and adds per-color customization
scoped to the one application.

The reverse holds too. A few menus in Photoshop are put up by Windows rather
than drawn by Photoshop - the context menu on a panel tab, for one - and those
are painted by the theming engine, below the level any in-process hook here can
reach. `dark-menus` darkens exactly those. Between them the two cover the set.

## Options

- **Menu Background Color**: Background color for all menu popups (Default: `#2D2D2D`).
- **Menu Text Color**: Text color for active items (Default: `#DCDCDC`).
- **Highlight Background Color**: Color when hovering over an item (Default: `#505050`).
- **Highlight Text Color**: Text color when hovering over an item (Default: `#FFFFFF`).
- **Separator Line Color**: Color for separator lines. Set to match the background color to hide them completely (Default: `#383838`).
- **Disabled Text Color**: Text color for disabled menu items (Default: `#808080`).
- **Border Color**: Color of the popup frame. Leave empty to keep the system frame (Default: `#2D2D2D`).
- **Theme Dropdown Lists**: Darken the dropdown lists as well, using the colors above (Default: on).

Colors can be given as `#RRGGBB` or the short `#RGB` form.

## Scope

The menu bar itself (File, Edit, Image, ...) is drawn by Photoshop's own UI
framework and already follows Photoshop's interface theme; this mod covers the
dropdown popups and context menus.

Menus that Windows pops up through its own internal paths, without going through
the exported `TrackPopupMenu` / `TrackPopupMenuEx` - a window's system menu, for
example - are outside the reach of an in-process hook and keep the system colors.

The font browser and the Glyphs panel's character cells stay light too, and are
outside what this mod can reach: Photoshop paints those from its own palette
rather than from any system color, mixing its dark chrome and their light
content in the same window with the same calls. Recoloring them would mean
matching color values rather than intercepting a system one, in a window class
Photoshop also uses for its swatches and color pickers - where a light fill is
the content.

## Changelog

### 1.1.0
- Dropdown lists are themed as well, using the same colors - the Layers panel
  blend mode list, the options bar lists, the Glyphs panel lists and the other
  combo boxes. Turn it off with **Theme Dropdown Lists**.
- The color overrides now apply only while a menu item or a dropdown row is
  actually being painted, rather than for as long as a menu is open. A menu runs
  a modal message loop that keeps dispatching to every other window on its
  thread, so an unrelated repaint could previously come out in menu colors.
- Menus destroyed over a long session no longer fill the tracked-menu cap and
  leave later menus unthemed.
- Changing one color no longer recreates all seven brushes.
*/
// ==/WindhawkModReadme==


// ==WindhawkModSettings==
/*
- MenuBgColor: "#2D2D2D"
  $name: "Menu Background Color"
- MenuTextColor: "#DCDCDC"
  $name: "Menu Text Color"
- HighlightBgColor: "#505050"
  $name: "Highlight Background Color"
- HighlightTextColor: "#FFFFFF"
  $name: "Highlight Text Color"
- SeparatorColor: "#383838"
  $name: "Separator Line Color"
  $description: Set this to the menu background color to hide separators.
- GrayTextColor: "#808080"
  $name: "Disabled Text Color"
- BorderColor: "#2D2D2D"
  $name: "Border Color"
  $description: >-
    Color of the popup frame, which the system draws from the 3D colors. Leave
    empty to keep the system frame.
- ThemeDropdowns: true
  $name: "Theme Dropdown Lists"
  $description: >-
    Also darken the dropdown lists, such as the Layers panel blend mode list,
    using the colors above.
*/
// ==/WindhawkModSettings==

#include <windows.h>
#include <commctrl.h>
#include <windhawk_api.h>
#include <windhawk_utils.h>
#include <wchar.h>
#include <wctype.h>
#include <atomic>
#include <mutex>
#include <unordered_set>
#include <vector>

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

// Colors read by the in-process hooks. Each one doubles as the cache that keeps
// a settings change from recreating a brush whose color did not move; they hold
// the defaults rather than a sentinel so that a color is always a real color,
// even if a brush ever fails to be created. The border is the exception -
// CLR_INVALID is how "leave the system frame alone" is spelled.
std::atomic<COLORREF> g_colMenu{RGB(45, 45, 45)};
std::atomic<COLORREF> g_colText{RGB(220, 220, 220)};
std::atomic<COLORREF> g_colHighlight{RGB(80, 80, 80)};
std::atomic<COLORREF> g_colHiText{RGB(255, 255, 255)};
std::atomic<COLORREF> g_colGray{RGB(128, 128, 128)};
std::atomic<COLORREF> g_colSeparator{RGB(56, 56, 56)};
std::atomic<COLORREF> g_colBorder{CLR_INVALID};

std::atomic<bool> g_themeDropdowns{true};

// Brushes are published with an atomic exchange and never deleted - see the
// note in Wh_ModUninit.
std::atomic<HBRUSH> g_hMenuBrush{nullptr};
std::atomic<HBRUSH> g_hTextBrush{nullptr};
std::atomic<HBRUSH> g_hHighlightBrush{nullptr};
std::atomic<HBRUSH> g_hHiTextBrush{nullptr};
std::atomic<HBRUSH> g_hGrayBrush{nullptr};
std::atomic<HBRUSH> g_hSeparatorBrush{nullptr};
std::atomic<HBRUSH> g_hBorderBrush{nullptr};
std::vector<HBRUSH> g_retiredBrushes;
std::mutex g_brushMutex;

// Number of menu tracking calls currently on this thread's stack. This is what
// decides whether the per-menu hooks are needed, not whether a color should be
// overridden - a menu keeps a modal message loop running that dispatches to
// every other window on the thread, so "a menu is open" says nothing about what
// is being painted right now.
thread_local int tl_menuDepth = 0;

// The two contexts that do decide it. Instrumenting Photoshop shows every menu
// item pixel - background, text and separators alike - painted inside a
// WM_DRAWITEM whose CtlType is ODT_MENU, and the whole of a dropdown list
// painted inside the ComboLBox window's own message handling. Both are narrow,
// both are thread-local, and outside them the hooks are a single TLS read.
thread_local int tl_menuDrawDepth = 0;
thread_local int tl_comboListDepth = 0;

// Set while this thread is painting on the mod's behalf, so our own drawing
// does not re-enter the hooks.
thread_local bool tl_inOurPaint = false;

// The hooks belong to a UI thread rather than to a menu: dropdown lists are
// painted with no menu in sight, and both they and the menu popups are created
// once and kept for the life of the process - usually well before this mod
// loaded - so nothing about them can be caught at creation alone. See
// EnsureThreadHooks and InstallThreadHook.

// Unlike the mod's function hooks, SetWindowsHookEx hooks are not removed by the
// Windhawk engine, and their procedures live in the mod image - so one still
// registered when the image is unmapped calls into freed memory on the next
// message for that thread. The handles are tracked globally because Wh_ModUninit
// never runs on the thread that installed them: while a menu is up that thread
// is blocked in the modal menu loop. UnhookWindowsHookEx is not thread-affine,
// so the handles can be released from any thread in the process.
std::unordered_set<HHOOK> g_winHooks;
std::mutex g_winHooksMutex;

void TrackWinHook(HHOOK hHook) {
    if (!hHook) return;

    std::lock_guard<std::mutex> lock(g_winHooksMutex);
    g_winHooks.insert(hHook);
}

// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------

// Returns false for an empty string, so a setting can mean "leave it alone".
bool ParseHexColor(PCWSTR hexStr, COLORREF* pOut) {
    if (!hexStr) return false;

    const wchar_t* p = hexStr;
    while (*p == L' ') p++;
    if (*p == L'#') p++;

    size_t len = wcslen(p);
    while (len > 0 && p[len - 1] == L' ') len--;
    if (len == 0) return false;

    bool allHex = true;
    for (size_t i = 0; i < len && allHex; i++) {
        if (!iswxdigit(p[i])) allHex = false;
    }

    unsigned int r, g, b;
    if (allHex && len == 6 && swscanf_s(p, L"%02x%02x%02x", &r, &g, &b) == 3) {
        *pOut = RGB(r, g, b);
        return true;
    }
    if (allHex && len == 3 && swscanf_s(p, L"%1x%1x%1x", &r, &g, &b) == 3) {
        *pOut = RGB(r * 17, g * 17, b * 17);  // expand short hex
        return true;
    }

    Wh_Log(L"ParseHexColor: invalid format '%s', using default.", hexStr);
    return false;
}

COLORREF ParseHexColorOr(PCWSTR hexStr, COLORREF defaultColor) {
    COLORREF c;
    return ParseHexColor(hexStr, &c) ? c : defaultColor;
}

void PublishBrush(std::atomic<HBRUSH>* pSlot, HBRUSH hNew) {
    HBRUSH hOld = pSlot->exchange(hNew, std::memory_order_release);
    if (hOld) {
        std::lock_guard<std::mutex> lock(g_brushMutex);
        g_retiredBrushes.push_back(hOld);
    }
}

// Retired brushes are never deleted, so a settings change that recreated all
// seven would leak six of them for nothing. Only the colors that actually moved
// get a new brush.
void PublishColor(std::atomic<COLORREF>* pColor, std::atomic<HBRUSH>* pSlot, COLORREF color) {
    if (pColor->load(std::memory_order_relaxed) == color &&
        pSlot->load(std::memory_order_acquire)) {
        return;
    }

    HBRUSH hNew = CreateSolidBrush(color);
    if (!hNew) return;

    pColor->store(color, std::memory_order_relaxed);
    PublishBrush(pSlot, hNew);
}

void LoadSettings() {
    g_themeDropdowns.store(Wh_GetIntSetting(L"ThemeDropdowns") != 0, std::memory_order_relaxed);

    PublishColor(&g_colMenu, &g_hMenuBrush,
                 ParseHexColorOr(WindhawkUtils::StringSetting::make(L"MenuBgColor").get(), RGB(45, 45, 45)));
    PublishColor(&g_colText, &g_hTextBrush,
                 ParseHexColorOr(WindhawkUtils::StringSetting::make(L"MenuTextColor").get(), RGB(220, 220, 220)));
    PublishColor(&g_colHighlight, &g_hHighlightBrush,
                 ParseHexColorOr(WindhawkUtils::StringSetting::make(L"HighlightBgColor").get(), RGB(80, 80, 80)));
    PublishColor(&g_colHiText, &g_hHiTextBrush,
                 ParseHexColorOr(WindhawkUtils::StringSetting::make(L"HighlightTextColor").get(), RGB(255, 255, 255)));
    PublishColor(&g_colGray, &g_hGrayBrush,
                 ParseHexColorOr(WindhawkUtils::StringSetting::make(L"GrayTextColor").get(), RGB(128, 128, 128)));
    PublishColor(&g_colSeparator, &g_hSeparatorBrush,
                 ParseHexColorOr(WindhawkUtils::StringSetting::make(L"SeparatorColor").get(), RGB(56, 56, 56)));

    // Empty means "keep the system frame".
    COLORREF colBorder;
    if (ParseHexColor(WindhawkUtils::StringSetting::make(L"BorderColor").get(), &colBorder)) {
        PublishColor(&g_colBorder, &g_hBorderBrush, colBorder);
    } else if (g_hBorderBrush.load(std::memory_order_acquire)) {
        g_colBorder.store(CLR_INVALID, std::memory_order_relaxed);
        PublishBrush(&g_hBorderBrush, nullptr);
    }
}

// ---------------------------------------------------------------------------
// Menu scoping
// ---------------------------------------------------------------------------

inline bool MenuIsOpen() {
    return tl_menuDepth > 0;
}

// Photoshop paints menu items into a popup-sized memory DC while the popup is
// being laid out, and directly onto the popup's own window DC when an item is
// re-drawn on hover. The memory DC fallback would match any memory DC, so it
// leans on the caller having established tl_menuDrawDepth first: inside a menu's
// WM_DRAWITEM, this thread is painting that menu item and nothing else.
bool IsPaintContext(HDC hdc, PCWSTR windowClass) {
    HWND hWnd = WindowFromDC(hdc);
    if (hWnd) {
        WCHAR cls[16];
        return GetClassNameW(hWnd, cls, ARRAYSIZE(cls)) && wcscmp(cls, windowClass) == 0;
    }
    return GetObjectType(hdc) == OBJ_MEMDC;
}

// Our color for a system color index, or nullptr / false if we don't override it.
//
// The frame's 3D colors (COLOR_BTNSHADOW and friends) are deliberately not
// listed: the popup frame is drawn kernel-side and never reaches a user-mode
// GDI call, so overriding them here would change nothing - it is repainted in
// the popup's subclass instead.
HBRUSH BrushForSysColor(int nIndex) {
    switch (nIndex) {
        case COLOR_MENU:
        case COLOR_MENUBAR:       return g_hMenuBrush.load(std::memory_order_acquire);
        case COLOR_MENUTEXT:      return g_hTextBrush.load(std::memory_order_acquire);
        case COLOR_HIGHLIGHT:     return g_hHighlightBrush.load(std::memory_order_acquire);
        case COLOR_HIGHLIGHTTEXT: return g_hHiTextBrush.load(std::memory_order_acquire);
        case COLOR_GRAYTEXT:      return g_hGrayBrush.load(std::memory_order_acquire);
    }
    return nullptr;
}

bool ColorForSysColor(int nIndex, COLORREF* pOut) {
    switch (nIndex) {
        case COLOR_MENU:
        case COLOR_MENUBAR:       *pOut = g_colMenu.load(std::memory_order_relaxed); return true;
        case COLOR_MENUTEXT:      *pOut = g_colText.load(std::memory_order_relaxed); return true;
        case COLOR_HIGHLIGHT:     *pOut = g_colHighlight.load(std::memory_order_relaxed); return true;
        case COLOR_HIGHLIGHTTEXT: *pOut = g_colHiText.load(std::memory_order_relaxed); return true;
        case COLOR_GRAYTEXT:      *pOut = g_colGray.load(std::memory_order_relaxed); return true;
    }
    return false;
}

// The dropdown equivalents. Which index a list reaches for is not consistent:
// the Layers blend list takes its row text from COLOR_WINDOWTEXT, while the
// options bar's list takes it from COLOR_MENUTEXT - so rather than track down
// which control uses which, every background index maps to the background and
// every text index to the text. The mapping only applies while a list is
// painting, so a generous table costs nothing elsewhere.
bool ColorForDropdown(int nIndex, COLORREF* pOut) {
    switch (nIndex) {
        case COLOR_WINDOW:
        case COLOR_MENU:
        case COLOR_MENUBAR:       *pOut = g_colMenu.load(std::memory_order_relaxed); return true;
        case COLOR_WINDOWTEXT:
        case COLOR_MENUTEXT:      *pOut = g_colText.load(std::memory_order_relaxed); return true;
        case COLOR_HIGHLIGHT:     *pOut = g_colHighlight.load(std::memory_order_relaxed); return true;
        case COLOR_HIGHLIGHTTEXT: *pOut = g_colHiText.load(std::memory_order_relaxed); return true;
        case COLOR_GRAYTEXT:      *pOut = g_colGray.load(std::memory_order_relaxed); return true;
        case COLOR_WINDOWFRAME: {
            COLORREF border = g_colBorder.load(std::memory_order_relaxed);
            *pOut = border != CLR_INVALID ? border : g_colMenu.load(std::memory_order_relaxed);
            return true;
        }
    }
    return false;
}

HBRUSH BrushForDropdown(int nIndex) {
    switch (nIndex) {
        case COLOR_WINDOW:
        case COLOR_MENU:
        case COLOR_MENUBAR:       return g_hMenuBrush.load(std::memory_order_acquire);
        case COLOR_WINDOWTEXT:
        case COLOR_MENUTEXT:      return g_hTextBrush.load(std::memory_order_acquire);
        case COLOR_HIGHLIGHT:     return g_hHighlightBrush.load(std::memory_order_acquire);
        case COLOR_HIGHLIGHTTEXT: return g_hHiTextBrush.load(std::memory_order_acquire);
        case COLOR_GRAYTEXT:      return g_hGrayBrush.load(std::memory_order_acquire);
    }
    return nullptr;
}

// Separator geometry scales with DPI; 1px at 100% is often 2-3px at 175%.
bool IsSeparatorGeometry(HDC hdc, int w, int h) {
    int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
    if (dpi <= 0) dpi = 96;
    return h >= 1 && h <= MulDiv(3, dpi, 96) && w > MulDiv(20, dpi, 96);
}

// A background brush set on a menu is state on the menu object, not on a hook,
// so it outlives the mod: without clearing it, disabling the mod leaves every
// area the system fills from hbrBack - gaps, margins, separator bands - dark
// until Photoshop restarts. Remember what we stamped so it can be undone.
//
// Photoshop keeps a bounded set of menus, but they can be rebuilt over a long
// session, so the set is capped rather than allowed to grow without limit.
constexpr size_t kMaxTrackedMenus = 1024;
std::unordered_set<HMENU> g_stampedMenus;
std::mutex g_stampedMenusMutex;

void ApplyMenuBackground(HMENU hMenu) {
    HBRUSH hBrush = g_hMenuBrush.load(std::memory_order_acquire);
    if (!hMenu || !hBrush) return;

    // The cap is checked before stamping, not after: a menu that is stamped but
    // not recorded would stay dark for the life of the process, which is the one
    // thing the tracking exists to prevent.
    {
        std::lock_guard<std::mutex> lock(g_stampedMenusMutex);
        if (g_stampedMenus.size() >= kMaxTrackedMenus && !g_stampedMenus.contains(hMenu)) {
            // Nothing removes an entry when Photoshop destroys a menu, so a full
            // set is mostly dead handles. Drop those before giving up on this
            // menu: refusing to stamp puts themed items on an unthemed popup
            // background, which is more visible than the untracked stamp the cap
            // exists to prevent.
            std::erase_if(g_stampedMenus, [](HMENU hDead) { return !IsMenu(hDead); });
            if (g_stampedMenus.size() >= kMaxTrackedMenus) return;
        }
    }

    MENUINFO mi = { sizeof(mi) };
    mi.fMask = MIM_BACKGROUND | MIM_APPLYTOSUBMENUS;
    mi.hbrBack = hBrush;
    if (!SetMenuInfo(hMenu, &mi)) return;

    // SetMenuInfo can reach a menu owned by another thread, so it is called
    // outside the lock; two threads racing past the check can leave the set one
    // entry over the cap, which is harmless.
    std::lock_guard<std::mutex> lock(g_stampedMenusMutex);
    g_stampedMenus.insert(hMenu);
}

// MIM_APPLYTOSUBMENUS propagates the cleared brush down each menu tree, so
// submenus that were stamped but never opened are covered too.
void RestoreStampedMenus() {
    std::vector<HMENU> menus;
    {
        std::lock_guard<std::mutex> lock(g_stampedMenusMutex);
        menus.assign(g_stampedMenus.begin(), g_stampedMenus.end());
        g_stampedMenus.clear();
    }

    // SetMenuInfo can reach a menu owned by another thread, so it is called
    // with the lock released.
    for (HMENU hMenu : menus) {
        MENUINFO mi = { sizeof(mi) };
        mi.fMask = MIM_BACKGROUND | MIM_APPLYTOSUBMENUS;
        mi.hbrBack = nullptr;
        SetMenuInfo(hMenu, &mi);
    }
}

// Width of the non-client frame, i.e. how much of the window rect the system
// paints from the 3D colors.
int GetPopupFrameWidth(HWND hWnd, const RECT& windowRect) {
    RECT client;
    POINT origin = { 0, 0 };
    if (!GetClientRect(hWnd, &client) || !ClientToScreen(hWnd, &origin)) return 1;

    int left = origin.x - windowRect.left;
    int top = origin.y - windowRect.top;
    int right = windowRect.right - (origin.x + (client.right - client.left));
    int bottom = windowRect.bottom - (origin.y + (client.bottom - client.top));

    int frame = left;
    if (top > frame) frame = top;
    if (right > frame) frame = right;
    if (bottom > frame) frame = bottom;

    if (frame < 1) frame = 1;
    if (frame > 4) frame = 4;
    return frame;
}

// The popup's non-client frame is drawn by the system from COLOR_3DDKSHADOW /
// COLOR_BTNHIGHLIGHT, which no process-local hook can change - so repaint it
// once the system is done with it.
//
// hdcTarget is the DC the system rendered the popup into. Menus are drawn
// offscreen and blitted (WM_PRINT / WM_NCUAHDRAWFRAME carry that DC in wParam),
// so painting on the window DC instead would just be overwritten by the blit.
void PaintPopupBorder(HWND hWnd, HDC hdcTarget) {
    // Popups are subclassed unconditionally and the system reuses them, so a
    // popup can be shown again later by a menu this mod doesn't theme - one that
    // never went through TrackPopupMenu(Ex). Painting only while this thread is
    // tracking a menu keeps the dark frame off an otherwise untouched menu.
    if (!MenuIsOpen()) return;

    HBRUSH hBorder = g_hBorderBrush.load(std::memory_order_acquire);
    if (!hBorder) return;

    RECT windowRect;
    if (!GetWindowRect(hWnd, &windowRect)) return;

    int frame = GetPopupFrameWidth(hWnd, windowRect);

    RECT rc = windowRect;
    OffsetRect(&rc, -rc.left, -rc.top);
    if (rc.right <= 0 || rc.bottom <= 0) return;

    HDC hdc = hdcTarget ? hdcTarget : GetWindowDC(hWnd);
    if (!hdc) return;

    tl_inOurPaint = true;
    for (int i = 0; i < frame && rc.right > rc.left && rc.bottom > rc.top; i++) {
        FrameRect(hdc, &rc, hBorder);
        InflateRect(&rc, -1, -1);
    }
    tl_inOurPaint = false;

    if (!hdcTarget) ReleaseDC(hWnd, hdc);
}

// Undocumented: sent to a window to paint its frame into the DC in wParam.
constexpr UINT WM_NCUAHDRAWFRAME = 0x00AF;

bool IsMenuPopupWindow(HWND hWnd) {
    WCHAR cls[16];
    return GetClassNameW(hWnd, cls, ARRAYSIZE(cls)) && wcscmp(cls, L"#32768") == 0;
}

// The system reuses menu popup windows, so a subclassed popup can still be
// alive when the mod unloads - and its window procedure would then point into
// an unloaded DLL. Track them and remove each subclass at uninit.
std::unordered_set<HWND> g_subclassedPopups;
std::mutex g_subclassedPopupsMutex;

// Subclassing the popup rather than watching messages from a hook: WM_PAINT is
// posted, not sent, so a WH_CALLWNDPROCRET hook never sees it, and a popup that
// repaints that way would keep the system frame.
LRESULT CALLBACK MenuPopupSubclassProc(HWND hWnd,
                                       UINT uMsg,
                                       WPARAM wParam,
                                       LPARAM lParam,
                                       DWORD_PTR dwRefData) {
    if (uMsg == WM_NCDESTROY) {
        std::lock_guard<std::mutex> lock(g_subclassedPopupsMutex);
        g_subclassedPopups.erase(hWnd);
    }

    LRESULT result = DefSubclassProc(hWnd, uMsg, wParam, lParam);

    switch (uMsg) {
        // wParam carries the DC the popup was rendered into. A caller asking for
        // the client area alone is not asking for a frame, and drawing one anyway
        // would land it over the items - the system's own render passes both
        // flags, so this costs nothing on the path the border relies on.
        case WM_PRINT:
            if (!(lParam & PRF_NONCLIENT)) break;
            [[fallthrough]];
        case WM_NCUAHDRAWFRAME:
            PaintPopupBorder(hWnd, (HDC)wParam);
            break;

        // Painted on the window's own DC. WM_ERASEBKGND's DC is client
        // relative, so the frame goes on the window DC in every one of these.
        case WM_PAINT:
        case WM_NCPAINT:
        case WM_ERASEBKGND:
            PaintPopupBorder(hWnd, nullptr);
            break;
    }

    return result;
}

// Subclassed even when Border Color is empty, and regardless of whether a menu
// is up: the system reuses menu popup windows, so a popup passed over here is
// never revisited, and setting a border color later would silently do nothing
// for the rest of the session. PaintPopupBorder returns immediately when there
// is no border brush.
void SubclassPopup(HWND hWnd) {
    {
        std::lock_guard<std::mutex> lock(g_subclassedPopupsMutex);
        if (g_subclassedPopups.contains(hWnd)) return;
    }

    // Not subclassed under the lock: from another thread the helper gets there
    // with a SendMessage, and the subclass procedure takes the same lock on
    // WM_NCDESTROY.
    if (WindhawkUtils::SetWindowSubclassFromAnyThread(hWnd, MenuPopupSubclassProc, 0)) {
        std::lock_guard<std::mutex> lock(g_subclassedPopupsMutex);
        g_subclassedPopups.insert(hWnd);
    } else {
        Wh_Log(L"Failed to subclass menu popup %p; it keeps the system frame.", hWnd);
    }
}

bool IsComboListWindow(HWND hWnd) {
    WCHAR cls[16];
    return GetClassNameW(hWnd, cls, ARRAYSIZE(cls)) && wcscmp(cls, L"ComboLBox") == 0;
}

std::unordered_set<HWND> g_subclassedLists;
std::mutex g_subclassedListsMutex;

// The whole window procedure is bracketed rather than the paint messages alone.
// Photoshop owner-draws the items, which arrive as a WM_DRAWITEM sent to the
// combo's parent from inside the list's own painting, while the fill behind the
// whole list happens outside any WM_DRAWITEM at all. One flag around the list's
// message handling covers both - and covers nothing else on the thread, since
// only this window's messages pass through here.
LRESULT CALLBACK ComboListSubclassProc(HWND hWnd,
                                       UINT uMsg,
                                       WPARAM wParam,
                                       LPARAM lParam,
                                       DWORD_PTR dwRefData) {
    if (uMsg == WM_NCDESTROY) {
        std::lock_guard<std::mutex> lock(g_subclassedListsMutex);
        g_subclassedLists.erase(hWnd);
    }

    if (!g_themeDropdowns.load(std::memory_order_relaxed)) {
        return DefSubclassProc(hWnd, uMsg, wParam, lParam);
    }

    tl_comboListDepth++;
    LRESULT result = DefSubclassProc(hWnd, uMsg, wParam, lParam);
    tl_comboListDepth--;
    return result;
}

// Subclassed whatever the setting says, for the same reason the popups are: the
// list window is created once and reused, so a list skipped here would stay
// light for the rest of the session if the setting were turned on later.
void SubclassComboList(HWND hWnd) {
    {
        std::lock_guard<std::mutex> lock(g_subclassedListsMutex);
        if (g_subclassedLists.contains(hWnd)) return;
    }

    if (WindhawkUtils::SetWindowSubclassFromAnyThread(hWnd, ComboListSubclassProc, 0)) {
        {
            std::lock_guard<std::mutex> lock(g_subclassedListsMutex);
            g_subclassedLists.insert(hWnd);
        }
        Wh_Log(L"Subclassed dropdown list %p.", hWnd);

        // A list is usually found in the middle of the paint that revealed it,
        // by which point the system has already filled its background. Ask for
        // one more paint so the fill lands in the mod's color straight away
        // rather than on the next time the list is opened.
        InvalidateRect(hWnd, nullptr, TRUE);
    } else {
        Wh_Log(L"Failed to subclass dropdown list %p; it keeps the system colors.", hWnd);
    }
}

// Catches a menu popup or a dropdown list at creation, before it has painted.
// user32 creates both for itself, so nothing exported sees them appear - only a
// CBT hook does.
LRESULT CALLBACK CbtProcHook(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HCBT_CREATEWND) {
        HWND hWnd = (HWND)wParam;
        if (IsMenuPopupWindow(hWnd)) {
            SubclassPopup(hWnd);
        } else if (IsComboListWindow(hWnd)) {
            SubclassComboList(hWnd);
        }
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

// Threads whose CBT hook is up. Keyed globally rather than by a thread_local so
// the hook can also be installed for a thread from outside it, which is what
// Wh_ModInit does for a Photoshop that is already running.
std::unordered_set<DWORD> g_hookedThreads;
std::mutex g_hookedThreadsMutex;

LRESULT CALLBACK CallWndProcHook(int code, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK CallWndRetProcHook(int code, WPARAM wParam, LPARAM lParam);

// The message hooks are not tied to a menu being up. A dropdown list is painted
// with no menu in sight, and the list windows are created once per combo box and
// kept for the life of the process - most of them before this mod ever loaded -
// so the only thing that reliably identifies one is the painting itself.
void InstallThreadHook(DWORD tid) {
    {
        std::lock_guard<std::mutex> lock(g_hookedThreadsMutex);
        if (!g_hookedThreads.insert(tid).second) return;
    }

    HHOOK hCbt = SetWindowsHookExW(WH_CBT, CbtProcHook, nullptr, tid);
    HHOOK hCwp = SetWindowsHookExW(WH_CALLWNDPROC, CallWndProcHook, nullptr, tid);
    HHOOK hCwpRet = SetWindowsHookExW(WH_CALLWNDPROCRET, CallWndRetProcHook, nullptr, tid);
    TrackWinHook(hCbt);
    TrackWinHook(hCwp);
    TrackWinHook(hCwpRet);

    if (!hCbt || !hCwp || !hCwpRet) {
        Wh_Log(L"SetWindowsHookEx failed on thread %u (%u); menus and dropdowns "
               L"fall back to the system colors.", tid, GetLastError());
    } else {
        Wh_Log(L"Hooked thread %u.", tid);
    }
}

// Called from the color hooks, which any thread that paints reaches early. The
// mod can be enabled before Photoshop has a single window - Windhawk injects at
// process start - so this retries rather than giving up, but at most once a
// second per thread while the answer is still no.
thread_local bool tl_threadSwept = false;
thread_local ULONGLONG tl_lastSweep = 0;

void EnsureThreadHooks() {
    if (tl_threadSwept) return;

    ULONGLONG now = GetTickCount64();
    if (tl_lastSweep && now - tl_lastSweep < 1000) return;
    tl_lastSweep = now;

    // Set before the sweep, not after: subclassing can paint, and painting comes
    // back through here.
    tl_threadSwept = true;

    // Windows this thread already has, which the CBT hook by definition never
    // sees. Both kinds are cached and reused by the system, so one that existed
    // before the mod loaded would never be revisited - which is the usual case,
    // since a mod is normally first enabled in a Photoshop already in use.
    int windows = 0;
    EnumThreadWindows(GetCurrentThreadId(), [](HWND hWnd, LPARAM lParam) WINAPI -> BOOL {
        (*(int*)lParam)++;
        if (IsMenuPopupWindow(hWnd)) {
            SubclassPopup(hWnd);
        }

        // A combo's list is created as a child window and only becomes a popup
        // when it drops down, so EnumThreadWindows - which enumerates non-child
        // windows only - would never reach one that already exists.
        EnumChildWindows(hWnd, [](HWND hChild, LPARAM) WINAPI -> BOOL {
            if (IsComboListWindow(hChild)) {
                SubclassComboList(hChild);
            }
            return TRUE;
        }, 0);
        return TRUE;
    }, (LPARAM)&windows);

    if (windows == 0) {
        tl_threadSwept = false;  // not a UI thread yet - ask again later
        return;
    }

    InstallThreadHook(GetCurrentThreadId());
}

// An owner-draw callback for a menu item, and one for a row of a list. The
// closed combo field arrives as a list draw too, marked ODS_COMBOBOXEDIT -
// Photoshop paints that one in its own colors, and it already looks right.
bool IsMenuItemDraw(const DRAWITEMSTRUCT* di) {
    return di->CtlType == ODT_MENU;
}

bool IsListItemDraw(const DRAWITEMSTRUCT* di) {
    return (di->CtlType == ODT_LISTBOX || di->CtlType == ODT_COMBOBOX) &&
           !(di->itemState & ODS_COMBOBOXEDIT);
}

// Not every row's text color is asked for. Some of Photoshop's list drawing
// paints with whatever is already on the DC, and what user32 left there is the
// real COLOR_WINDOWTEXT, put in place through an internal path that no hook
// sees - which is how a row came out black on a themed background. Setting the
// colors before the owner-draw runs costs nothing where Photoshop does specify
// them, since its own call lands after this one.
void PrepareListItemDC(const DRAWITEMSTRUCT* di) {
    if (!di->hDC) return;

    bool selected = (di->itemState & ODS_SELECTED) != 0;
    SetTextColor(di->hDC, selected ? g_colHiText.load(std::memory_order_relaxed)
                                   : g_colText.load(std::memory_order_relaxed));
    SetBkColor(di->hDC, selected ? g_colHighlight.load(std::memory_order_relaxed)
                                 : g_colMenu.load(std::memory_order_relaxed));
}

// Before the window handles the message. Both counters are opened here and
// closed in the return hook, so they are set for exactly the span in which the
// item is painted - which is where every color the mod cares about is read.
LRESULT CALLBACK CallWndProcHook(int code, WPARAM wParam, LPARAM lParam) {
    if (code != HC_ACTION || !lParam) {
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }

    auto* cwp = (CWPSTRUCT*)lParam;

    if (cwp->message == WM_DRAWITEM && cwp->lParam) {
        auto* di = (const DRAWITEMSTRUCT*)cwp->lParam;

        // Menu items stay scoped to a menu this mod is tracking, so a menu it
        // deliberately leaves alone is not half-themed.
        if (IsMenuItemDraw(di) && MenuIsOpen()) {
            tl_menuDrawDepth++;
        } else if (IsListItemDraw(di) && g_themeDropdowns.load(std::memory_order_relaxed)) {
            tl_comboListDepth++;
            // hwndItem is the list window itself, which is how the mod finds a
            // list it never saw created. The subclass is for the fill behind the
            // whole list, which happens outside any WM_DRAWITEM.
            if (IsComboListWindow(di->hwndItem)) SubclassComboList(di->hwndItem);
            PrepareListItemDC(di);
        }
    } else if (cwp->message == WM_CTLCOLORLISTBOX &&
               g_themeDropdowns.load(std::memory_order_relaxed)) {
        // The other message that names the list, and it arrives earlier in the
        // paint than the first item does.
        if (IsComboListWindow((HWND)cwp->lParam)) SubclassComboList((HWND)cwp->lParam);
    }

    return CallNextHookEx(nullptr, code, wParam, lParam);
}

// And after, which is where the pairs close and where a popup that has just been
// initialised can be stamped.
LRESULT CALLBACK CallWndRetProcHook(int code, WPARAM wParam, LPARAM lParam) {
    if (code != HC_ACTION || !lParam) {
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }

    auto* ret = (CWPRETSTRUCT*)lParam;

    if (ret->message == WM_INITMENUPOPUP) {
        // Only for a menu going up through TrackPopupMenu(Ex): stamping every
        // menu in the process would darken the background of ones whose text the
        // system paints, leaving them dark on dark.
        if (MenuIsOpen()) ApplyMenuBackground((HMENU)ret->wParam);
    } else if (ret->message == WM_DRAWITEM && ret->lParam) {
        auto* di = (const DRAWITEMSTRUCT*)ret->lParam;
        if (IsMenuItemDraw(di)) {
            if (tl_menuDrawDepth > 0) tl_menuDrawDepth--;
        } else if (IsListItemDraw(di)) {
            if (tl_comboListDepth > 0) tl_comboListDepth--;
        }
    } else if (ret->message == WM_CTLCOLORLISTBOX &&
               g_themeDropdowns.load(std::memory_order_relaxed) &&
               IsComboListWindow((HWND)ret->lParam)) {
        // The same DC, prepared for whatever the list paints outside a row -
        // and done after the parent has answered, so it is the mod's colors
        // that stand.
        SetTextColor((HDC)ret->wParam, g_colText.load(std::memory_order_relaxed));
        SetBkColor((HDC)ret->wParam, g_colMenu.load(std::memory_order_relaxed));
    }

    return CallNextHookEx(nullptr, code, wParam, lParam);
}

// Nested tracking calls (submenus opened via TrackPopupMenu) share the outermost
// entry. The hooks themselves are not put up here - they belong to the thread,
// not to the menu - but this is one more chance to notice a thread that has not
// been hooked yet.
void EnterMenu(HMENU hMenu) {
    tl_menuDepth++;
    EnsureThreadHooks();
    ApplyMenuBackground(hMenu);
}

void LeaveMenu() {
    if (--tl_menuDepth > 0) return;

    tl_menuDepth = 0;

    // A menu dismissed mid-draw would otherwise leave the counter high and the
    // color overrides stuck on for this thread.
    tl_menuDrawDepth = 0;
}

// ---------------------------------------------------------------------------
// Hooks
// ---------------------------------------------------------------------------

decltype(&TrackPopupMenu) TrackPopupMenu_Original;
BOOL WINAPI TrackPopupMenu_Hook(HMENU hMenu, UINT uFlags, int x, int y,
                                int nReserved, HWND hWnd, const RECT* prcRect) {
    EnterMenu(hMenu);
    BOOL bRes = TrackPopupMenu_Original(hMenu, uFlags, x, y, nReserved, hWnd, prcRect);
    LeaveMenu();
    return bRes;
}

decltype(&TrackPopupMenuEx) TrackPopupMenuEx_Original;
BOOL WINAPI TrackPopupMenuEx_Hook(HMENU hMenu, UINT uFlags, int x, int y,
                                  HWND hWnd, LPTPMPARAMS lptpm) {
    EnterMenu(hMenu);
    BOOL bRes = TrackPopupMenuEx_Original(hMenu, uFlags, x, y, hWnd, lptpm);
    LeaveMenu();
    return bRes;
}

// The mod can be enabled before Photoshop has a single window, so the thread
// hooks cannot all go up at init. This is the earliest reliable moment: a thread
// that creates a window is a UI thread, and Photoshop creates its own windows
// through the exported call - unlike the menu popups and dropdown lists, which
// user32 creates for itself and which this hook therefore never sees.
decltype(&CreateWindowExW) CreateWindowExW_Original;
HWND WINAPI CreateWindowExW_Hook(DWORD exStyle, PCWSTR className, PCWSTR windowName,
                                 DWORD style, int x, int y, int width, int height,
                                 HWND hParent, HMENU hMenu, HINSTANCE hInstance,
                                 LPVOID lpParam) {
    HWND hWnd = CreateWindowExW_Original(exStyle, className, windowName, style,
                                         x, y, width, height, hParent, hMenu,
                                         hInstance, lpParam);
    if (hWnd) EnsureThreadHooks();
    return hWnd;
}

// Both color hooks are also where the mod notices a UI thread for the first
// time: any thread that paints gets here early, long before it opens a menu or
// drops a list down.
decltype(&GetSysColor) GetSysColor_Original;
DWORD WINAPI GetSysColor_Hook(int nIndex) {
    EnsureThreadHooks();

    COLORREF color;
    if (!tl_inOurPaint) {
        if (tl_menuDrawDepth > 0 && ColorForSysColor(nIndex, &color)) return color;
        if (tl_comboListDepth > 0 && ColorForDropdown(nIndex, &color)) return color;
    }
    return GetSysColor_Original(nIndex);
}

decltype(&GetSysColorBrush) GetSysColorBrush_Original;
HBRUSH WINAPI GetSysColorBrush_Hook(int nIndex) {
    EnsureThreadHooks();

    if (!tl_inOurPaint) {
        // Callers may cache this handle for the process lifetime, so the
        // brushes it hands out can never be deleted. See Wh_ModUninit.
        HBRUSH hBrush = nullptr;
        if (tl_menuDrawDepth > 0) {
            hBrush = BrushForSysColor(nIndex);
        } else if (tl_comboListDepth > 0) {
            hBrush = BrushForDropdown(nIndex);
        }
        if (hBrush) return hBrush;
    }
    return GetSysColorBrush_Original(nIndex);
}

decltype(&FillRect) FillRect_Original;
int WINAPI FillRect_Hook(HDC hdc, const RECT* lprc, HBRUSH hbr) {
    bool inMenu = tl_menuDrawDepth > 0;
    bool inList = tl_comboListDepth > 0;
    if ((!inMenu && !inList) || tl_inOurPaint || !lprc) {
        return FillRect_Original(hdc, lprc, hbr);
    }

    int w = lprc->right - lprc->left;
    int h = lprc->bottom - lprc->top;

    // Some of Photoshop's owner-draw code fills with a system color
    // pseudo-handle - (HBRUSH)(COLOR_MENU + 1), (HBRUSH)(COLOR_HIGHLIGHT + 1) -
    // which user32 resolves from shared memory, so neither GetSysColor nor
    // GetSysColorBrush is consulted; re-resolve them here instead. Both routes
    // are live: the Layers blend list asks for the color and makes its own
    // brush, while the options bar's list paints its rows with the pseudo-handle
    // and would otherwise stay light while the other went dark.
    ULONG_PTR sysIndex = (ULONG_PTR)hbr - 1;
    if (sysIndex <= (ULONG_PTR)COLOR_MENUBAR) {
        HBRUSH ours = inMenu ? BrushForSysColor((int)sysIndex)
                             : BrushForDropdown((int)sysIndex);
        if (ours && IsPaintContext(hdc, inMenu ? L"#32768" : L"ComboLBox")) {
            Wh_Log(L"FillRect: system color %d -> mod color, %dx%d", (int)sysIndex, w, h);
            return FillRect_Original(hdc, lprc, ours);
        }
        return FillRect_Original(hdc, lprc, hbr);
    }

    // Separator lines are a menu shape; a list draws its dividers as part of an
    // item, so the geometry guess is not extended to one.
    HBRUSH hSep = g_hSeparatorBrush.load(std::memory_order_acquire);
    if (inMenu && hSep && IsSeparatorGeometry(hdc, w, h) && IsPaintContext(hdc, L"#32768")) {
        Wh_Log(L"FillRect: separator %dx%d recolored", w, h);
        return FillRect_Original(hdc, lprc, hSep);
    }

    return FillRect_Original(hdc, lprc, hbr);
}

decltype(&PatBlt) PatBlt_Original;
BOOL WINAPI PatBlt_Hook(HDC hdc, int x, int y, int w, int h, DWORD rop) {
    if (tl_menuDrawDepth <= 0 || tl_inOurPaint || rop != PATCOPY) {
        return PatBlt_Original(hdc, x, y, w, h, rop);
    }

    HBRUSH hSep = g_hSeparatorBrush.load(std::memory_order_acquire);
    if (hSep && IsSeparatorGeometry(hdc, w, h) && IsPaintContext(hdc, L"#32768")) {
        HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hSep);
        BOOL bRes = PatBlt_Original(hdc, x, y, w, h, rop);
        SelectObject(hdc, hOldBrush);
        return bRes;
    }

    return PatBlt_Original(hdc, x, y, w, h, rop);
}

// ---------------------------------------------------------------------------
// Windhawk events
// ---------------------------------------------------------------------------

void Wh_ModSettingsChanged() {
    LoadSettings();
}

BOOL Wh_ModInit() {
    LoadSettings();

    if (!WindhawkUtils::SetFunctionHook(TrackPopupMenu, TrackPopupMenu_Hook, &TrackPopupMenu_Original)) Wh_Log(L"Failed to hook TrackPopupMenu");
    if (!WindhawkUtils::SetFunctionHook(TrackPopupMenuEx, TrackPopupMenuEx_Hook, &TrackPopupMenuEx_Original)) Wh_Log(L"Failed to hook TrackPopupMenuEx");
    if (!WindhawkUtils::SetFunctionHook(GetSysColor, GetSysColor_Hook, &GetSysColor_Original)) Wh_Log(L"Failed to hook GetSysColor");
    if (!WindhawkUtils::SetFunctionHook(GetSysColorBrush, GetSysColorBrush_Hook, &GetSysColorBrush_Original)) Wh_Log(L"Failed to hook GetSysColorBrush");
    if (!WindhawkUtils::SetFunctionHook(FillRect, FillRect_Hook, &FillRect_Original)) Wh_Log(L"Failed to hook FillRect");
    if (!WindhawkUtils::SetFunctionHook(PatBlt, PatBlt_Hook, &PatBlt_Original)) Wh_Log(L"Failed to hook PatBlt");
    if (!WindhawkUtils::SetFunctionHook(CreateWindowExW, CreateWindowExW_Hook, &CreateWindowExW_Original)) Wh_Log(L"Failed to hook CreateWindowExW");

    // When the mod is enabled in a Photoshop that is already running, the CBT
    // hook can go up immediately - and it needs to, because a dropdown list
    // created before it would never be seen. A thread hook can be installed from
    // any thread, so this works from the engine's. At process start there is
    // nothing to find yet, and EnsureThreadHooks picks it up from the first
    // paint instead.
    EnumWindows([](HWND hWnd, LPARAM) WINAPI -> BOOL {
        DWORD pid = 0;
        DWORD tid = GetWindowThreadProcessId(hWnd, &pid);
        if (tid && pid == GetCurrentProcessId()) InstallThreadHook(tid);
        return TRUE;
    }, 0);

    return TRUE;
}

void Wh_ModUninit() {
    // The message hooks belong to whichever Photoshop thread had a menu up, not
    // to this one, so they are released through the global set - see g_winHooks.
    {
        std::vector<HHOOK> hooks;
        {
            std::lock_guard<std::mutex> lock(g_winHooksMutex);
            hooks.assign(g_winHooks.begin(), g_winHooks.end());
            g_winHooks.clear();
        }
        for (HHOOK hHook : hooks) {
            UnhookWindowsHookEx(hHook);
        }
    }

    // Not WindhawkUtils::RemoveAllWindowSubclasses(), which needs Windhawk 1.8.
    {
        std::vector<HWND> popups;
        {
            std::lock_guard<std::mutex> lock(g_subclassedPopupsMutex);
            popups.assign(g_subclassedPopups.begin(), g_subclassedPopups.end());
            g_subclassedPopups.clear();
        }
        for (HWND hWnd : popups) {
            WindhawkUtils::RemoveWindowSubclassFromAnyThread(hWnd, MenuPopupSubclassProc);
        }
    }

    {
        std::vector<HWND> lists;
        {
            std::lock_guard<std::mutex> lock(g_subclassedListsMutex);
            lists.assign(g_subclassedLists.begin(), g_subclassedLists.end());
            g_subclassedLists.clear();
        }
        for (HWND hWnd : lists) {
            WindhawkUtils::RemoveWindowSubclassFromAnyThread(hWnd, ComboListSubclassProc);
        }
    }

    // Undo the background brush stamped on Photoshop's menus, so disabling the
    // mod restores the normal menus without restarting the application.
    RestoreStampedMenus();

    // The brushes are deliberately NOT deleted.
    //
    // Two paths hand these handles to Photoshop and neither can be reclaimed:
    // MENUINFO::hbrBack on menus that outlive the mod, and GetSysColorBrush
    // return values, which callers are entitled to cache for the process
    // lifetime. Deleting them means Photoshop eventually paints with a freed -
    // possibly recycled - GDI handle. A handful of brushes for the remaining
    // lifetime of the process is a trivial cost next to a crash in the host.
    {
        std::lock_guard<std::mutex> lock(g_brushMutex);
        g_retiredBrushes.clear();
    }
}
