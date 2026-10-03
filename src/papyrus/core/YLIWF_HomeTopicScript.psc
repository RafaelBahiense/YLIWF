Scriptname YLIWF_HomeTopicScript Extends TopicInfo Hidden

Int Property HomeAction Auto
Quest Property HomeQuest Auto
Faction Property HomeFaction Auto

Function Fragment_0(ObjectReference akSpeakerRef)
	YLIWF_SKSE.SetHome(akSpeakerRef as Actor, HomeAction, HomeQuest, HomeFaction)
EndFunction
