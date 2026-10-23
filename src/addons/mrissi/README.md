# M'rissi add-on

Connects M'rissi's follower controller to YLIWF while retaining her recruitment,
story, dialogue, outfits and mount behavior.

## Requirements and installation

- [YLIWF core](../../../README.md) with follower adapter API v1.
- M'rissi's Tails of Troubles SE 1.7 (`MrissiTailOfTroubles.esp`).

Install `YouLeadIWillFollow-Mrissi-<version>.zip` alongside both mods. Recruit
M'rissi through her own dialogue. She appears in YLIWF without consuming a core
follower slot. No original plugin or scripts are replaced.

## Features

| Feature | Support |
| --- | --- |
| Follow, Wait and Dismiss | Individual and party commands, including hotkeys |
| Following distance | Close, Normal and Far during ordinary on-foot following |
| Travel formations | Column, Line and Wedge during eligible travel |
| Stand down | Passive following or waiting with combat suppression |
| Friendly-fire and crossfire protection | While her controller permits it |
| Combat roles, homes and essential-status changes | Unsupported |
| Sahara and other companions | Controlled by their original mods |

Quest restrictions, scenes, special broken-state behavior and mounting transitions
retain priority. Restricted locations prevent Wait and Dismiss. Resume following
before dismissing a waiting M'rissi, matching her own dialogue.

Distance and formation preferences yield to combat, waiting, sandboxing, mounting
and foreign package overrides. Stand down can operate during combat and ordinary
waiting; it yields to story and movement restrictions. Changing a distance never
resumes a waiting follower.

See [developer notes](docs/development.md) for controller mapping and validation.
