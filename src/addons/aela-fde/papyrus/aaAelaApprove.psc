Scriptname aaAelaApprove extends Quest

Function ap(int modifier)

    int modify = modifier

        aaAelaApproval.Mod(modifier)
        if modify >= 1
            aaAelaApproveMessage.Show()
        elseif modify < 0
            aaAelaDisapproveMessage.Show()
        endif

EndFunction

Event OnUpdateGameTime()
    If AelaTheHuntressREF.IsInFaction(CurrentFollowerFaction)
        aaAelaApproval.Mod(1)
    EndIf
    RegisterForSingleUpdateGameTime(24)
EndEvent

Function AelaCombat()
If !YLIWF_AelaFDE.ReleaseFollower(AelaTheHuntressRef)
    Return
EndIf
aaAela8Q.Transform()
AelaTheHuntressRef.SetRelationshipRank(PlayerRef, -3)
AelaTheHuntressRef.Say(DialogueFollowerDismissTopic)
AelaTheHuntressRef.RemoveFromFaction(CurrentFollowerFaction)
if C06PostQuest.IsRunning()
C06PostQuest.Stop()
endif
AelaTheHuntressRef.RemoveFromFaction(CompanionsFaction)
AelaTheHuntressRef.AddToFaction(aaAelaEnemyFaction)
AelaTheHuntressRef.SetAV("Aggression", 1)
AelaTheHuntressRef.SetAV("Confidence", 4)
AelaTheHuntressRef.GetActorBase().SetEssential(0)
AelaTheHuntressRef.SetAV("SpeedMult", 150)
AelaTheHuntressRef.AddSpell(aaAelaFortifyHealth)
AelaTheHuntressRef.StartCombat(PlayerRef)
EndFunction

Function GraceLost(int Trust = 0)
aaAelaGrace.Mod(-1)
if Trust == 0
    aaAelaGraceLostMessage.Show()
else
    aaAelaTrustMessage.Show()
endif
EndFunction


GlobalVariable Property aaAelaApproval  Auto

Message Property aaAelaApproveMessage  Auto
Message Property aaAelaDisapproveMessage  Auto
Faction Property CurrentFollowerFaction  Auto

Actor Property AelaTheHuntressRef  Auto


aaAelaTransformScript Property aaAela8Q  Auto

GlobalVariable Property aaAelaGrace  Auto

Message Property aaAelaGraceLostMessage  Auto

Message Property aaAelaTrustMessage  Auto

Actor Property PlayerRef  Auto

Faction Property aaAelaEnemyFaction  Auto

Faction Property CompanionsFaction  Auto

SPELL Property aaAelaFortifyHealth  Auto

Topic Property DialogueFollowerDismissTopic  Auto

Quest Property C06PostQuest  Auto
