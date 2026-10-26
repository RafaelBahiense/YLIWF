;BEGIN FRAGMENT CODE - Do not edit anything between this and the end comment
;NEXT FRAGMENT INDEX 2
Scriptname aaAe_TIF__0502E64F Extends TopicInfo Hidden

;BEGIN FRAGMENT Fragment_1
Function Fragment_1(ObjectReference akSpeakerRef)
Actor akSpeaker = akSpeakerRef as Actor
;BEGIN CODE
If !YLIWF_AelaFDE.ReleaseFollower(akSpeaker)
    Return
EndIf
akSpeaker.RemoveFromFaction(PotentialFollowerFaction)
akSpeaker.SetRelationshipRank(PlayerRef, -1)
(GetOwningQuest() as aaAelaApprove).ap(-100)
;END CODE
EndFunction
;END FRAGMENT

;END FRAGMENT CODE - Do not edit anything between this and the begin comment

Faction Property PotentialFollowerFaction  Auto

Actor Property PlayerRef  Auto
