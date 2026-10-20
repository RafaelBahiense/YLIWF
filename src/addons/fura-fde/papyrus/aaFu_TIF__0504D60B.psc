Scriptname aaFu_TIF__0504D60B extends TopicInfo Hidden

Function Fragment_0(ObjectReference akSpeakerRef)
    Actor fura = Game.GetFormFromFile(0x0033B2, "Dawnguard.esm") as Actor
    Quest controller = Game.GetFormFromFile(0x000810, "FDE Fura.esp") as Quest
    If akSpeakerRef != fura || !YLIWF_SKSE.HasQuestPolicy(fura, controller)
        Debug.Trace("YLIWF Fura FDE: vengeance departure bindings unavailable; quest was not advanced.")
        Return
    EndIf

    ; The dialogue belongs to aaFuraQ2, not aaFuraFollowerScript's quest.
    GetOwningQuest().SetStage(10)
    If !YLIWF_SKSE.ReleaseActorForQuest(fura, controller)
        Debug.Trace("YLIWF Fura FDE: vengeance departure failed; do not replay the dialogue automatically.")
    EndIf
EndFunction
