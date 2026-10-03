# You Lead, I Will Follow

**THIS MOD IS NOT MADE, GUARANTEED OR SUPPORTED BY ZENIMAX OR ITS AFFILIATES.**

**You Lead, I Will Follow (YLIWF)** is a Skyrim Special Edition follower framework
and a substantial architectural rewrite derived from **Simple Follower Framework
(SFF) by Ivy / ItzIvy05**. It retains a largely similar gameplay feature set, with
its own native controller, persistence, diagnostics, and source-based build system.

See [CREDITS.md](CREDITS.md) for the original project and contributors, and
[NOTICE](NOTICE) for the dated fork origin.

## Why this fork exists

I wanted to experiment again with Skyrim modding and build on the original
author's work, which I enjoyed using. My goal was a repository that builds from
source with minimal setup and keeps configuration and behavior easy to review
and version-control.

A save of mine broke during SFF's architectural changes. Investigating it led
to this fork's debugging and save-repair tools.

I also wanted to explore a follower framework centered on native code, reducing
the work handled by the Papyrus VM. C++ now owns follower policy, command
execution, timers, and controller persistence; Papyrus remains for compatibility
and engine calls that still need it. This provides a place to experiment with
that boundary and diagnose delayed or failed operations.

Understanding the framework and establishing a maintainable foundation come
first; new gameplay features are a future goal. The
[architecture document](docs/architecture.md) explains the technical changes
and their rationale.

## Gameplay features

- Recruit up to eight followers using Skyrim's vanilla follower system.
- Give individual Follow, Wait, and Dismiss commands through in-game dialogue.
- Choose the primary follower through the SKSE menu without dismissing anyone,
  for quests and integrations that use the vanilla primary slot, such as Blades
  recruitment.
- Set a fixed party limit or unlock slots through perks or Speech levels.
- Assign follower homes and enable idle sandboxing.
- Configure essential status, friendly-fire protection, and crossfire protection.

## Diagnostics and repair

The SKSE menu exposes follower registrations, debug commands, flow logs,
context dumps, and targeted state repairs. The **Followers** tab provides primary
selection alongside state inspection and test controls. Most follower management
uses in-game dialogue or compatible control mods. See
[debugging.md](docs/debugging.md) for instructions.

## Compatibility

YLIWF extends the vanilla `DialogueFollower` quest. Followers with their own
frameworks, such as Inigo or Lucien, should remain under those systems. Another
framework managing the same quest requires a compatibility patch.

Use a new game or a save that already uses YLIWF. Save migration from SFF is not supported.
Do not enable these frameworks together.

## Requirements

- Skyrim Special Edition, including its bundled Dawnguard, Hearthfire, and
  Dragonborn DLCs.
- [Skyrim Script Extender (SKSE64)](https://skse.silverlock.org/) matching your
  installed Skyrim runtime.
- [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444),
  using the Special Edition or Anniversary Edition download appropriate for your
  runtime.
- [SKSE Menu Framework **3.9 or newer**](https://www.nexusmods.com/skyrimspecialedition/mods/120352)
  for the in-game settings and diagnostic pages. Follower gameplay works without
  this menu; settings can also be edited in the INI.

## Installation

Install the release ZIP through your mod manager and enable
`You Lead, I Will Follow.esp`.

Install the DLL, ESP, and Papyrus scripts together. The mod creates
`Data/SKSE/Plugins/YouLeadIWillFollow.ini` on first launch if it is missing;
release ZIPs omit it so extracting an update does not overwrite your settings.

## Configuration

Edit the INI or use the menu's Settings page. Defaults are in
[assets/settings.ini](assets/settings.ini).
Missing or invalid values use defaults without rewriting your file. The menu's
Save button writes your chosen settings. With MO2, generated files normally go
to Overwrite; keep the INI in a separate settings mod to preserve it across
reinstallations and mod-folder replacement.

| Setting | Purpose |
| --- | --- |
| `iMaxFollowers` | Fixed party cap, including the primary follower, from 1 to 8 |
| `bFollowerOptionSelector` | `0`: fixed cap; `1`: perks; `2`: Speech levels |
| `sPerkForms` | Comma-separated `PluginName\|LocalFormID` entries for perk slots |
| `iSpeechLevelsPerSlot` | Speech levels per slot in Speech mode |
| `bFollowerEssential` | Essential protection |
| `bFriendlyFireProtection` / `bFollowerCrossfireProtection` | Combat protection |
| `bFollowerSandbox` / `bFollowerHomes` | Sandboxing and home assignments |

Debug options and flow logging are separate settings, both off by default.

## Development

Run `.\setup.ps1`, then `.\build.ps1` from PowerShell.
Run `.\build.ps1 -Target Test` for native and tooling checks.

- [Build guide](docs/build-release.md): prerequisites, targets, packaging, and releases.
- [Architecture](docs/architecture.md): source layout and design decisions.
- [Native controller](docs/native-controller.md): execution and persistence contracts.
- [Development tools](tools/README.md): plugin inspection and C# analysis.

## License

The maintained mod source, Papyrus scripts, plugin record definitions, build
tools, tests, configuration, and documentation are licensed under **GNU GPL
version 3**, except where a separate license is explicitly identified. See
[LICENSE](LICENSE) and [NOTICE](NOTICE). The work comes without warranty.

Third-party components retain their own licenses and notices, collected into
release archives. Bethesda game files and unmodified compiler SDK inputs are
not covered by this declaration. Publish each binary with its matching source
archive; see [publishing a release](docs/build-release.md#publishing-a-release).
