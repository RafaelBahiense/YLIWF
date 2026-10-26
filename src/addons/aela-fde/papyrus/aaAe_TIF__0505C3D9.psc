;BEGIN FRAGMENT CODE - Do not edit anything between this and the end comment
;NEXT FRAGMENT INDEX 1
Scriptname aaAe_TIF__0505C3D9 Extends TopicInfo Hidden

;BEGIN FRAGMENT Fragment_0
Function Fragment_0(ObjectReference akSpeakerRef)
Actor akSpeaker = akSpeakerRef as Actor
;BEGIN CODE
If !YLIWF_AelaFDE.ReleaseFollower(akSpeaker)
    Return
EndIf
akSpeaker.SetRelationshipRank(PlayerRef, 0)
aaAelaGrace.Value = 0
akSpeaker.RemoveFromFaction(PotentialFollowerFaction)
aaAelaDB04SO.Value = 1
;END CODE
EndFunction
;END FRAGMENT

;END FRAGMENT CODE - Do not edit anything between this and the begin comment

Actor Property PlayerRef  Auto

GlobalVariable Property aaAelaGrace  Auto

Faction Property PotentialFollowerFaction  Auto

GlobalVariable Property aaAelaDB04SO  Auto
