ScriptName DialogueFollowerScript extends Quest Conditional

GlobalVariable Property pPlayerFollowerCount Auto
GlobalVariable Property pPlayerAnimalCount Auto
ReferenceAlias Property pFollowerAlias Auto
ReferenceAlias Property pAnimalAlias Auto
Faction Property pDismissedFollower Auto
Faction Property pCurrentHireling Auto
Message Property FollowerDismissMessage Auto
Message Property AnimalDismissMessage Auto
Message Property FollowerDismissMessageWedding Auto
Message Property FollowerDismissMessageCompanions Auto
Message Property FollowerDismissMessageCompanionsMale Auto
Message Property FollowerDismissMessageCompanionsFemale Auto
Message Property FollowerDismissMessageWait Auto
SetHirelingRehire Property HirelingRehireScript Auto
Int Property iFollowerDismiss Auto Conditional
Weapon Property FollowerHuntingBow Auto
Ammo Property FollowerIronArrow Auto
Int Property USKPMjollInWindhelm = 0 Auto Conditional

Int Property iSFFFollowerCount Auto Conditional
ReferenceAlias[] Property SFFExtraAliases Auto
GlobalVariable Property SFFCanRecruitMore Auto
GlobalVariable Property SFFCurrentFollowerCount Auto
Actor SFFLastSpeaker

Event OnInit()
	SyncSFFFollowerState()
EndEvent

Function SFF_SetLastSpeaker(Actor akActor)
	If !akActor
		Return
	EndIf
	If akActor == Game.GetPlayer()
		Return
	EndIf
	SFFLastSpeaker = akActor
EndFunction

Function SFF_ClearLastSpeakerIfMatches(Actor akActor)
	If SFFLastSpeaker == akActor
		SFFLastSpeaker = None
	EndIf
EndFunction

Actor Function SFF_GetPrimaryFollower()
	Actor a = pFollowerAlias.GetActorReference()
	If a && a.IsDead()
		SFF_SKSE.RestoreFollowerEssential(a)
		pFollowerAlias.UnregisterForUpdateGameTime()
		pFollowerAlias.Clear()
		If SFFLastSpeaker == a
			SFFLastSpeaker = None
		EndIf
		Return None
	EndIf
	Return a
EndFunction

Int Function SFF_GetExtraAliasIndexByActor(Actor akActor)
	If !akActor
		Return -1
	EndIf

	Int i = 0
	Int n = SFFExtraAliases.Length
	While i < n
		ReferenceAlias a = SFFExtraAliases[i]
		If a
			Actor current = a.GetActorReference()
			If current == akActor
				Return i
			EndIf
		EndIf
		i += 1
	EndWhile

	Return -1
EndFunction

Int Function SFF_FindFirstFreeExtraAliasIndex()
	Int i = 0
	Int n = SFFExtraAliases.Length
	While i < n
		ReferenceAlias a = SFFExtraAliases[i]
		If a && !a.GetReference()
			Return i
		EndIf
		i += 1
	EndWhile
	Return -1
EndFunction

Int Function SFF_CleanupDeadExtraFollowers()
	Int removed = 0
	Int i = 0
	Int n = SFFExtraAliases.Length

	While i < n
		ReferenceAlias a = SFFExtraAliases[i]
		If a
			Actor current = a.GetActorReference()
			If current && current.IsDead()
				If SFFLastSpeaker == current
					SFFLastSpeaker = None
				EndIf
				SFF_SKSE.RestoreFollowerEssential(current)
				a.UnregisterForUpdateGameTime()
				a.Clear()
				removed += 1
			EndIf
		EndIf
		i += 1
	EndWhile

	Return removed
EndFunction

Int Function SFF_GetExtraFollowerCount()
	SFF_CleanupDeadExtraFollowers()

	Int count = 0
	Int i = 0
	Int n = SFFExtraAliases.Length

	While i < n
		ReferenceAlias a = SFFExtraAliases[i]
		If a && a.GetReference()
			count += 1
		EndIf
		i += 1
	EndWhile

	Return count
EndFunction

Bool Function SFF_IsPrimaryFollower(Actor akActor)
	If !akActor
		Return False
	EndIf
	Return pFollowerAlias.GetActorReference() == akActor
EndFunction

Bool Function SFF_IsExtraFollower(Actor akActor)
	If !akActor
		Return False
	EndIf
	Return SFF_GetExtraAliasIndexByActor(akActor) >= 0
EndFunction

Bool Function IsManagedFollower(Actor akActor)
	If !akActor
		Return False
	EndIf

	If SFF_IsPrimaryFollower(akActor)
		Return True
	EndIf

	If SFF_IsExtraFollower(akActor)
		Return True
	EndIf

	Return False
EndFunction

ReferenceAlias Function SFF_GetAliasForActor(Actor akActor)
	If !akActor
		Return None
	EndIf

	If pFollowerAlias.GetActorReference() == akActor
		Return pFollowerAlias
	EndIf

	Int idx = SFF_GetExtraAliasIndexByActor(akActor)
	If idx < 0
		Return None
	EndIf

	Return SFFExtraAliases[idx]
EndFunction

Bool Function SFF_AddExtraFollowerAlias(Actor akActor)
	If !akActor
		Return False
	EndIf
	If akActor == Game.GetPlayer()
		Return False
	EndIf

	SFF_CleanupDeadExtraFollowers()

	If SFF_IsExtraFollower(akActor)
		Return True
	EndIf

	Int freeIndex = SFF_FindFirstFreeExtraAliasIndex()
	If freeIndex < 0
		Return False
	EndIf

	ReferenceAlias a = SFFExtraAliases[freeIndex]
	a.ForceRefTo(akActor)
	akActor.EvaluatePackage()
	Return True
EndFunction

Actor Function SFF_PopFirstExtraFollower()
	SFF_CleanupDeadExtraFollowers()

	Int i = 0
	Int n = SFFExtraAliases.Length
	While i < n
		ReferenceAlias a = SFFExtraAliases[i]
		If a
			Actor current = a.GetActorReference()
			If current
				If SFFLastSpeaker == current
					SFFLastSpeaker = None
				EndIf
				a.UnregisterForUpdateGameTime()
				a.Clear()
				Return current
			EndIf
		EndIf
		i += 1
	EndWhile

	Return None
EndFunction

Int Function SFF_GetMaxFollowersSafe()
	Int maxFollowers = SFF_SKSE.GetMaxFollowers()
	If maxFollowers < 1
		maxFollowers = 1
	EndIf

	Int aliasCapacity = SFFExtraAliases.Length + 1

	If maxFollowers > aliasCapacity
		maxFollowers = aliasCapacity
	EndIf

	Return maxFollowers
EndFunction

Function SFF_UpdateFollowerGlobals()
	If iSFFFollowerCount > 0
		pPlayerFollowerCount.SetValue(1)
	Else
		pPlayerFollowerCount.SetValue(0)
	EndIf

	If SFFCurrentFollowerCount
		SFFCurrentFollowerCount.SetValue(iSFFFollowerCount)
	EndIf

	Int maxFollowers = SFF_GetMaxFollowersSafe()
	If SFFCanRecruitMore
		If iSFFFollowerCount < maxFollowers
			SFFCanRecruitMore.SetValue(1)
		Else
			SFFCanRecruitMore.SetValue(0)
		EndIf
	EndIf
EndFunction

Function SyncSFFFollowerState()
	Actor primary = SFF_GetPrimaryFollower()
	If !primary
		SFF_PromoteExtraToPrimaryIfNeeded()
		primary = SFF_GetPrimaryFollower()
	EndIf

	iSFFFollowerCount = SFF_GetExtraFollowerCount()
	If primary
		iSFFFollowerCount += 1
	EndIf

	SFF_UpdateFollowerGlobals()
EndFunction

Function PrepareFollowerActor(Actor FollowerActor)
	If !FollowerActor
		Return
	EndIf

	FollowerActor.RemoveFromFaction(pDismissedFollower)

	Int rel = FollowerActor.GetRelationshipRank(Game.GetPlayer())
	If rel < 3 && rel >= 0
		FollowerActor.SetRelationshipRank(Game.GetPlayer(), 3)
	EndIf

	FollowerActor.SetPlayerTeammate()
	SFF_SKSE.AddVanillaFollower(FollowerActor)
	FollowerActor.StopCombatAlarm()
	FollowerActor.SetAV("WaitingForPlayer", 0)
	FollowerActor.EvaluatePackage()
EndFunction

Function ShowFollowerDismissMessageByType(Int iMessage)
	If iMessage == 0
		FollowerDismissMessage.Show()
	ElseIf iMessage == 1
		FollowerDismissMessageWedding.Show()
	ElseIf iMessage == 2
		FollowerDismissMessageCompanions.Show()
	ElseIf iMessage == 3
		FollowerDismissMessageCompanionsMale.Show()
	ElseIf iMessage == 4
		FollowerDismissMessageCompanionsFemale.Show()
	ElseIf iMessage == 5
		FollowerDismissMessageWait.Show()
	Else
		FollowerDismissMessage.Show()
	EndIf
EndFunction

Function CleanupDismissedFollowerActor(Actor DismissedFollowerActor, Int iSayLine = 1, ReferenceAlias akOwnAlias = None)
	If !DismissedFollowerActor
		Return
	EndIf

	If akOwnAlias
		akOwnAlias.UnregisterForUpdateGameTime()
	EndIf

	DismissedFollowerActor.StopCombatAlarm()
	DismissedFollowerActor.AddToFaction(pDismissedFollower)
	DismissedFollowerActor.SetPlayerTeammate(False)
	DismissedFollowerActor.RemoveFromFaction(pCurrentHireling)
	DismissedFollowerActor.SetAV("WaitingForPlayer", 0)
	DismissedFollowerActor.RemoveItem(FollowerHuntingBow, 999, True)
	DismissedFollowerActor.RemoveItem(FollowerIronArrow, 999, True)

	If HirelingRehireScript
		HirelingRehireScript.DismissHireling(DismissedFollowerActor.GetActorBase())
	EndIf

	If iSayLine == 1
		iFollowerDismiss = 1
		DismissedFollowerActor.EvaluatePackage()
		Utility.Wait(2)
	EndIf
EndFunction

Function SFF_PromoteExtraToPrimaryIfNeeded()
	If pFollowerAlias && pFollowerAlias.GetReference()
		Return
	EndIf

	Actor promoted = SFF_PopFirstExtraFollower()
	If promoted
		pFollowerAlias.ForceRefTo(promoted)
		SFF_SKSE.ApplyFollowerEssential(promoted)
		If promoted.GetAV("WaitingForPlayer") == 1
			pFollowerAlias.RegisterForUpdateGameTime(72)
		EndIf
		promoted.EvaluatePackage()
	EndIf
EndFunction

Actor Function GetDialogueFollowerTarget()
	Actor target = Game.GetDialogueTarget() as Actor
	If target && IsManagedFollower(target)
		Return target
	EndIf

	If SFFLastSpeaker && IsManagedFollower(SFFLastSpeaker)
		Return SFFLastSpeaker
	EndIf

	Return pFollowerAlias.GetActorReference()
EndFunction

Function SetFollower(ObjectReference FollowerRef)
	Actor FollowerActor = FollowerRef as Actor
	If !FollowerActor
		Return
	EndIf

	SyncSFFFollowerState()

	Int maxFollowers = SFF_GetMaxFollowersSafe()
	Int currentCount = iSFFFollowerCount

	If IsManagedFollower(FollowerActor)
		PrepareFollowerActor(FollowerActor)
		Return
	EndIf

	Actor CurrentFollowerActor = SFF_GetPrimaryFollower()

	If CurrentFollowerActor
		If currentCount >= maxFollowers
			Return
		EndIf

		If !SFF_AddExtraFollowerAlias(FollowerActor)
			SyncSFFFollowerState()
			Return
		EndIf

		PrepareFollowerActor(FollowerActor)
		SFF_SKSE.ApplyFollowerEssential(FollowerActor)

		If pFollowerAlias.GetActorReference() != CurrentFollowerActor
			pFollowerAlias.ForceRefTo(CurrentFollowerActor)
			CurrentFollowerActor.EvaluatePackage()
		EndIf

		SyncSFFFollowerState()
		Return
	EndIf

	If currentCount >= maxFollowers
		Return
	EndIf

	PrepareFollowerActor(FollowerActor)
	pFollowerAlias.ForceRefTo(FollowerActor)
	SFF_SKSE.ApplyFollowerEssential(FollowerActor)
	SyncSFFFollowerState()
EndFunction

Function SetAnimal(ObjectReference AnimalRef)
	Actor AnimalActor = AnimalRef as Actor
	If !AnimalActor
		Return
	EndIf

	AnimalActor.SetAV("Lockpicking", 0)
	AnimalActor.SetRelationshipRank(Game.GetPlayer(), 3)
	AnimalActor.SetPlayerTeammate(abCanDoFavor = False)
	pAnimalAlias.ForceRefTo(AnimalActor)
	pPlayerAnimalCount.SetValue(1)
EndFunction

Function FollowerWait()
	SyncSFFFollowerState()
	SFF_WaitActor(GetDialogueFollowerTarget())
EndFunction

Function SFF_WaitActor(Actor FollowerActor)
	If !FollowerActor
		Return
	EndIf

	FollowerActor.SetAV("WaitingForPlayer", 1)
	FollowerActor.EvaluatePackage()

	ReferenceAlias ownAlias = SFF_GetAliasForActor(FollowerActor)
	If ownAlias
		ownAlias.RegisterForUpdateGameTime(72)
	EndIf
EndFunction

Function AnimalWait()
	Actor AnimalActor = pAnimalAlias.GetActorReference()
	If !AnimalActor
		Return
	EndIf

	AnimalActor.SetAV("WaitingForPlayer", 1)
	pAnimalAlias.RegisterForUpdateGameTime(72)
EndFunction

Function FollowerFollow()
	SyncSFFFollowerState()
	SFF_FollowActor(GetDialogueFollowerTarget())
EndFunction

Function SFF_FollowActor(Actor FollowerActor)
	If !FollowerActor
		Return
	EndIf

	FollowerActor.SetAV("WaitingForPlayer", 0)
	FollowerActor.EvaluatePackage()

	ReferenceAlias ownAlias = SFF_GetAliasForActor(FollowerActor)
	If ownAlias
		ownAlias.UnregisterForUpdateGameTime()
	EndIf

	If SFF_IsPrimaryFollower(FollowerActor)
		SetObjectiveDisplayed(10, abDisplayed = False)
	EndIf
EndFunction

Function AnimalFollow()
	Actor AnimalActor = pAnimalAlias.GetActorReference()
	If !AnimalActor
		Return
	EndIf

	AnimalActor.SetAV("WaitingForPlayer", 0)
	SetObjectiveDisplayed(20, abDisplayed = False)
	pAnimalAlias.UnregisterForUpdateGameTime()
EndFunction

Function DismissFollower(Int iMessage = 0, Int iSayLine = 1)
	SyncSFFFollowerState()
	SFF_DismissActor(GetDialogueFollowerTarget(), iMessage, iSayLine)
EndFunction

Function SFF_DismissActor(Actor DismissedFollowerActor, Int iMessage = 0, Int iSayLine = 1)
	If !DismissedFollowerActor
		Return
	EndIf

	ReferenceAlias ownAlias = SFF_GetAliasForActor(DismissedFollowerActor)
	If !ownAlias
		Return
	EndIf

	Bool wasAlive = !DismissedFollowerActor.IsDead()

	If wasAlive
		ShowFollowerDismissMessageByType(iMessage)
		CleanupDismissedFollowerActor(DismissedFollowerActor, iSayLine, ownAlias)
		ownAlias = SFF_GetAliasForActor(DismissedFollowerActor)
	EndIf

	SFF_SKSE.RestoreFollowerEssential(DismissedFollowerActor)

	If ownAlias
		ownAlias.UnregisterForUpdateGameTime()
		ownAlias.Clear()
	EndIf

	SFF_ClearLastSpeakerIfMatches(DismissedFollowerActor)

	If wasAlive
		iFollowerDismiss = 0
	EndIf

	SyncSFFFollowerState()
EndFunction

Function SFF_HandleFollowerDeath(ReferenceAlias akAlias, Actor akActor)
	If akAlias
		akAlias.UnregisterForUpdateGameTime()
	EndIf

	If akActor
		akActor.RemoveFromFaction(pCurrentHireling)
	EndIf

	SyncSFFFollowerState()
EndFunction

Function DismissAnimal()
	If pAnimalAlias.GetActorReference() && pAnimalAlias.GetActorReference().IsDead() == False
		Actor DismissedAnimalActor = pAnimalAlias.GetActorReference()
		pAnimalAlias.UnregisterForUpdateGameTime()
		DismissedAnimalActor.SetPlayerTeammate(False)
		DismissedAnimalActor.SetActorValue("Variable04", 0)
		pPlayerAnimalCount.SetValue(0)
		pAnimalAlias.Clear()
		AnimalDismissMessage.Show()
	EndIf
EndFunction