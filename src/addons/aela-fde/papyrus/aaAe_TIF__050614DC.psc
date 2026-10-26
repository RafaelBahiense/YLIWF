;BEGIN FRAGMENT CODE - Do not edit anything between this and the end comment
;NEXT FRAGMENT INDEX 1
Scriptname aaAe_TIF__050614DC Extends TopicInfo Hidden

;BEGIN FRAGMENT Fragment_0
Function Fragment_0(ObjectReference akSpeakerRef)
Actor akSpeaker = akSpeakerRef as Actor
;BEGIN CODE
If !YLIWF_AelaFDE.ReleaseFollower(akSpeaker)
    Return
EndIf
(GetOwningQuest() as aaAelaApprove).AelaCombat()
PlayerRef.RemoveSpell(aaAelaWerewolfSpell)
akSpeaker.RemoveFromFaction(PotentialFollowerFaction)
;END CODE
EndFunction
;END FRAGMENT

;END FRAGMENT CODE - Do not edit anything between this and the begin comment

SPELL Property aaAelaWerewolfSpell  Auto

Actor Property PlayerRef  Auto

Faction Property PotentialFollowerFaction  Auto
