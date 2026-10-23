Scriptname YLIWF_Engine Hidden

Function RestoreRecruitment(Actor target, Faction potential, Int ticket) Global
	If YLIWF_SKSE.CanRestoreRecruitment(target, ticket)
		If !target.IsInFaction(potential)
			target.AddToFaction(potential)
		EndIf
	EndIf
	Int relationshipRank = 0
	If target
		relationshipRank = target.GetRelationshipRank(Game.GetPlayer())
	EndIf
	YLIWF_SKSE.RecruitmentRestored(target, ticket, relationshipRank)
EndFunction

Function Formation(Actor target, Actor leader, Int ticket, Int sessionHigh, Int sessionLow, Bool enabled, Float behind) Global
	Bool current = YLIWF_SKSE.SetNativeFormationCurrent(target, ticket, sessionHigh, sessionLow)
	If current
		; Retained only to clear offsets saved by the earlier formation preview.
		If !enabled
			target.ClearKeepOffsetFromActor()
		EndIf
	EndIf
	YLIWF_SKSE.SetNativeFormationReturned(target, ticket, sessionHigh, sessionLow, current)
EndFunction

; Mechanical adapters to vanilla engine APIs and external controllers.
; Policy, ordering, waits and receipts belong to the native controller.
; The saved VM stack reports completion after the API call returns.

Function SwapPrimary(Quest owner, Int ticket, Int offset, ReferenceAlias primary, ReferenceAlias extra, ObjectReference selected, ObjectReference previous) Global
	Bool current = YLIWF_SKSE.SetNativeEffectCurrent(owner, ticket, offset)
	If current
		primary.ForceRefTo(selected)
		If previous
			extra.ForceRefTo(previous)
		Else
			extra.Clear()
		EndIf
	EndIf
	YLIWF_SKSE.SetNativeEffectReturned(owner, ticket, offset, current)
EndFunction

Function Clear(Quest owner, Int ticket, Int offset, ReferenceAlias slot) Global
	Bool current = YLIWF_SKSE.SetNativeEffectCurrent(owner, ticket, offset)
	If current
		slot.Clear()
	EndIf
	YLIWF_SKSE.SetNativeEffectReturned(owner, ticket, offset, current)
EndFunction

Function Assign(Quest owner, Int ticket, Int offset, ReferenceAlias slot, ObjectReference target) Global
	Bool current = YLIWF_SKSE.SetNativeEffectCurrent(owner, ticket, offset)
	If current
		slot.ForceRefTo(target)
	EndIf
	YLIWF_SKSE.SetNativeEffectReturned(owner, ticket, offset, current)
EndFunction

Function Teammate(Quest owner, Int ticket, Int offset, Actor target, Bool enabled, Bool canDoFavor) Global
	Bool current = YLIWF_SKSE.SetNativeEffectCurrent(owner, ticket, offset)
	If current
		target.SetPlayerTeammate(enabled, canDoFavor)
	EndIf
	YLIWF_SKSE.SetNativeEffectReturned(owner, ticket, offset, current)
EndFunction

Function Relationship(Quest owner, Int ticket, Int offset, Actor target) Global
	Bool current = YLIWF_SKSE.SetNativeEffectCurrent(owner, ticket, offset)
	If current
		If YLIWF_SKSE.GetNativeBefriendRank(target.GetRelationshipRank(Game.GetPlayer()))
			target.SetRelationshipRank(Game.GetPlayer(), 3)
		EndIf
	EndIf
	YLIWF_SKSE.SetNativeEffectReturned(owner, ticket, offset, current)
EndFunction

Function Message(Quest owner, Int ticket, Int offset, Message notification) Global
	Bool current = YLIWF_SKSE.SetNativeEffectCurrent(owner, ticket, offset)
	If current
		notification.Show()
	EndIf
	YLIWF_SKSE.SetNativeEffectReturned(owner, ticket, offset, current)
EndFunction

Function Hireling(Quest owner, Int ticket, Int offset, Quest hirelingQuest, ActorBase target) Global
	Bool current = YLIWF_SKSE.SetNativeEffectCurrent(owner, ticket, offset)
	If current
		SetHirelingRehire controller = hirelingQuest as SetHirelingRehire
		If controller
			controller.DismissHireling(target)
		EndIf
	EndIf
	YLIWF_SKSE.SetNativeEffectReturned(owner, ticket, offset, current)
EndFunction

Function Objective(Quest owner, Int ticket, Int offset, Quest target, Int objective) Global
	Bool current = YLIWF_SKSE.SetNativeEffectCurrent(owner, ticket, offset)
	If current
		target.SetObjectiveDisplayed(objective, False)
	EndIf
	YLIWF_SKSE.SetNativeEffectReturned(owner, ticket, offset, current)
EndFunction

Function Animal(Quest owner, Int ticket, Int offset, Actor target) Global
	Bool current = YLIWF_SKSE.SetNativeEffectCurrent(owner, ticket, offset)
	If current
		target.SetRelationshipRank(Game.GetPlayer(), 3)
		target.SetPlayerTeammate(True, False)
	EndIf
	YLIWF_SKSE.SetNativeEffectReturned(owner, ticket, offset, current)
EndFunction

Function SetGlobal(Quest owner, Int ticket, Int offset, GlobalVariable target, Float value) Global
	Bool current = YLIWF_SKSE.SetNativeEffectCurrent(owner, ticket, offset)
	If current
		target.SetValue(value)
	EndIf
	YLIWF_SKSE.SetNativeEffectReturned(owner, ticket, offset, current)
EndFunction

Function Counts(Quest owner, Int ticket, Int offset, GlobalVariable vanilla, GlobalVariable party, GlobalVariable gate, Float vanillaCount, Float partyCount, Float recruitGate) Global
	Bool current = YLIWF_SKSE.SetNativeEffectCurrent(owner, ticket, offset)
	If current
		party.SetValue(partyCount)
		gate.SetValue(recruitGate)
	EndIf
	YLIWF_SKSE.SetNativeEffectReturned(owner, ticket, offset, current)
EndFunction
