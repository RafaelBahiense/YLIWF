# Aela FDE compatibility patch

Integrates **Follower Dialogue Expansion – Aela the Huntress**, including Part 2,
with YLIWF's core follower controller.

## Features

- Standard recruitment, core slots, Follow, Wait, distance and combat roles.
- Keeps Aela's slot during werewolf transformation. Movement overrides and
  humanoid combat roles suspend until she returns; Dismiss remains available
  outside scenes and quest movement.
- Reconciles teammate status after transformation without restoring dismissed
  YLIWF ownership.
- Releases Aela's specific slot when FDE makes her leave or become hostile.
- Preserves FDE approval, forgiveness, recruitment conditions, scenes and quest
  packages. Story restrictions take priority over follower commands.

## Installation

Requires a compatible **YLIWF core**, `FDE Aela.esp` and
`FDE Aela Part 2.esp`.

Install `YouLeadIWillFollow-AelaFDE-<version>.zip`. In MO2, place it below FDE Aela
so its replacement scripts win conflicts. Keep `YouLeadIWillFollow.AelaFDE.dll`
and its scripts installed together. No additional ESP is needed.

The add-on supports the original Aela. Existing YLIWF recruitment keeps its slot.
Install while she is in humanoid form and no FDE transformation or departure is
in progress.

[Developer documentation](docs/development.md) describes the integration and
in-game checks.
