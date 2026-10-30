Scriptname YLIWF_CombatTests Hidden Native

Bool Function IsCurrent(Int ticket) Global Native

; This function performs no latent waits and rechecks its session before combat.
; Results match BeginResult in native/Rules.h. Equipment is verified by the
; runner after the engine has had time to apply EquipItem.
Int Function Begin(Actor subject, Actor enemy, Int ticket) Global
    If !subject || !enemy
        Return 4
    EndIf
    If !IsCurrent(ticket)
        Return 2
    EndIf
    Weapon sword = Game.GetFormFromFile(0x012EB7, "Skyrim.esm") as Weapon
    If !sword
        Return 3
    EndIf
    enemy.RemoveFromAllFactions()
    ; Only the runner's generated opponent is passed here. Keep its armor,
    ; but remove inherited weapons/ammo before establishing the test loadout.
    Int index = enemy.GetNumItems()
    While index > 0
        If !IsCurrent(ticket)
            Return 2
        EndIf
        index -= 1
        Form item = enemy.GetNthForm(index)
        If (item as Weapon) || (item as Ammo)
            enemy.RemoveItem(item, enemy.GetItemCount(item), True, None)
        EndIf
    EndWhile
    enemy.AddItem(sword, 1, True)
    enemy.EquipItem(sword, True, True)
    enemy.SetActorValue("Aggression", 0.0)
    enemy.SetActorValue("Confidence", 4.0)
    enemy.SetActorValue("WeaponSpeedMult", 1.0)
    enemy.SetActorValue("Health", 2000.0)
    enemy.RestoreActorValue("Health", 2000.0)
    If !IsCurrent(ticket)
        Return 2
    EndIf
    enemy.StartCombat(subject)
    If !IsCurrent(ticket)
        enemy.StopCombat()
        Return 2
    EndIf
    subject.StartCombat(enemy)
    Return 1
EndFunction
