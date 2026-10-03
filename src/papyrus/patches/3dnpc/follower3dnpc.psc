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


Faction Function BardAudienceExcluded()
	YLIWF_SKSE.Debug("Papyrus.follower3dnpc.BardAudienceExcluded.enter", None, "")
	Faction yliwfDebugReturn1 = Game.GetFormFromFile(0x0010FCB4, "Skyrim.esm") as Faction
	YLIWF_SKSE.DebugForm("Papyrus.follower3dnpc.BardAudienceExcluded.return", None, "result", yliwfDebugReturn1)
	Return yliwfDebugReturn1
EndFunction

Actor Function ResolveFollower(ObjectReference akFollower)
	YLIWF_SKSE.DebugForm("Papyrus.follower3dnpc.ResolveFollower.enter", akFollower, "akFollower", akFollower)
	Actor a = akFollower as Actor
	If !a
		a = Game.GetDialogueTarget() as Actor
	EndIf
	YLIWF_SKSE.DebugForm("Papyrus.follower3dnpc.ResolveFollower.return", akFollower, "result", a)
	Return a
EndFunction

Bool Function IsLegacyFollower(Actor akActor)
	YLIWF_SKSE.DebugForm("Papyrus.follower3dnpc.IsLegacyFollower.enter", akActor, "akActor", akActor)
	Bool yliwfDebugReturn1 = akActor && pFollowerAlias.GetActorReference() == akActor
	YLIWF_SKSE.DebugBool("Papyrus.follower3dnpc.IsLegacyFollower.return", akActor, "result", yliwfDebugReturn1)
	Return yliwfDebugReturn1
EndFunction

Function SetFollower(ObjectReference FollowerRef)
	YLIWF_SKSE.DebugForm("Papyrus.follower3dnpc.SetFollower.enter", FollowerRef, "FollowerRef", FollowerRef)
	Actor FollowerActor = FollowerRef as Actor
	If !FollowerActor
		YLIWF_SKSE.Debug("Papyrus.follower3dnpc.SetFollower.return", FollowerRef, "")
		Return
	EndIf

	Bool legacy = IsLegacyFollower(FollowerActor)
	If legacy
		pFollowerAlias.UnregisterForUpdateGameTime()
		YLIWF_SKSE.Debug("Papyrus.follower3dnpc.SetFollower.state", FollowerRef, "pFollowerAlias.UnregisterForUpdateGameTime()")
		pFollowerAlias.Clear()
		YLIWF_SKSE.Debug("Papyrus.follower3dnpc.SetFollower.state", FollowerRef, "pFollowerAlias.Clear()")
	EndIf

	YLIWF_SKSE.SetFollower(FollowerActor)

	If !YLIWF_SKSE.IsManagedFollower(FollowerActor)
		If legacy
			pFollowerAlias.ForceRefTo(FollowerActor)
			YLIWF_SKSE.Debug("Papyrus.follower3dnpc.SetFollower.state", FollowerRef, "pFollowerAlias.ForceRefTo(FollowerActor)")
		EndIf
		YLIWF_SKSE.Debug("Papyrus.follower3dnpc.SetFollower.return", FollowerRef, "")
		Return
	EndIf

	If legacy
		pPlayerFollowerCount.SetValue(0)
		YLIWF_SKSE.Debug("Papyrus.follower3dnpc.SetFollower.state", FollowerRef, "pPlayerFollowerCount.SetValue(0)")
	EndIf

	FollowerActor.RemoveFromFaction(pDismissedFollower)
	YLIWF_SKSE.Debug("Papyrus.follower3dnpc.SetFollower.state", FollowerRef, "FollowerActor.RemoveFromFaction(pDismissedFollower)")

	Faction excluded = BardAudienceExcluded()
	If !FollowerActor.IsInFaction(excluded)
		FollowerActor.AddToFaction(excluded)
		YLIWF_SKSE.Debug("Papyrus.follower3dnpc.SetFollower.state", FollowerRef, "FollowerActor.AddToFaction(excluded)")
		FollowerActor.SetFactionRank(excluded, 1)
	EndIf
	YLIWF_SKSE.Debug("Papyrus.follower3dnpc.SetFollower.exit", FollowerRef, "")
EndFunction

Function FollowerWait(ObjectReference akFollower)
	YLIWF_SKSE.DebugForm("Papyrus.follower3dnpc.FollowerWait.enter", akFollower, "akFollower", akFollower)
	Actor FollowerActor = ResolveFollower(akFollower)
	If !FollowerActor
		YLIWF_SKSE.Debug("Papyrus.follower3dnpc.FollowerWait.return", akFollower, "")
		Return
	EndIf

	If IsLegacyFollower(FollowerActor)
		FollowerActor.SetAV("WaitingForPlayer", 1)
		YLIWF_SKSE.Debug("Papyrus.follower3dnpc.FollowerWait.state", akFollower, "FollowerActor.SetAV(\"WaitingForPlayer\", 1)")
		pFollowerAlias.RegisterForUpdateGameTime(72)
		YLIWF_SKSE.Debug("Papyrus.follower3dnpc.FollowerWait.state", akFollower, "pFollowerAlias.RegisterForUpdateGameTime(72)")
		YLIWF_SKSE.Debug("Papyrus.follower3dnpc.FollowerWait.return", akFollower, "")
		Return
	EndIf

	YLIWF_SKSE.WaitActor(FollowerActor)
	YLIWF_SKSE.Debug("Papyrus.follower3dnpc.FollowerWait.exit", akFollower, "")
EndFunction

Function FollowerFollow(ObjectReference akFollower)
	YLIWF_SKSE.DebugForm("Papyrus.follower3dnpc.FollowerFollow.enter", akFollower, "akFollower", akFollower)
	Actor FollowerActor = ResolveFollower(akFollower)
	If !FollowerActor
		YLIWF_SKSE.Debug("Papyrus.follower3dnpc.FollowerFollow.return", akFollower, "")
		Return
	EndIf

	If IsLegacyFollower(FollowerActor)
		FollowerActor.SetAV("WaitingForPlayer", 0)
		YLIWF_SKSE.Debug("Papyrus.follower3dnpc.FollowerFollow.state", akFollower, "FollowerActor.SetAV(\"WaitingForPlayer\", 0)")
		SetObjectiveDisplayed(10, False)
		YLIWF_SKSE.Debug("Papyrus.follower3dnpc.FollowerFollow.return", akFollower, "")
		Return
	EndIf

	YLIWF_SKSE.FollowActor(FollowerActor)
	YLIWF_SKSE.Debug("Papyrus.follower3dnpc.FollowerFollow.exit", akFollower, "")
EndFunction

Function DismissFollower(ObjectReference akFollower, Int iMessage = 0, Int iSayLine = 1)
	If YLIWF_SKSE.IsDebugLoggingEnabled()
		YLIWF_SKSE.Debug("Papyrus.follower3dnpc.DismissFollower.enter", akFollower, "akFollower=" + akFollower + " " + "iMessage=" + iMessage + " " + "iSayLine=" + iSayLine)
	EndIf
	Actor FollowerActor = ResolveFollower(akFollower)
	If !FollowerActor
		YLIWF_SKSE.Debug("Papyrus.follower3dnpc.DismissFollower.return", akFollower, "")
		Return
	EndIf

	If IsLegacyFollower(FollowerActor)
		DismissLegacyFollower(FollowerActor, iMessage, iSayLine)
		YLIWF_SKSE.Debug("Papyrus.follower3dnpc.DismissFollower.return", akFollower, "")
		Return
	EndIf

	If !YLIWF_SKSE.IsManagedFollower(FollowerActor)
		YLIWF_SKSE.Debug("Papyrus.follower3dnpc.DismissFollower.return", akFollower, "")
		Return
	EndIf

	YLIWF_SKSE.DismissActor(FollowerActor, iMessage, iSayLine)
	FollowerActor.AddToFaction(pDismissedFollower)
	YLIWF_SKSE.Debug("Papyrus.follower3dnpc.DismissFollower.state", akFollower, "FollowerActor.AddToFaction(pDismissedFollower)")
	HirelingRehireScript.DismissHireling(FollowerActor.GetActorBase())

	Faction excluded = BardAudienceExcluded()
	If FollowerActor.GetFactionRank(excluded) == 1
		FollowerActor.RemoveFromFaction(excluded)
		YLIWF_SKSE.Debug("Papyrus.follower3dnpc.DismissFollower.state", akFollower, "FollowerActor.RemoveFromFaction(excluded)")
	EndIf
	YLIWF_SKSE.Debug("Papyrus.follower3dnpc.DismissFollower.exit", akFollower, "")
EndFunction

Function DismissLegacyFollower(Actor DismissedFollowerActor, Int iMessage, Int iSayLine)
	If YLIWF_SKSE.IsDebugLoggingEnabled()
		YLIWF_SKSE.Debug("Papyrus.follower3dnpc.DismissLegacyFollower.enter", DismissedFollowerActor, "DismissedFollowerActor=" + DismissedFollowerActor + " " + "iMessage=" + iMessage + " " + "iSayLine=" + iSayLine)
	EndIf
	If DismissedFollowerActor.IsDead()
		YLIWF_SKSE.Debug("Papyrus.follower3dnpc.DismissLegacyFollower.return", DismissedFollowerActor, "")
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
	YLIWF_SKSE.Debug("Papyrus.follower3dnpc.DismissLegacyFollower.state", DismissedFollowerActor, "DismissedFollowerActor.AddToFaction(pDismissedFollower)")
	DismissedFollowerActor.SetPlayerTeammate(False)
	YLIWF_SKSE.Debug("Papyrus.follower3dnpc.DismissLegacyFollower.state", DismissedFollowerActor, "DismissedFollowerActor.SetPlayerTeammate(False)")
	DismissedFollowerActor.RemoveFromFaction(pCurrentHireling)
	YLIWF_SKSE.Debug("Papyrus.follower3dnpc.DismissLegacyFollower.state", DismissedFollowerActor, "DismissedFollowerActor.RemoveFromFaction(pCurrentHireling)")
	DismissedFollowerActor.SetAV("WaitingForPlayer", 0)
	YLIWF_SKSE.Debug("Papyrus.follower3dnpc.DismissLegacyFollower.state", DismissedFollowerActor, "DismissedFollowerActor.SetAV(\"WaitingForPlayer\", 0)")
	HirelingRehireScript.DismissHireling(DismissedFollowerActor.GetActorBase())

	If iSayLine == 1
		iFollowerDismiss = 1
		DismissedFollowerActor.EvaluatePackage()
		Utility.Wait(2)
	EndIf

	pFollowerAlias.UnregisterForUpdateGameTime()
	YLIWF_SKSE.Debug("Papyrus.follower3dnpc.DismissLegacyFollower.state", DismissedFollowerActor, "pFollowerAlias.UnregisterForUpdateGameTime()")
	pFollowerAlias.Clear()
	YLIWF_SKSE.Debug("Papyrus.follower3dnpc.DismissLegacyFollower.state", DismissedFollowerActor, "pFollowerAlias.Clear()")
	iFollowerDismiss = 0

	If iMessage != 2
		pPlayerFollowerCount.SetValue(0)
		YLIWF_SKSE.Debug("Papyrus.follower3dnpc.DismissLegacyFollower.state", DismissedFollowerActor, "pPlayerFollowerCount.SetValue(0)")
	EndIf
	YLIWF_SKSE.Debug("Papyrus.follower3dnpc.DismissLegacyFollower.exit", DismissedFollowerActor, "")
EndFunction

Function SetAnimal(ObjectReference AnimalRef)
	YLIWF_SKSE.DebugForm("Papyrus.follower3dnpc.SetAnimal.enter", AnimalRef, "AnimalRef", AnimalRef)
	Actor AnimalActor = AnimalRef as Actor
	AnimalActor.SetAV("Lockpicking", 0)
	YLIWF_SKSE.Debug("Papyrus.follower3dnpc.SetAnimal.state", AnimalRef, "AnimalActor.SetAV(\"Lockpicking\", 0)")
	AnimalActor.SetRelationshipRank(Game.GetPlayer(), 3)
	AnimalActor.SetPlayerTeammate(True, False)
	YLIWF_SKSE.Debug("Papyrus.follower3dnpc.SetAnimal.state", AnimalRef, "AnimalActor.SetPlayerTeammate(True, False)")
	pAnimalAlias.ForceRefTo(AnimalActor)
	YLIWF_SKSE.Debug("Papyrus.follower3dnpc.SetAnimal.state", AnimalRef, "pAnimalAlias.ForceRefTo(AnimalActor)")
	pPlayerAnimalCount.SetValue(1)
	YLIWF_SKSE.Debug("Papyrus.follower3dnpc.SetAnimal.state", AnimalRef, "pPlayerAnimalCount.SetValue(1)")
	YLIWF_SKSE.Debug("Papyrus.follower3dnpc.SetAnimal.exit", AnimalRef, "")
EndFunction

Function AnimalWait(ObjectReference akFollower)
	YLIWF_SKSE.DebugForm("Papyrus.follower3dnpc.AnimalWait.enter", akFollower, "akFollower", akFollower)
	Actor AnimalActor = pAnimalAlias.GetActorRef()
	AnimalActor.SetAV("WaitingForPlayer", 1)
	YLIWF_SKSE.Debug("Papyrus.follower3dnpc.AnimalWait.state", akFollower, "AnimalActor.SetAV(\"WaitingForPlayer\", 1)")
	pAnimalAlias.RegisterForUpdateGameTime(72)
	YLIWF_SKSE.Debug("Papyrus.follower3dnpc.AnimalWait.state", akFollower, "pAnimalAlias.RegisterForUpdateGameTime(72)")
	YLIWF_SKSE.Debug("Papyrus.follower3dnpc.AnimalWait.exit", akFollower, "")
EndFunction

Function AnimalFollow(ObjectReference akFollower)
	YLIWF_SKSE.DebugForm("Papyrus.follower3dnpc.AnimalFollow.enter", akFollower, "akFollower", akFollower)
	Actor AnimalActor = pAnimalAlias.GetActorRef()
	AnimalActor.SetAV("WaitingForPlayer", 0)
	YLIWF_SKSE.Debug("Papyrus.follower3dnpc.AnimalFollow.state", akFollower, "AnimalActor.SetAV(\"WaitingForPlayer\", 0)")
	SetObjectiveDisplayed(20, False)
	YLIWF_SKSE.Debug("Papyrus.follower3dnpc.AnimalFollow.exit", akFollower, "")
EndFunction

Function DismissAnimal()
	YLIWF_SKSE.Debug("Papyrus.follower3dnpc.DismissAnimal.enter", None, "")
	If pAnimalAlias && pAnimalAlias.GetActorRef().IsDead() == False
		Actor DismissedAnimalActor = pAnimalAlias.GetActorRef()
		DismissedAnimalActor.SetActorValue("Variable04", 0)
		YLIWF_SKSE.Debug("Papyrus.follower3dnpc.DismissAnimal.state", None, "DismissedAnimalActor.SetActorValue(\"Variable04\", 0)")
		pAnimalAlias.Clear()
		YLIWF_SKSE.Debug("Papyrus.follower3dnpc.DismissAnimal.state", None, "pAnimalAlias.Clear()")
		AnimalDismissMessage.Show()
	EndIf
	YLIWF_SKSE.Debug("Papyrus.follower3dnpc.DismissAnimal.exit", None, "")
EndFunction
