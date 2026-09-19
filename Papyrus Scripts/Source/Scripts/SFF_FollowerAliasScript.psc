ScriptName SFF_FollowerAliasScript extends ReferenceAlias

DialogueFollowerScript Property DialogueFollower Auto

Event OnActivate(ObjectReference akActionRef)
	If akActionRef != Game.GetPlayer()
		Return
	EndIf

	If !DialogueFollower
		Return
	EndIf

	Actor a = GetActorRef()
	If !a
		Return
	EndIf

	DialogueFollower.SyncSFFFollowerState()
	DialogueFollower.SFF_SetLastSpeaker(a)
EndEvent

Event OnDeath(Actor akKiller)
	If !DialogueFollower
		Return
	EndIf

	DialogueFollower.SyncSFFFollowerState()
EndEvent