ScriptName follower3dnpc extends Quest Conditional

Message Property AnimalDismissMessage Auto
Bool Property CanRecruit Auto Conditional
Message Property FollowerDismissMessage Auto
Message Property FollowerDismissMessageCompanions Auto
Message Property FollowerDismissMessageCompanionsFemale Auto
Message Property FollowerDismissMessageCompanionsMale Auto
Message Property FollowerDismissMessageWait Auto
Message Property FollowerDismissMessageWedding Auto
SetHirelingRehire3DNPC Property HirelingRehireScript Auto
Int Property iFollowerDismiss Auto Conditional
ReferenceAlias Property pAnimalAlias Auto
Faction Property pCurrentHireling Auto
Faction Property pDismissedFollower Auto
ReferenceAlias Property pFollowerAlias Auto
ReferenceAlias Property pFollowerAlias1 Auto
ReferenceAlias Property pFollowerAlias2 Auto
GlobalVariable Property pPlayerAnimalCount Auto
GlobalVariable Property pPlayerFollowerCount Auto

DialogueFollowerScript Function SFF()
	Return Game.GetFormFromFile(0x000750BA, "Skyrim.esm") as DialogueFollowerScript
EndFunction

Faction Function BardAudienceExcluded()
	Return Game.GetFormFromFile(0x0010FCB4, "Skyrim.esm") as Faction
EndFunction

Actor Function ResolveFollower(ObjectReference akFollower)
	Actor a = akFollower as Actor
	If !a
		a = Game.GetDialogueTarget() as Actor
	EndIf
	Return a
EndFunction

Bool Function IsLegacyFollower(Actor akActor)
	Return akActor && pFollowerAlias.GetActorReference() == akActor
EndFunction

Function SetFollower(ObjectReference FollowerRef)
	Actor FollowerActor = FollowerRef as Actor
	If !FollowerActor
		Return
	EndIf

	Bool legacy = IsLegacyFollower(FollowerActor)
	If legacy
		pFollowerAlias.UnregisterForUpdateGameTime()
		pFollowerAlias.Clear()
	EndIf

	DialogueFollowerScript sff = SFF()
	sff.SetFollower(FollowerActor)

	If !sff.IsManagedFollower(FollowerActor)
		If legacy
			pFollowerAlias.ForceRefTo(FollowerActor)
		EndIf
		Return
	EndIf

	If legacy
		pPlayerFollowerCount.SetValue(0)
	EndIf

	FollowerActor.RemoveFromFaction(pDismissedFollower)

	Faction excluded = BardAudienceExcluded()
	If !FollowerActor.IsInFaction(excluded)
		FollowerActor.AddToFaction(excluded)
		FollowerActor.SetFactionRank(excluded, 1)
	EndIf
EndFunction

Function FollowerWait(ObjectReference akFollower)
	Actor FollowerActor = ResolveFollower(akFollower)
	If !FollowerActor
		Return
	EndIf

	If IsLegacyFollower(FollowerActor)
		FollowerActor.SetAV("WaitingForPlayer", 1)
		pFollowerAlias.RegisterForUpdateGameTime(72)
		Return
	EndIf

	SFF().SFF_WaitActor(FollowerActor)
EndFunction

Function FollowerFollow(ObjectReference akFollower)
	Actor FollowerActor = ResolveFollower(akFollower)
	If !FollowerActor
		Return
	EndIf

	If IsLegacyFollower(FollowerActor)
		FollowerActor.SetAV("WaitingForPlayer", 0)
		SetObjectiveDisplayed(10, False)
		Return
	EndIf

	SFF().SFF_FollowActor(FollowerActor)
EndFunction

Function DismissFollower(ObjectReference akFollower, Int iMessage = 0, Int iSayLine = 1)
	Actor FollowerActor = ResolveFollower(akFollower)
	If !FollowerActor
		Return
	EndIf

	If IsLegacyFollower(FollowerActor)
		DismissLegacyFollower(FollowerActor, iMessage, iSayLine)
		Return
	EndIf

	DialogueFollowerScript sff = SFF()
	If !sff.IsManagedFollower(FollowerActor)
		Return
	EndIf

	sff.SFF_DismissActor(FollowerActor, iMessage, iSayLine)
	FollowerActor.AddToFaction(pDismissedFollower)
	HirelingRehireScript.DismissHireling(FollowerActor.GetActorBase())

	Faction excluded = BardAudienceExcluded()
	If FollowerActor.GetFactionRank(excluded) == 1
		FollowerActor.RemoveFromFaction(excluded)
	EndIf
EndFunction

Function DismissLegacyFollower(Actor DismissedFollowerActor, Int iMessage, Int iSayLine)
	If DismissedFollowerActor.IsDead()
		Return
	EndIf

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

	DismissedFollowerActor.StopCombatAlarm()
	DismissedFollowerActor.AddToFaction(pDismissedFollower)
	DismissedFollowerActor.SetPlayerTeammate(False)
	DismissedFollowerActor.RemoveFromFaction(pCurrentHireling)
	DismissedFollowerActor.SetAV("WaitingForPlayer", 0)
	HirelingRehireScript.DismissHireling(DismissedFollowerActor.GetActorBase())

	If iSayLine == 1
		iFollowerDismiss = 1
		DismissedFollowerActor.EvaluatePackage()
		Utility.Wait(2)
	EndIf

	pFollowerAlias.UnregisterForUpdateGameTime()
	pFollowerAlias.Clear()
	iFollowerDismiss = 0

	If iMessage != 2
		pPlayerFollowerCount.SetValue(0)
	EndIf
EndFunction

Function SetAnimal(ObjectReference AnimalRef)
	Actor AnimalActor = AnimalRef as Actor
	AnimalActor.SetAV("Lockpicking", 0)
	AnimalActor.SetRelationshipRank(Game.GetPlayer(), 3)
	AnimalActor.SetPlayerTeammate(True, False)
	pAnimalAlias.ForceRefTo(AnimalActor)
	pPlayerAnimalCount.SetValue(1)
EndFunction

Function AnimalWait(ObjectReference akFollower)
	Actor AnimalActor = pAnimalAlias.GetActorRef()
	AnimalActor.SetAV("WaitingForPlayer", 1)
	pAnimalAlias.RegisterForUpdateGameTime(72)
EndFunction

Function AnimalFollow(ObjectReference akFollower)
	Actor AnimalActor = pAnimalAlias.GetActorRef()
	AnimalActor.SetAV("WaitingForPlayer", 0)
	SetObjectiveDisplayed(20, False)
EndFunction

Function DismissAnimal()
	If pAnimalAlias && pAnimalAlias.GetActorRef().IsDead() == False
		Actor DismissedAnimalActor = pAnimalAlias.GetActorRef()
		DismissedAnimalActor.SetActorValue("Variable04", 0)
		pAnimalAlias.Clear()
		AnimalDismissMessage.Show()
	EndIf
EndFunction
