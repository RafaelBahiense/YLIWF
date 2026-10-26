Scriptname aaAelaTransformScript extends Quest

Actor Property Aela  auto

bool Property ObserverTransform auto conditional

Idle Property IdleWerewolfTransformation auto
Race Property WerewolfRace auto
;ReferenceAlias Property ObserverLycanStorage auto
Outfit Property AelaOutfit auto

Weapon Property AelaBaseMeleeWeapon auto
Weapon Property AelaBaseRangedWeapon auto
Keyword Property WeapTypeBow auto

Spell Property WerewolfChangeFX auto
Armor Property WolfSkinFXArmor auto

bool Property DoRealTransform = false auto

bool __observerWeaponWasBow = false
Race __observerOriginalRace = None ; should always be Nord, but we don't know for sure

; Saved flags describe transition work, not follower ownership.
Bool Property YLIWFTransforming = False Auto
Bool YLIWFWasManaged = False
Bool YLIWFWasTeammate = False
Int YLIWFTransformGeneration = 0

Function Transform(Int akInt = 0)
    If !Aela || YLIWFTransforming || Aela.GetRace() == WerewolfRace
        Return
    EndIf
    YLIWFTransforming = True
    YLIWFTransformGeneration += 1
    Int generation = YLIWFTransformGeneration
    YLIWFWasManaged = YLIWF_SKSE.IsManagedFollower(Aela)
    YLIWFWasTeammate = Aela.IsPlayerTeammate()
    __transformTracked = False
    __observerOriginalRace = Aela.GetRace()

    WerewolfChangeFX.Cast(Aela)

    RegisterForAnimationEvent(Aela, "SetRace")
    Utility.Wait(10)
    If generation != YLIWFTransformGeneration || !YLIWFTransforming
        Return
    EndIf
    ObserverActuallyTransform()
    if akInt == 1
        RegisterForSingleUpdate(150)
        Aela.AddSpell(crWerewolfUnarmedDamage03)
        Game.EnableFastTravel(False)
    else
        Aela.AddSpell(crWerewolfUnarmedDamage06)
    endif

    Aela.UnequipItem(Torch01)

    TorchCount = Aela.GetItemCount(Torch01)
    Aela.RemoveItem(Torch01, TorchCount)

    int playerLevel = Aela.GetLevel()
    if     (playerLevel <= 10)
        Aela.AddSpell(PlayerWerewolfLvl10AndBelowAbility, false)
    elseif (playerLevel <= 15)
        Aela.AddSpell(PlayerWerewolfLvl15AndBelowAbility, false)
    elseif (playerLevel <= 20)
        Aela.AddSpell(PlayerWerewolfLvl20AndBelowAbility, false)
    elseif (playerLevel <= 25)
        Aela.AddSpell(PlayerWerewolfLvl25AndBelowAbility, false)
    elseif (playerLevel <= 30)
        Aela.AddSpell(PlayerWerewolfLvl30AndBelowAbility, false)
    elseif (playerLevel <= 35)
        Aela.AddSpell(PlayerWerewolfLvl35AndBelowAbility, false)
    elseif (playerLevel <= 40)
        Aela.AddSpell(PlayerWerewolfLvl40AndBelowAbility, false)
    elseif (playerLevel <= 45)
        Aela.AddSpell(PlayerWerewolfLvl45AndBelowAbility, false)
    else
        Aela.AddSpell(PlayerWerewolfLvl50AndOverAbility, false)
    endif
EndFunction


Event OnUpdate()
    TurnBack()
EndEvent

bool __transformTracked = false

Function ObserverActuallyTransform()
    if (__transformTracked)
        ; transformation already happened from FX spell
        return
    endif

    __transformTracked = true

    ObserverTransform = False

EndFunction

Function TurnBack()
    ; Cancel post-wait work from an earlier Transform invocation.
    YLIWFTransformGeneration += 1
    UnregisterForUpdate()
    if (Aela.GetRace() == WerewolfRace && __observerOriginalRace != None)
;         Debug.Trace("C01: Setting race " + __observerOriginalRace + " on " + obs)
        Aela.SetRace(__observerOriginalRace)
        ; Remove the Transformation effect armor if they have it on.
        if (Aela.GetItemCount(WolfSkinFXArmor) > 0)
            (Aela.Removeitem(WolfSkinFXArmor,1,True,none))
        endif
        __observerOriginalRace = None
        ;ObserverLycanStorage.GetReference().RemoveAllItems(Aela)
            ;Aela.SetOutfit(AelaOutfit)
            ;if (__observerWeaponWasBow)
            ;    Aela.AddItem(AelaBaseRangedWeapon, 1)
            ;    Aela.EquipItem(AelaBaseRangedWeapon)
            ;else
            ;    Aela.AddItem(AelaBaseMeleeWeapon, 1)
            ;    Aela.EquipItem(AelaBaseMeleeWeapon)
            ;endif
    endif
    Aela.RemoveSpell(PlayerWerewolfLvl10AndBelowAbility)
    Aela.RemoveSpell(PlayerWerewolfLvl15AndBelowAbility)
    Aela.RemoveSpell(PlayerWerewolfLvl20AndBelowAbility)
    Aela.RemoveSpell(PlayerWerewolfLvl25AndBelowAbility)
    Aela.RemoveSpell(PlayerWerewolfLvl30AndBelowAbility)
    Aela.RemoveSpell(PlayerWerewolfLvl35AndBelowAbility)
    Aela.RemoveSpell(PlayerWerewolfLvl40AndBelowAbility)
    Aela.RemoveSpell(PlayerWerewolfLvl45AndBelowAbility)
    Aela.RemoveSpell(PlayerWerewolfLvl50AndOverAbility)
    Aela.AddItem(EbonyArrow, 1)
    Aela.RemoveItem(EbonyArrow, 1)
    If YLIWF_SKSE.IsManagedFollower(Aela)
        Aela.SetPlayerTeammate(True)
    ElseIf !YLIWFWasManaged && YLIWFWasTeammate && !Aela.IsHostileToActor(Game.GetPlayer())
        ; Preserve a pre-existing non-YLIWF teammate; never resurrect dismissed ownership.
        Aela.SetPlayerTeammate(True)
    EndIf
    Aela.AddItem(Torch01, TorchCount)
    TorchCount = 0
    __transformTracked = False
    YLIWFTransforming = False
    Aela.EvaluatePackage()
EndFunction

SPELL Property crWerewolfUnarmedDamage03  Auto

SPELL Property crWerewolfUnarmedDamage06  Auto

Spell Property PlayerWerewolfLvl10AndBelowAbility auto
Spell Property PlayerWerewolfLvl15AndBelowAbility auto
Spell Property PlayerWerewolfLvl20AndBelowAbility auto
Spell Property PlayerWerewolfLvl25AndBelowAbility auto
Spell Property PlayerWerewolfLvl30AndBelowAbility auto
Spell Property PlayerWerewolfLvl35AndBelowAbility auto
Spell Property PlayerWerewolfLvl40AndBelowAbility auto
Spell Property PlayerWerewolfLvl45AndBelowAbility auto
Spell Property PlayerWerewolfLvl50AndOverAbility auto
Light Property Torch01  Auto

Ammo Property EbonyArrow  Auto

Int Property TorchCount  Auto
