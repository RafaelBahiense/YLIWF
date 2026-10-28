# UI preview

Run from the repository root:

```powershell
./tools/ui-preview/build.ps1
```

Builds and opens a resizable 900 × 720 window without Skyrim, SKSE or MO2.
Uses the existing Visual Studio C++ tools and repository-local vcpkg. The first
build downloads Dear ImGui; subsequent builds are incremental.

The preview compiles the production `UI.cpp` and `HotkeysPage.cpp`. Edit those
files or a participating add-on's `native/Menu.cpp` and save: the launcher
rebuilds and reloads the menu while the window stays open. Failed builds leave
the current menu running; details appear in
`build/ui-preview/hot-reload.log`. Closing the window stops the watcher.
Tabs provide the Followers, Settings, Hotkeys, Debug and registered add-on pages.
See [add-on menu tabs](../../docs/menu-tabs.md) for preview integration.
Select a scenario to exercise party, empty, quest-restricted, pending, failed and
unloaded states. Resize the window
and adjust Scale to check wrapping and spacing.

Hot reload preserves the simulated party, settings, scenario and scale. Popups,
expanded sections, the unsaved-settings baseline and hotkey capture reset; pending simulated commands are
cancelled. Changes to the window host or shared session interfaces require
closing the preview and rebuilding. Reloading applies only to this executable.
The Skyrim DLL and its loading behavior are unchanged.

Follower commands, distance, roles, Stand down and hotkey capture update sample
state. Settings stay in memory, including Save; no game files or INI are written.
Perk resolution, engine actions and persistence require in-game validation.
The preview uses SKSE Menu Framework's Skyrim theme, including its colors,
borders and spacing. To match an installation's selected theme and font, point
it at the directory containing `SKSEMenuFramework.ini`:

```powershell
./tools/ui-preview/build.ps1 -FrameworkDirectory 'C:\Mods\SKSE Menu Framework\SKSE\Plugins'
```

`-FrameworkIni` selects an overriding INI; `-Font` selects a TTF/OTF explicitly.
The launcher reads `MenuStyle`, `PrimaryFont` and `FontSizeMedium`, with the
framework's `SkyrimMenuFont.ttf` fallback. It copies the selected font into the
local build directory. Fonts are not included in this tool's source or release
archives. Without a framework directory, the bundled Skyrim theme uses ImGui's
built-in font at 32 px. Skyrim's blurred game background and input routing still
require in-game validation.

Local defaults can be stored in ignored `.tools/ui-preview.json` with
`FrameworkDirectory`, `FrameworkIni` and `Font` properties. Command-line values
override them. No installed files are changed.

```powershell
./tools/ui-preview/build.ps1 -BuildOnly
./tools/ui-preview/build.ps1 -SmokeTest
```

`-SmokeTest` renders all pages in six scenarios at two widths without opening a
window. It also loads a replacement UI module, checks session restoration and
rejects a missing replacement while retaining the previous module. It does not
verify visual quality or gameplay. The executable is
`build/ui-preview/yliwf-ui-preview-host.exe`.

`include/SKSEMCP/SKSEMenuFramework.hpp` is a preview-only backend: it forwards
drawing to Dear ImGui and hosts the production menu registrations. It is never
included by the Skyrim DLL. `Simulation.cpp` supplies in-memory implementations
of the services called by the menu.

The host owns the window and DirectX device. `yliwf-preview-ui.dll` owns ImGui,
menu callbacks and simulation services. Successful builds publish `ui-ready`;
the host loads a unique copy between frames, transfers session data and destroys
old ImGui resources before unloading. Host and UI use the same dynamic C++ runtime
for the shared session types. These interfaces are local development contracts,
not an add-on API. Theme JSON edits are watched; changes to INI/font selection
require rerunning the launcher.

The bundled `skyrimDefault.json` and theme baseline originate from
[SKSE Menu Framework 3](https://github.com/QTR-Modding/SKSE-Menu-Framework-3),
under GPL-3.0, the same license as this repository. The theme loader was checked
against upstream commit `dc9182454556910b2fd873d770c345cfe6e53777`.
