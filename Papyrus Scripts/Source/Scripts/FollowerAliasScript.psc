ScriptName FollowerAliasScript extends ReferenceAlias

DialogueFollowerScript Property DialogueFollower Auto
GlobalVariable Property PlayerFollowerCount Auto
Faction Property CurrentHirelingFaction Auto

Event OnUpdateGameTime()
	UnregisterForUpdateGameTime()
	Actor a = GetActorRef()
	If a && a.GetAV("WaitingForPlayer") != 0
		DialogueFollower.SFF_DismissActor(a, 5)
	EndIf
EndEvent

Event OnUnload()
	Actor a = GetActorRef()
	If a && a.GetAV("WaitingForPlayer") == 1
		RegisterForUpdateGameTime(72)
	EndIf
EndEvent

Event OnCombatStateChanged(Actor akTarget, Int aeCombatState)
	If akTarget == Game.GetPlayer()
		DialogueFollower.SFF_DismissActor(GetActorRef(), 0, 0)
	EndIf
EndEvent

Event OnDeath(Actor akKiller)
	DialogueFollower.SFF_HandleFollowerDeath(Self, GetActorRef())
EndEvent
