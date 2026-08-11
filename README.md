# Photoshop Dark Menus

This Windhawk mod enables dark menus (top-bar dropdowns and context menus) in Adobe Photoshop on Windows 11, along with custom color settings.

## Screenshots

![Top Bar Menu Dropdown](https://raw.githubusercontent.com/sabergraphics/photoshop-dark-menus/main/images/photoshop-dark-menu-screenshot-1.png)

![Context Menu](https://raw.githubusercontent.com/sabergraphics/photoshop-dark-menus/main/images/photoshop-dark-menu-screenshot-2.png)

## How it works

The mod has two theming engines, selectable in the settings.

### Global system colors (default)

Photoshop renders its legacy Win32 menus through the classic (unthemed) path, which reads the shared system color table directly, so per-process dark-mode techniques (`SetPreferredAppMode`, `GetSysColor` hooks) do not reach it. This engine updates the active Windows session palette with `SetSysColors`, which is the only approach found to reliably darken Photoshop's menus. No permanent modifications are made to registry keys on disk.

**Warning:** while this mode is active, the menu/selection/3D colors change for *every* application on the desktop, not just Photoshop, until Photoshop exits. Safeguards used in this mode:

- Original colors are backed up from `HKCU\Control Panel\Colors` (not from the possibly-already-modified live palette), so restarts and multiple instances cannot corrupt the backup.
- Colors are restored from an `ExitProcess` teardown hook. If another Photoshop instance is still running, restoration is deferred to the last instance to exit, so instances no longer fight over the palette.
- If Photoshop **hard-crashes** (faulty plugin, Force Quit via Task Manager), the teardown hook cannot run and the desktop may remain dark. Launching and cleanly closing Photoshop once restores the original colors.

### Process-local (experimental)

Everything stays inside the Photoshop process; no other application is affected:

- Switches the process to the dark menu theme via `uxtheme` (`SetPreferredAppMode` + `FlushMenuThemes`).
- Hooks `GetSysColor` / `GetSysColorBrush` in-process, so legacy menu drawing code picks up the custom colors without touching the system palette.
- Sets a custom background brush on Photoshop's menus (`MENUINFO::hbrBack`, applied to submenus and to context menus via `TrackPopupMenu(Ex)` hooks).
- Hooks `PatBlt` / `FillRect`, strictly scoped to active menu windows (class `#32768` or modal `GUI_INMENUMODE`), to recolor the separator lines that Photoshop draws with legacy GDI calls.

In testing, this mode does **not** darken Photoshop's menu backgrounds and text, because Photoshop's classic menu rendering bypasses the hooked user-mode color APIs. It is kept as an opt-in for experimentation and for setups where it may behave differently.

## Why a Standalone Mod?

The system-wide [Dark mode context menus](https://github.com/MGGSK/DarkMenus) (`dark-menus`) Windhawk mod darkens Win32 menus globally via per-process theming, but that approach does not reach Photoshop's classic menu rendering (see above, the same technique is implemented here as the experimental mode and has no effect on Photoshop). This mod additionally offers:

1. Fully customizable menu colors (background, text, highlight, separator, disabled text).
2. Photoshop-scoped GDI separator hooks. Photoshop draws its menu separators using legacy Win32 GDI functions (`PatBlt` and `FillRect`); this mod strictly targets device contexts belonging to active menu windows. Merging such application-tailored hooks into a global `@include *` mod would risk visual glitches and false-positive artifacts in other applications.
3. A global-palette engine with strict safeguards (registry-backed color backups, last-instance-to-exit restoration, `ExitProcess` teardown hooks) for the rendering path that per-process theming cannot reach.

## Options

The following settings can be customized in the Windhawk mod panel:

- **Theming Mode**: Global system colors (default; affects the whole desktop while Photoshop runs) or process-local (experimental; only affects Photoshop but does not darken menus on most setups).
- **Menu Background Color**: Background color for all menu popups (Default: `#282828`).
- **Menu Text Color**: Text color for active items (Default: `#DCDCDC`).
- **Highlight Background Color**: Color when hovering over an item (Default: `#505050`).
- **Highlight Text Color**: Text color when hovering over an item (Default: `#FFFFFF`).
- **Separator Line Color**: Color for separator lines. Set to match the background color to hide them completely (Default: `#383838`).
- **Disabled Text Color**: Text color for disabled menu items (Default: `#808080`).
