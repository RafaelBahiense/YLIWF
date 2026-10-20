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

**Healer and Shield defender have specialized behavior.** Archer, Mage, Frontline
and Support currently save a preference without changing how the follower fights.
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

## Shield defender behavior

- Equips an existing one-handed weapon and shield during combat.
- Favors blocking when a nearby enemy makes a melee attack against the defender,
  and attacks between threats.
- Holds ground with less circling and retreating, and favors interrupting incoming
  attacks with bashes when stamina permits, especially enemy power attacks.
- Favors a weapon counterattack when the current opponent recoils or staggers
  within melee range. Incoming attacks from other enemies still take priority.
- Conserves stamina below 25%, reducing bash and power-attack preferences until
  stamina recovers to 50%. Still favors blocking incoming melee attacks.
- Yields to Stand down, waiting, scenes and foreign interrupt packages.

Skyrim controls attack, block and bash timing. The style applies to the follower's
active combat controller; shared NPC records stay unchanged. Clearing the role or
losing the required equipment returns control to ordinary combat AI. Equipment is
not locked. Guarding a particular ally and taunts are not implemented.
Combat mods may trigger dodges independently of these combat-style preferences.
Requiem's perks, stamina costs and animation restrictions remain in effect.

Flow diagnostics report stance changes as `ShieldDefender.Status` and observed
entries into blocking/bash animation states as `ShieldDefender.Actions`. These
counts cover observed starts, not successful hits or blocks; brief animations
between combat updates can be missed.
