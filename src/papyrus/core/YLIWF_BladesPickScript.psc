Scriptname YLIWF_BladesPickScript Extends TopicInfo Hidden

Int Property SlotIndex Auto
Quest Property FreeformSkyhavenTempleA Auto
Quest Property DialogueFollower Auto

Function Fragment_0(ObjectReference akSpeakerRef)
	YLIWF_SKSE.DebugForm("Papyrus.YLIWF_BladesPickScript.Fragment_0.enter", akSpeakerRef, "akSpeakerRef", akSpeakerRef)
	Actor chosen = YLIWF_SKSE.GetControllerBladeCandidate(DialogueFollower, SlotIndex)
	If !chosen
		YLIWF_SKSE.Debug("Papyrus.YLIWF_BladesPickScript.Fragment_0.return", akSpeakerRef, "")
		Return
	EndIf
	YLIWF_SKSE.SetNativeSpeaker(DialogueFollower, chosen, False)
	(FreeformSkyhavenTempleA as FreeformSkyHavenTempleAScript).RecruitBlade(chosen)
	YLIWF_SKSE.Debug("Papyrus.YLIWF_BladesPickScript.Fragment_0.exit", akSpeakerRef, "")
EndFunction
