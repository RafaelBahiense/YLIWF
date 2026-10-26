;BEGIN FRAGMENT CODE - Do not edit anything between this and the end comment
;NEXT FRAGMENT INDEX 1
Scriptname aaAe_TIF__0505C3B9 Extends TopicInfo Hidden

;BEGIN FRAGMENT Fragment_0
Function Fragment_0(ObjectReference akSpeakerRef)
Actor akSpeaker = akSpeakerRef as Actor
;BEGIN CODE
If !YLIWF_AelaFDE.ReleaseFollower(akSpeaker)
    Return
EndIf
(GetOwningQuest() as aaAelaApprove).ap(-999)
akSpeaker.SetRelationshipRank(PlayerRef, -1)
aaAelaGrace.Value = 0
aaAelaDB03SO.Value = 3
akSpeaker.RemoveFromFaction(PotentialFollowerFaction)
;END CODE
EndFunction
;END FRAGMENT

;END FRAGMENT CODE - Do not edit anything between this and the begin comment

Actor Property PlayerRef  Auto

GlobalVariable Property aaAelaGrace  Auto

GlobalVariable Property aaAelaDB03SO  Auto

Faction Property PotentialFollowerFaction  Auto
