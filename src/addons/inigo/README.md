# Inigo add-on

Connects Inigo's follower controller to YLIWF without replacing his scripts or plugin.

## Requirements

- [YLIWF core](../../../README.md) with follower adapter API v1.
- INIGO SE 2.4C (`Inigo.esp`).

Install `YouLeadIWillFollow-Inigo-<version>.zip` alongside both mods. Recruit Inigo
through his own dialogue. He appears in YLIWF's follower menu and party commands
without consuming a core recruitment slot.

## Features

| Feature | Support |
| --- | --- |
| Follow, Wait and Dismiss | Individual and party commands, including hotkeys |
| Following distance | Same Close, Normal and Far presets as core followers during ordinary on-foot following |
| Travel formations | Column, Line and Wedge during eligible on-foot travel |
| Relaxing | Reported as Waiting; Follow resumes travel |
| Friendly-fire and crossfire protection | While eligible |
| Location, distance and state inspection | Supported |
| Recruitment, quests, combat style and horse behavior | Controlled by Inigo |
| Stand down | Passive following or waiting with combat suppression while eligible |
| Combat roles, homes and essential-status changes | Unsupported |
| Automatic dismissal and primary-slot promotion | Unsupported |

Commands pause during scenes, quest restrictions, incapacitation and unrecognized
packages. Inigo's own dialogue remains available under its authored conditions.

Distance and formation overrides release for combat, scenes, waiting, quest
movement, horse use or another mod's interrupt package. Horse movement retains
Inigo's native near/far preference. Changes to distance never resume waiting.

Stand down takes priority over travel spacing and formations, including during
combat. Returning to normal behavior restores eligible travel preferences.
Quest restrictions, scenes, horse use and foreign interrupts retain priority.

[Developer notes](docs/development.md) describe state mapping and validation.
