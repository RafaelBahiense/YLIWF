# Aela FDE integration

## Ownership and bindings

This is a core-owned follower policy, not an external follower adapter. The
add-on does not replace recruitment topics, claim an external slot or change the
core API. `FDE Aela.esp:0008D4` is the fixed policy owner for the original Aela
(`Skyrim.esm:01A697`). Transformation uses `FDE Aela Part 2.esp:000800`.

Native inspection validates both controller actor bindings, the patched
`YLIWFTransforming` property, the potential-follower faction and werewolf race.
Missing bindings suspend control and deny quest-release authority. Policies read
state on the main thread; the core's periodic policy observation reconciles
movement and combat restrictions. No add-on timer or serialization namespace is
needed.

FDE Aela packages, including banter modules, and active scenes retain priority.
Transformation transition work and werewolf race block every action except
Dismiss; scene, hostile and unavailable states take precedence. Losing potential
follower membership blocks recruitment without restoring authored eligibility.

## Replacement scripts

The package contains fifteen FDE replacements and one helper:

- `aaAelaTransformScript` preserves existing properties and saved variable names.
  It records transition state and prior ownership before the latent wait, guards
  overlapping starts, and invalidates delayed work on return. Return uses the
  actor's current race and a valid captured original race. Aela's slot is retained;
  teammate restoration checks live core ownership rather than always setting it.
  Prior unmanaged teammate status is restored only when she was not core-owned
  and is not hostile. FDE's FX, ability scaling and timer remain in place.
- `aaAelaApprove.AelaCombat` releases the specific actor before transformation
  and hostility. Its approval and forgiveness functions retain FDE behavior.
- Thirteen departure fragments call the same helper before their authored
  effects. Other dialogue, approval changes and one-time globals remain intact.
- `YLIWF_AelaFDE.ReleaseFollower` accepts only the original reference, checks
  registered policy ownership, and uses the core's acknowledged
  `ReleaseActorForQuest`. Failure stops the departure effects; calls are not
  retried. An already unmanaged actor needs no slot release.

The replacements derive from FDE Aela's supplied scripts. Updating FDE requires
comparing its bound properties, fragments and transformation logic against these
sources, not just checking filenames.

## Validation

Native rule tests cover normal eligibility, transformation restrictions,
dismissal and higher-priority story locks. Papyrus compilation validates the
replacement interfaces. Engine scheduling and persistence require in-game tests:

1. Recruit with other core followers present; verify one slot, Follow, Wait,
   distance and combat role selection.
2. Trigger FDE's timed transformation. Verify FX, beast combat, retained slot,
   restricted controls and return to the captured race and waiting/follow state.
3. Dismiss during beast form outside a scene; let the timer return her. Confirm
   no restored slot or teammate flag. Repeat with dismissal before the latent
   transformation wait finishes.
4. Save/load in beast form and during the transition, then allow return. Repeat
   with two successive transformations and confirm no duplicated delayed work.
5. Trigger an authored peaceful departure and hostile breakup. Confirm only
   Aela's slot is released and FDE relationship, factions and hostility persist.
6. Verify quest/banter packages and scenes suspend conflicting core orders and
   normal controls resume afterward. Test a missing replacement script as a
   failure case; verify no quest release is authorized.

