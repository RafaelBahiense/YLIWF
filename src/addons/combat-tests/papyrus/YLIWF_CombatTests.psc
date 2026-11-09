Scriptname YLIWF_CombatTests Hidden Native

Bool Function IsCurrent(Int ticket) Global Native

; Fixture-only setup. SetActorValue alone leaves ability/perk modifiers in place.
Bool Function NormalizeResource(Actor fixture, String valueName, Float target) Global
    If !fixture || target <= 0.0
        Return False
    EndIf
    Float maximum = fixture.GetActorValueMax(valueName)
    Float adjustedBase = fixture.GetBaseActorValue(valueName) + target - maximum
    If !(adjustedBase > 0.0 && adjustedBase < 1000000.0)
        Return False
    EndIf
    fixture.SetActorValue(valueName, adjustedBase)
    fixture.RestoreActorValue(valueName, target)
    Float actual = fixture.GetActorValue(valueName)
    maximum = fixture.GetActorValueMax(valueName)
    Return actual >= target - 0.1 && actual <= target + 0.1 && maximum >= target - 0.1 && maximum <= target + 0.1
EndFunction

Int Function ConfigureParticipant(Actor participant, Int ticket, Int loadout, Bool ally, Float health) Global
    If !participant || health <= 0.0
        Return 4
    EndIf
    Int result = Prepare(participant, ticket, loadout)
    If result != 1
        Return result
    EndIf
    If !IsCurrent(ticket)
        Return 2
    EndIf
    Int factionID = 0x000845
    If ally
        factionID = 0x000844
        Faction playerFaction = Game.GetFormFromFile(0x000DB1, "Skyrim.esm") as Faction
        If !playerFaction
            Return 3
        EndIf
        participant.AddToFaction(playerFaction)
        participant.SetPlayerTeammate(True, False)
    EndIf
    Faction team = Game.GetFormFromFile(factionID, "YouLeadIWillFollow.CombatArena.esp") as Faction
    If !team
        Return 3
    EndIf
    participant.AddToFaction(team)
    participant.SetActorValue("Health", health)
    participant.RestoreActorValue("Health", health)
    participant.EnableAI(True)
    participant.DrawWeapon()
    Return 1
EndFunction

Int Function NormalizeParticipant(Actor participant, Int ticket, Float health) Global
    If !IsCurrent(ticket)
        Return 2
    EndIf
    If !NormalizeResource(participant, "Health", health) || !NormalizeResource(participant, "Magicka", 2000.0)
        Return 4
    EndIf
    Return 1
EndFunction

Int Function StartParticipant(Actor participant, Actor target, Int ticket, Bool setAggression) Global
    If !participant || !target || !IsCurrent(ticket)
        Return 2
    EndIf
    If setAggression
        participant.SetActorValue("Aggression", 1.0)
    EndIf
    participant.StartCombat(target)
    If !IsCurrent(ticket)
        participant.StopCombat()
        Return 2
    EndIf
    Return 1
EndFunction

; This function performs no latent waits and rechecks its session before combat.
; Results match BeginResult in native/runtime/Rules.h. Equipment is verified by the
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
