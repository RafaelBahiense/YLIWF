Scriptname YLIWF_SKSE Hidden

; Vanilla follower interface (latent: returns after verified completion).
Bool Function SetFollower(ObjectReference follower) Global Native
Bool Function SetAnimal(ObjectReference animal) Global Native
Bool Function FollowerWait() Global Native
Bool Function AnimalWait() Global Native
Bool Function FollowerFollow() Global Native
Bool Function AnimalFollow() Global Native
Bool Function DismissFollower(Int messageType = 0, Int sayLine = 1) Global Native
Bool Function DismissAnimal() Global Native

; Follower integrations and dialogue fragments.
Bool Function DismissActor(Actor follower, Int messageType = 0, Int sayLine = 1) Global Native
Bool Function WaitActor(Actor follower) Global Native
Bool Function FollowActor(Actor follower) Global Native
Bool Function IsManagedFollower(Actor follower) Global Native
Bool Function SetHome(Actor follower, Int action, Quest homeQuest, Faction homeFaction) Global Native
Actor Function GetControllerBladeCandidate(Quest owner, Int aliasID) Global Native
Function SetNativeSpeaker(Quest owner, Actor target, Bool clear) Global Native

; Engine adapter validation and completion.
Bool Function SetNativeEffectCurrent(Quest owner, Int ticket, Int offset) Global Native
Function SetNativeEffectReturned(Quest owner, Int ticket, Int offset, Bool succeeded) Global Native
Bool Function GetNativeBefriendRank(Int rank) Global Native

; Flow logging.
Bool Function IsDebugLoggingEnabled() Global Native
Function Debug(String eventName, Form subject = None, String detail = "") Global Native
Function DebugInt(String eventName, Form subject, String name, Int value, String secondName = "", Int secondValue = 0) Global Native
Function DebugBool(String eventName, Form subject, String name, Bool value) Global Native
Function DebugForm(String eventName, Form subject, String name, Form value) Global Native
