Scriptname YLIWF_CombatTests Hidden Native

Bool Function IsCurrent(Int ticket) Global Native

; This function performs no latent waits and rechecks its session before combat.
; Results match BeginResult in native/Rules.h. Equipment is verified by the
; runner after the engine has had time to apply EquipItem.
Int Function Prepare(Actor enemy, Int ticket, Int loadout) Global
    If !enemy
        Return 4
    EndIf
    If !IsCurrent(ticket)
        Return 2
    EndIf
    Int itemID = 0x012EB7
    If loadout == 1
        itemID = 0x01359D
    ElseIf loadout == 2
        itemID = 0x013985
    EndIf
    Weapon sword = Game.GetFormFromFile(itemID, "Skyrim.esm") as Weapon
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
    If loadout < 3
        enemy.AddItem(sword, 1, True)
        enemy.EquipItem(sword, True, True)
        If loadout == 2
            Ammo arrows = Game.GetFormFromFile(0x01397D, "Skyrim.esm") as Ammo
            If !arrows
                Return 3
            EndIf
            enemy.AddItem(arrows, 500, True)
            enemy.EquipItem(arrows, True, True)
        EndIf
    Else
        Int spellID = 0x012FD0
        If loadout == 4
            spellID = 0x012FCD
        ElseIf loadout == 5
            spellID = 0x01C789
        EndIf
        Spell spell = Game.GetFormFromFile(spellID, "Skyrim.esm") as Spell
        If !spell
            Return 3
        EndIf
        enemy.AddSpell(spell, False)
        enemy.EquipSpell(spell, 1)
    EndIf
    enemy.SetActorValue("Aggression", 0.0)
    enemy.SetActorValue("Confidence", 4.0)
    enemy.SetActorValue("WeaponSpeedMult", 1.0)
    enemy.SetActorValue("Health", 2000.0)
    enemy.RestoreActorValue("Health", 2000.0)
    enemy.SetActorValue("Magicka", 2000.0)
    enemy.RestoreActorValue("Magicka", 2000.0)
    If loadout >= 3
        enemy.SetActorValue("Destruction", 100.0)
    EndIf
    Return 1
EndFunction

; No waits or repeated StartCombat calls. Every step validates the session.
; loadout: sword, greatsword, bow, firebolt, flames.
Int Function Begin(Actor subject, Actor enemy, Int ticket, Int loadout, Actor auxiliary, Bool ally) Global
    If !subject || !enemy
        Return 4
    EndIf
    Int result = Prepare(enemy, ticket, loadout)
    If result != 1
        Return result
    EndIf
    If auxiliary
        result = Prepare(auxiliary, ticket, 0)
        If result != 1
            Return result
        EndIf
        If ally
            auxiliary.SetPlayerTeammate(True, False)
            ; The private protection target observes pressure without joining
            ; the melee and creating incidental friendly-fire damage.
            auxiliary.EnableAI(False)
        Else
            auxiliary.StartCombat(subject)
        EndIf
    EndIf
    If !IsCurrent(ticket)
        Return 2
    EndIf
    If auxiliary && ally
        enemy.StartCombat(auxiliary)
    Else
        enemy.StartCombat(subject)
    EndIf
    If !IsCurrent(ticket)
        enemy.StopCombat()
        If auxiliary
            auxiliary.StopCombat()
        EndIf
        Return 2
    EndIf
    subject.StartCombat(enemy)
    Return 1
EndFunction
