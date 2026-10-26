;BEGIN FRAGMENT CODE - Do not edit anything between this and the end comment
;NEXT FRAGMENT INDEX 1
Scriptname aaAe_TIF__05052147 Extends TopicInfo Hidden

;BEGIN FRAGMENT Fragment_0
Function Fragment_0(ObjectReference akSpeakerRef)
Actor akSpeaker = akSpeakerRef as Actor
;BEGIN CODE
If !YLIWF_AelaFDE.ReleaseFollower(akSpeaker)
    Return
EndIf
aaAelaTGFGSO.Value = 1
aaAelaTGMessage.Show()
if aaAelaApproval.Value > aaAelaApprovalThreshold0Bad.Value + 10
(GetOwningQuest() as aaAelaApprove).ap(-10)
else
(GetOwningQuest() as aaAelaApprove).ap(-1)
aaAelaApproval.Value = aaAelaApprovalThreshold0Bad.Value + 1
endif
aaAelaLowApprovalSO.Value = 2
akSpeaker.SetRelationshipRank(PlayerRef, -1)
akSpeaker.RemoveFromFaction(PotentialFollowerFaction)
;END CODE
EndFunction
;END FRAGMENT

;END FRAGMENT CODE - Do not edit anything between this and the begin comment

GlobalVariable Property aaAelaTGFGSO  Auto

Message Property aaAelaTGMessage  Auto

GlobalVariable Property aaAelaApproval  Auto

GlobalVariable Property aaAelaApprovalThreshold0Bad  Auto

GlobalVariable Property aaAelaLowApprovalSO  Auto

Actor Property PlayerRef  Auto

Faction Property PotentialFollowerFaction  Auto
