# Photoshop Dark Menus

This Windhawk mod enables dark menus (top-bar dropdowns and context menus) in Adobe Photoshop on Windows 11, along with custom color settings.

This mod dynamically updates the active Windows session palette and intercepts GDI line drawing calls to force dark menu styling without making permanent modifications to registry keys on disk.

## Screenshots

![Top Bar Menu Dropdown](https://raw.githubusercontent.com/sabergraphics/photoshop-dark-menus/main/images/photoshop-dark-menu-screenshot-1.png)

![Context Menu](https://raw.githubusercontent.com/sabergraphics/photoshop-dark-menus/main/images/photoshop-dark-menu-screenshot-2.png)

## Why a Standalone Mod?

While the [Dark mode context menus](https://github.com/MGGSK/DarkMenus) (`dark-menus`) Windhawk mod provides system-wide dark Win32 menus, it does not function correctly for Photoshop due to how Adobe implements its legacy UI. This standalone mod was created out of necessity to address that gap.

Photoshop draws its menu separators using legacy Win32 GDI functions (`PatBlt` and `FillRect`). To reliably color or hide these specific separators, this mod relies on targeted pixel-dimension heuristics. Merging these highly specific GDI hooks into a global, system-wide mod (`@include *`) would risk causing visual glitches and false-positive artifacts in other applications.

## Options
The following settings can be customized in the Windhawk mod panel:
- **Menu Background Color**: Background color for all menu popups (Default: `#282828`).
- **Menu Text Color**: Text color for active items (Default: `#DCDCDC`).
- **Highlight Background Color**: Color when hovering over an item (Default: `#505050`).
- **Separator Line Color**: Color for separator lines. Set to match the background color to hide them completely (Default: `#383838`).
- **Disabled Text Color**: Text color for disabled menu items (Default: `#808080`).