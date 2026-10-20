# Fura FDE compatibility patch

Makes the original Fura Bloodmouth a normal YLIWF follower while preserving
**Follower Dialogue Expansion – Fura Bloodmouth** (`FDE Fura.esp`) dialogue,
quests and special movement.

## Features

| Feature | Support |
| --- | --- |
| Recruitment and dismissal | FDE dialogue and YLIWF controls |
| Follow and Wait | FDE dialogue, YLIWF UI, party commands and hotkeys |
| Follower dialogue | FDE's voiced choices for Follow, Wait, Dismiss, Trade and order topics |
| Following distance | Core Close, Normal and Far settings |
| Travel formations and stand down | Core behavior; FDE quest restrictions take priority |
| Combat roles | Core role selection and eligibility |
| Essential status, protection, homes and waiting timeout | Core settings, subject to quest restrictions |
| Core slots and recruitment limit | Uses one normal slot and respects the configured cap |
| FDE quests and scenes | Preserve dialogue, scripted travel and progression |

## Installation

Requires **Follower Dialogue Expansion – Fura Bloodmouth** (`FDE Fura.esp`)
and a compatible **YLIWF core**.

Install `YouLeadIWillFollow-FuraFDE-<version>.zip` alongside those mods. In MO2,
place the add-on below FDE Fura and allow its scripts to overwrite FDE's scripts.
Keep `YouLeadIWillFollow.FuraFDE.dll` and the add-on's scripts installed together.

If Fura is following you through FDE alone, dismiss her before installing the
add-on, then recruit her again afterward. Fura already registered with YLIWF
keeps her existing slot. This add-on supports the original Fura, not cloned copies.

## Quest behavior

FDE's scenes and scripted travel take priority over follower commands. During
these sequences, YLIWF may temporarily disable recruitment, Follow, Wait,
Dismiss, homes, formations, stand down and combat roles. The Followers page
shows the restriction, and party commands skip her while she is unavailable.
Continue her dialogue or quest to restore normal follower controls.

Accepting Bloodmouth's vengeance task dismisses Fura automatically. Recruit her
again when the quest allows her to follow.

[Developer documentation](docs/development.md) covers implementation and testing.
