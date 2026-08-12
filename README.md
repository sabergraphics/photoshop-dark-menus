# Photoshop Dark Menus

This Windhawk mod enables dark menus (top-bar dropdowns and context menus) in Adobe Photoshop on Windows, with fully customizable colors.

Everything happens inside the Photoshop process. No system colors are changed, no other application is affected, and nothing has to be restored when Photoshop exits or crashes.

## Screenshots

![Top Bar Menu Dropdown](https://raw.githubusercontent.com/sabergraphics/photoshop-dark-menus/main/images/photoshop-dark-menu-screenshot-1.png)

![Context Menu](https://raw.githubusercontent.com/sabergraphics/photoshop-dark-menus/main/images/photoshop-dark-menu-screenshot-2.png)

## How it works

Photoshop's menus look like classic unthemed Win32 menus, but they are not. The process is themed (`IsAppThemed() == 1`), and Photoshop **owner-draws every menu item itself** through `AdobeOwl.dll`, using plain GDI, inside its own process. Instrumenting a running Photoshop shows the popups going up via `TrackPopupMenuEx`, and each item being painted with:

- `FillRect(hdc, &rc, (HBRUSH)(COLOR_MENU + 1))` for the item background, and `(HBRUSH)(COLOR_HIGHLIGHT + 1)` for the hovered item. These **system color pseudo-handles** are resolved inside `user32` from shared memory, so they never reach `GetSysColor` or `GetSysColorBrush` — which is why hooking those alone appears to do nothing.
- `GetSysColor(COLOR_MENUTEXT / COLOR_GRAYTEXT / COLOR_HIGHLIGHTTEXT)` for item, disabled and selected text.
- Photoshop's own 1px `FillRect` for separator lines.

So every pixel of a menu item is reachable from inside the process:

| Element | How Photoshop draws it | How the mod reaches it |
| --- | --- | --- |
| Item background | `FillRect` with a `COLOR_MENU` pseudo-handle | `FillRect` hook re-resolves the pseudo-handle through the mod's colors |
| Hover highlight | `FillRect` with a `COLOR_HIGHLIGHT` pseudo-handle | same |
| Item / disabled / selected text | `GetSysColor` | `GetSysColor` hook, scoped to open menus |
| Separators | own 1px `FillRect` | `FillRect` / `PatBlt` hook, scoped to menu DCs |
| Popup background around items | system, from `COLOR_MENU` | `MENUINFO::hbrBack` at `WM_INITMENUPOPUP` |
| Popup frame | system, kernel-side, from the 3D colors | repainted from a subclass on the popup |

The system color overrides apply **only while a menu is open on the thread that opened it**, so Photoshop's dialogs, panels and lists keep the normal system colors. The popup frame is the one part drawn kernel-side, where no user-mode hook can reach it, so the popup window is subclassed and its frame repainted after the system has drawn it.

## Why a standalone mod?

The system-wide [Dark mode context menus](https://github.com/MGGSK/DarkMenus) (`dark-menus`) mod switches a process to uxtheme's dark menu theme with `SetPreferredAppMode` + `FlushMenuThemes`. That cannot help Photoshop: uxtheme only themes menus it draws, and Photoshop draws its menu items itself, so there is nothing for the dark menu theme to attach to. This mod intercepts Photoshop's own drawing instead, and adds:

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

## Scope

The menu bar itself (File, Edit, Image, ...) is drawn by Photoshop's own UI framework and already follows Photoshop's interface theme; this mod covers the dropdown popups and context menus.

Menus that Windows pops up through its own internal paths, without going through the exported `TrackPopupMenu` / `TrackPopupMenuEx` — a window's system menu, for example — are outside the reach of an in-process hook and keep the system colors.
