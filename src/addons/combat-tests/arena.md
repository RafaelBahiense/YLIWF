# Combat test arena

An isolated Nordic interior with an open combat area, a wall partition for
line-of-sight tests, and persistent starting markers. The enclosed floor is
4096 units square with a 640-unit ceiling. Navigation is inset from the perimeter
and routes around both ends of the partition.

## Use

The arena is included in the [Combat tests add-on](README.md). Enable
`YouLeadIWillFollow.CombatArena.esp` with its DLL and scripts.

Use **YLIWF → Combat tests → Enter arena** to record a return location.
After testing, **Return to previous location** takes you back.

For direct access without a return bookmark, use Skyrim's console:

```text
coc YLIWFCombatArena
```

The arena contains no actors, encounters, quests or startup scripts. It does not
move followers or start combat automatically. Use a dedicated test save.
Start scenarios through **YLIWF → Combat tests**.

## Starting markers

All IDs are local to `YouLeadIWillFollow.CombatArena.esp`. Marker names and
coordinates are listed below.

| Local ID | Purpose | X | Y |
| --- | --- | ---: | ---: |
| `000810` | Observer | -1280 | -1280 |
| `000811` | Defender | -1024 | -256 |
| `000812` | Melee enemy | -1024 | 256 |
| `000813` | Healer | -512 | -1024 |
| `000814` | Ally | -512 | 256 |
| `000815` | Pursuer | 512 | -1024 |
| `000816` | Healer behind partition | 256 | 0 |
| `000817` | Ally beyond partition | 1024 | 0 |


The partition sits on the east side; the west half stays open for melee tests.
Its freestanding stone sections sit over continuous floor tiles and overlap at
their ends. Navigation avoids the divider and leaves routes around both ends.
Lighting is fixed in the cell; ENB and other
visual mods can still affect its appearance.

## Build and validation

The add-on's `plugin/CombatArena.cs` defines the arena in Mutagen. Normal
`./build.ps1 -Target Plugin` and full builds generate its ESP; packaging includes
it with the runner. It creates new CELL, REFR, NAVM and NAVI records without
Bethesda inputs or overrides of existing cells and navmeshes.

Tests verify round-trip records, connected triangle adjacency, obstacle exclusion,
marker reachability and spatial-grid entries. In-game checks are still required:
enter the room, check collision and lighting, and have actors walk around both
ends of the partition. Geometry, avoidance and combat are not proven by binary
validation alone.
