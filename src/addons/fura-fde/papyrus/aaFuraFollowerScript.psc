Scriptname aaFuraFollowerScript extends Quest

; Preserve FDE's interface and saved property bindings. The core owns the
; roster transaction and waits for verified completion before returning.
Function SetFollower(Int Status)
    If !YLIWF_SKSE.HasQuestPolicy(DLC1FuraRef, Self)
        Debug.Trace("YLIWF Fura FDE: compatible quest policy is unavailable; follower state was not changed.")
        Return
    EndIf
    If Status == 1
        YLIWF_SKSE.SetFollower(DLC1FuraRef)
    ElseIf Status == 0
        YLIWF_SKSE.ReleaseActorForQuest(DLC1FuraRef, Self)
    EndIf
EndFunction

Faction Property CurrentFollowerFaction Auto
Actor Property DLC1FuraRef Auto
