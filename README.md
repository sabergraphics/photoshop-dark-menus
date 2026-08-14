# Photoshop Dark Menus

This Windhawk mod enables dark menus (top-bar dropdowns and context menus) and dark dropdown lists in Adobe Photoshop on Windows, with fully customizable colors.

Everything happens inside the Photoshop process. No system colors are changed and no other application is affected.

## Screenshots

![Top Bar Menu Dropdown](https://raw.githubusercontent.com/sabergraphics/photoshop-dark-menus/main/images/photoshop-dark-menu-screenshot-1.png)

![Context Menu](https://raw.githubusercontent.com/sabergraphics/photoshop-dark-menus/main/images/photoshop-dark-menu-screenshot-2.png)

## How it works

Photoshop's menus look like classic unthemed Win32 menus, but they are not. The process is themed (`IsAppThemed() == 1`), and Photoshop **owner-draws every menu item itself** through `AdobeOwl.dll`, using plain GDI, inside its own process. Instrumenting a running Photoshop shows the popups going up via `TrackPopupMenuEx`, and each item being painted with:

- `FillRect(hdc, &rc, (HBRUSH)(COLOR_MENU + 1))` for the item background, and `(HBRUSH)(COLOR_HIGHLIGHT + 1)` for the hovered item. These **system color pseudo-handles** are resolved inside `user32` from shared memory, so they never reach `GetSysColor` or `GetSysColorBrush`, which is why hooking those alone appears to do nothing.
- `GetSysColor(COLOR_MENUTEXT / COLOR_GRAYTEXT / COLOR_HIGHLIGHTTEXT)` for item, disabled and selected text.
- Photoshop's own 1px `FillRect` for separator lines.

So every pixel of a menu item is reachable from inside the process:

| Element | How Photoshop draws it | How the mod reaches it |
| --- | --- | --- |
| Item background | `FillRect` with a `COLOR_MENU` pseudo-handle | `FillRect` hook re-resolves the pseudo-handle through the mod's colors |
| Hover highlight | `FillRect` with a `COLOR_HIGHLIGHT` pseudo-handle | same |
| Item / disabled / selected text | `GetSysColor` | `GetSysColor` hook, scoped to the item being painted |
| Separators | own 1px `FillRect` | `FillRect` / `PatBlt` hook, scoped to menu DCs |
| Popup background around items | system, from `COLOR_MENU` | `MENUINFO::hbrBack` at `WM_INITMENUPOPUP` |
| Popup frame | system, kernel-side, from the 3D colors | repainted from a subclass on the popup |
| Dropdown list background | `GetSysColorBrush(COLOR_WINDOW)` | `GetSysColorBrush` hook, scoped to the list's painting |
| Dropdown row background | `GetSysColor(COLOR_WINDOW)`, or a `COLOR_HIGHLIGHT` pseudo-handle | `GetSysColor` and `FillRect` hooks, same scope |
| Dropdown row text | `GetSysColor(COLOR_WINDOWTEXT)` in one list, `COLOR_MENUTEXT` in another | `GetSysColor` hook, same scope |

The system color overrides apply **only while a menu item or a dropdown row is actually being painted**, inside a menu's `WM_DRAWITEM` or inside the list window's own message handling, rather than for as long as a menu happens to be open. That matters because `TrackPopupMenu` runs a modal message loop which keeps dispatching to every other window on the thread, so a panel repainting behind an open menu would otherwise come out in menu colors. The popup frame is the one part drawn kernel-side, where no user-mode hook can reach it, so the popup window is subclassed and its frame repainted after the system has drawn it.

The dropdown lists are stock Win32 combo boxes: Photoshop owner-draws the rows but reads their colors from the system, and the list itself is user32's `ComboLBox` window. It does not read them consistently, so both routes are covered. One list asks for a color and fills with a brush it makes; another fills with a `(HBRUSH)(COLOR_X + 1)` pseudo-handle. One takes its row text from `COLOR_WINDOWTEXT`, another from `COLOR_MENUTEXT`.

Those list windows are created once per combo box and kept for the life of the process, usually long before the mod loads, so catching them at creation is not enough. They are found instead from the messages sent while they paint: `WM_DRAWITEM` names the list in `hwndItem`, and `WM_CTLCOLORLISTBOX` names it in `lParam`. Menu popups are caught with a CBT hook at creation, with a sweep for any that already existed.

## Why a standalone mod?

The system-wide [Dark mode context menus](https://github.com/MGGSK/DarkMenus) (`dark-menus`) mod switches a process to uxtheme's dark menu theme with `SetPreferredAppMode` + `FlushMenuThemes`. That cannot help with Photoshop's own menus: uxtheme only themes menus it draws, and Photoshop draws its menu items itself, so there is nothing for the dark menu theme to attach to.

The reverse is also true. A few menus in Photoshop are put up by Windows rather than drawn by Photoshop, such as the context menu on a panel tab. Those are painted by the theming engine, below the level any in-process hook here can reach, and `dark-menus` darkens exactly those. **The two mods are complementary and worth running together.**

This mod intercepts Photoshop's own drawing instead, and adds:

1. Fully customizable colors (background, text, highlight, highlight text, separator, disabled text, border).
2. Photoshop-scoped GDI hooks. Re-resolving system color pseudo-handles and recoloring 1px fills is safe when scoped to one application's menus; it would risk false positives across every app in a global `@include *` mod.

## Options

The following settings can be customized in the Windhawk mod panel:

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

The menu bar itself (File, Edit, Image, ...) is drawn by Photoshop's own UI framework and already follows Photoshop's interface theme; this mod covers the dropdown popups and context menus.

Menus that Windows pops up through its own internal paths, without going through the exported `TrackPopupMenu` / `TrackPopupMenuEx` (a window's system menu, for example), are outside the reach of an in-process hook and keep the system colors.

The font browser and the Glyphs panel's character cells also stay light. Photoshop paints those from its own palette rather than from any system color, with its dark chrome and their light content coming from the same calls in the same window, so there is nothing for an in-process hook to answer. Recoloring them would mean matching color values instead, in a window class Photoshop also uses for swatches and color pickers, where a light fill is the content rather than a background.
