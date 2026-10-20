# Serana add-on

Makes Serana available to YLIWF features when her Dawnguard quests allow them.

## Features

| Feature | Support |
| --- | --- |
| Follow, Wait and Dismiss | Individual and party commands, including hotkeys |
| Following distance | Global and individual Close, Normal and Far settings |
| Travel formations | Column, Line and Wedge during ordinary travel |
| Stand down | Following and hostile avoidance without fighting |
| Friendly-fire and crossfire protection | Supported while eligible |
| Receive healing | From core healers, using spells that can affect her |
| Location, distance and state inspection | Supported |
| Recruitment alongside other followers | Through her own dialogue, respecting quest conditions |
| Combat-role assignment | Unsupported |
| Essential-status setting | Dawnguard retains control |
| YLIWF homes and idle sandboxing | Unsupported; her controller retains its behavior |
| Automatic dismissal after 72 hours waiting | Unsupported |
| Primary-slot promotion | Unsupported |
| Core recruitment limit | Does not consume a core slot |
| Controller repair | Diagnostics only; no automatic repair |

## Installation

Install `YouLeadIWillFollow-Serana-<version>.zip` alongside a compatible
[YLIWF core](../../../README.md). Recruit Serana through her own dialogue;
other recruited followers do not block recruitment when her quest permits it.

Dawnguard retains her recruitment, aliases, saved state and quest behavior.
Controller mismatches disable commands and appear in diagnostics.

[Developer documentation](docs/development.md) covers controller and movement integration.
