ScriptName DialogueFollowerScript extends Quest Conditional

GlobalVariable Property pPlayerFollowerCount Auto
GlobalVariable Property pPlayerAnimalCount Auto
ReferenceAlias Property pFollowerAlias Auto
ReferenceAlias property pAnimalAlias Auto
Faction Property pDismissedFollower Auto
Faction Property pCurrentHireling Auto
Message Property  FollowerDismissMessage Auto
Message Property AnimalDismissMessage Auto
Message Property  FollowerDismissMessageWedding Auto
Message Property  FollowerDismissMessageCompanions Auto
Message Property  FollowerDismissMessageCompanionsMale Auto
Message Property  FollowerDismissMessageCompanionsFemale Auto
Message Property  FollowerDismissMessageWait Auto
SetHirelingRehire Property HirelingRehireScript Auto

;Property to tell follower to say dismissal line
Int Property iFollowerDismiss Auto Conditional

; PATCH 1.9: 77615: remove unplayable hunting bow when follower is dismissed
Weapon Property FollowerHuntingBow Auto
Ammo Property FollowerIronArrow Auto


Function SetFollower(ObjectReference FollowerRef)
	YLIWF_SKSE.SetFollower(FollowerRef)
EndFunction

Function SetAnimal(ObjectReference AnimalRef)
	YLIWF_SKSE.SetAnimal(AnimalRef)
EndFunction

Function FollowerWait()
	YLIWF_SKSE.FollowerWait()
EndFunction

Function AnimalWait()
	YLIWF_SKSE.AnimalWait()
EndFunction

Function FollowerFollow()
	YLIWF_SKSE.FollowerFollow()
EndFunction

Function AnimalFollow()
	YLIWF_SKSE.AnimalFollow()
EndFunction

Function DismissFollower(Int iMessage = 0, Int iSayLine = 1)
	YLIWF_SKSE.DismissFollower(iMessage, iSayLine)
EndFunction

Function DismissAnimal()
	YLIWF_SKSE.DismissAnimal()
EndFunction
