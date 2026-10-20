Scriptname aaFu_TIF__05029129 extends TopicInfo Hidden

Function Fragment_0(ObjectReference akSpeakerRef)
    If !YLIWF_SKSE.HasQuestPolicy(akSpeakerRef as Actor, GetOwningQuest())
        Return
    EndIf
    YLIWF_SKSE.WaitActor(akSpeakerRef as Actor)
EndFunction
