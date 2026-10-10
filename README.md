# You Lead, I Will Follow

**THIS MOD IS NOT MADE, GUARANTEED OR SUPPORTED BY ZENIMAX OR ITS AFFILIATES.**

**You Lead, I Will Follow (YLIWF)** is a Skyrim SE follower framework derived
from **Simple Follower Framework by Ivy / ItzIvy05**. It keeps much of SFF's
gameplay, with a rewritten native controller, persistence, diagnostics and build
system. See [credits](CREDITS.md) and [fork origin](NOTICE).

## Why this fork exists

I enjoyed SFF and wanted to learn Skyrim modding by building on it. After an
architectural change broke my save, I started investigating its state and
developing debugging and repair tools.

This fork explores a framework centered on native code, with less work in the
Papyrus VM. It also keeps behavior and plugin records in reviewable source and
aims to make builds easy to reproduce. Its focus is a maintainable foundation
for studying Skyrim modding and developing follower features.
See [architecture](docs/architecture.md) for the design.

## Gameplay features

- Up to eight vanilla-system followers; fixed, perk-based or Speech-based limits.
- Follow, Wait and Dismiss through dialogue.
- Primary-slot selection without dismissal, for integrations such as Blades recruitment.
- Follower homes and idle sandboxing.
- Close, Normal or Far following distance, globally or per follower.
- Essential status, friendly-fire protection and crossfire protection.
- Native add-ons for quest-managed followers, respecting their controllers.

The SKSE menu also provides state inspection, logs and targeted repairs.
See [debugging](docs/debugging.md).

## Requirements

- Skyrim Special Edition with Dawnguard, Hearthfire and Dragonborn.
- [SKSE64](https://skse.silverlock.org/) matching your runtime.
- [Address Library](https://www.nexusmods.com/skyrimspecialedition/mods/32444),
  using the SE or AE download for your runtime.
- [SKSE Menu Framework 3.9+](https://www.nexusmods.com/skyrimspecialedition/mods/120352)
  for settings and diagnostics. Gameplay works without it.

## Installation

Install the release ZIP through your mod manager and enable
`You Lead, I Will Follow.esp`. Keep its DLL, ESP and scripts together.

Use a new game or an existing YLIWF save; SFF migration is unsupported.
Do not enable SFF alongside YLIWF. Other mods managing `DialogueFollower`
need a compatibility patch. Custom followers remain under their own systems
unless a dedicated add-on supports them.

## Add-ons

Install optional add-on ZIPs alongside the matching core release.

- **[Serana](docs/follower-adapters.md#serana-example)** (`-Serana.zip`):
  Makes Serana available to YLIWF's follower controls and combat protection when her Dawnguard quests allow them.

## Configuration

Use Settings or edit `Data/SKSE/Plugins/YouLeadIWillFollow.ini`.
The mod creates a missing INI; releases omit it to preserve your settings.
Invalid values fall back to [defaults](assets/settings.ini).
Click **Save** to persist menu settings. In MO2, keep the generated INI in a
separate settings mod.

| Setting | Purpose |
| --- | --- |
| `iMaxFollowers` | Party cap, 1–8 |
| `bFollowerOptionSelector` | `0`: fixed; `1`: perks; `2`: Speech |
| `sPerkForms` | Comma-separated `PluginName\|LocalFormID` perk entries |
| `iSpeechLevelsPerSlot` | Speech levels per slot |
| `iFollowDistance` | `0`: Close; `1`: Normal; `2`: Far |
| `bFollowerEssential` | Essential protection |
| `bFriendlyFireProtection` / `bFollowerCrossfireProtection` | Combat protection |
| `bFollowerSandbox` / `bFollowerHomes` | Sandboxing and homes |

Debug options and flow logging are separate, off by default.

## Development

Start with the [build guide](docs/build-release.md).
See [architecture](docs/architecture.md),
[controller contracts](docs/native-controller.md),
[add-on API](docs/follower-adapters.md) and [development tools](tools/README.md).

## License

[GPL-3.0](LICENSE), except separately licensed components. See [NOTICE](NOTICE)
and [CREDITS.md](CREDITS.md). No warranty.

Third-party components retain their licenses. Bethesda game files and unmodified
compiler SDK inputs are excluded. Distribute each binary with its matching
source archive; see [release instructions](docs/build-release.md#publishing-a-release).
