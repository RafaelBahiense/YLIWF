Scriptname aaFu_TIF__05029125 extends TopicInfo Hidden

Function Fragment_0(ObjectReference akSpeakerRef)
    If !YLIWF_SKSE.HasQuestPolicy(akSpeakerRef as Actor, GetOwningQuest())
        Return
    EndIf
    YLIWF_SKSE.FollowActor(akSpeakerRef as Actor)
EndFunction
