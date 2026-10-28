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

The framework moves controller work out of the Papyrus VM and keeps behavior
and plugin records in reviewable source with reproducible builds.
See [architecture](docs/architecture.md) for the design.

## Gameplay features

- Up to eight vanilla-system humanoid followers, alongside a separate vanilla
  animal companion; fixed, perk-based or Speech-based humanoid limits.
- Individual Follow, Wait and Dismiss through dialogue or the SKSE menu, plus
  Follow All, Wait All and Dismiss All in the menu.
- Configurable keyboard shortcuts, including Shift/Ctrl/Alt combinations, for
  party Follow/Wait, Stand down, following distance and travel formation.
- Waiting core followers dismiss themselves after 72 in-game hours.
- Party-wide Stand down: keep following and avoid nearby enemies without fighting.
  Healers may heal themselves and other followers, excluding the player.
  Quest restrictions retain priority.
- Primary-slot selection without dismissal, for integrations such as Blades recruitment.
- Follower homes and idle sandboxing.
- Close, Normal or Far following distance, globally or per follower.
- [Travel formations](docs/formations.md): Column follows your route, Line places followers
  beside you, and Wedge forms behind you with the player at its tip. Slots adjust to
  nearby navigable ground; ordinary following resumes in combat, interiors and settlements.
- Essential status, friendly-fire protection and crossfire protection.
- Native add-ons for quest-managed followers, respecting their controllers.
- [Combat roles](docs/combat-roles.md) (preview): choose each follower's role in
  the party, with spell and equipment requirements.

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

Install optional add-on ZIPs alongside a compatible core. See each add-on's
README for requirements and installation.

Browse [the add-on directory](src/addons/) for available modules.

## Configuration

Use Settings or edit `Data/SKSE/Plugins/YouLeadIWillFollow.ini`.
Assign shortcuts in **Hotkeys**. All start unbound and work only
during gameplay. Dismiss All remains menu-only.
Gameplay settings are stored with each game save in its SKSE co-save. The INI
supplies defaults for new games and saves without stored settings. Invalid INI
values fall back to [built-in defaults](assets/settings.ini).
Hotkeys and diagnostic preferences remain global; use **Save global settings**
to persist them. **Save current settings as INI defaults** also updates gameplay
defaults for future games. **Use INI defaults** applies those defaults to the
current game.

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
[add-on development](docs/addons/README.md) and [development tools](tools/README.md).

## License

[GPL-3.0](LICENSE), except separately licensed components. See [NOTICE](NOTICE)
and [CREDITS.md](CREDITS.md). No warranty.

Third-party components retain their licenses. Bethesda game files and unmodified
compiler SDK inputs are excluded. Distribute each binary with its matching
source archive; see [release instructions](docs/build-release.md#publishing-a-release).
