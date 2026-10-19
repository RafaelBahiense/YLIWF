# Combat roles

Choose a role under **Followers → Combat role preference** in the SKSE menu.
Roles are available for YLIWF-managed humanoid followers. Quest-managed followers,
including Serana, do not support role assignment (yet).

## Available roles

| Role | Requirements |
| --- | --- |
| Automatic | None; ordinary combat behavior |
| Healer | A known healing spell that can target allies |
| Archer | Bow or crossbow with suitable ammunition |
| Mage | A known offensive spell |
| Frontline | Melee weapon |
| Shield defender | One-handed weapon and shield |
| Support | A known ally buff or enemy crowd-control spell |

**Healer has specialized behavior.** Archer, Mage, Frontline, Shield defender and
Support currently save a preference without changing how the follower fights.
Separate combat-style controls are not yet available.

The menu shows missing requirements and insufficient magicka. Followers use their
existing spells and equipment. Role selections survive saving, dismissal and
primary-slot changes. If requirements are lost, the selection remains saved;
ordinary combat behavior remains available. Choose **Automatic** to clear it.

## Healer behavior

- Prioritizes self-healing when injured and a suitable self-healing spell is known.
- Seeks the most injured nearby ally below 70% health during combat, including
  the player and living followers in bleedout.
- Moves within spell range and seeks space away from enemies for animated casting.
- Returns to fighting when no usable healing action is needed. If pursuers prevent
  escape, fights back temporarily before trying to heal again.
- Restores the party to near-full health after combat, then resumes following.

Healing requires enough magicka and a suitable spell. Ally healing also needs
an unobstructed target within spell range. Supported spells are single-shot
(fire-and-forget). Self-healing alone does not qualify a follower for the Healer role.

Spell selection favors estimated useful healing over six seconds, accounting for
magicka cost, regeneration, casting time and healing duration. Extra healing beyond
the target's missing health adds no benefit. Casts resume when casting finishes;
interrupted or blocked attempts have a short retry delay.

Dialogue, quest scenes, waiting and incapacitation take priority over healing.
Active quest-managed companions can receive healing when recognized as allies.
During Stand down, healers heal themselves and other followers, but never select
the player or area spells. They remain passive even when escape fails.
