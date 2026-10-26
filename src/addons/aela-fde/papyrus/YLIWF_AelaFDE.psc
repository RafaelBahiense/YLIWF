Scriptname YLIWF_AelaFDE Hidden

Bool Function ReleaseFollower(Actor follower) Global
    Actor original = Game.GetFormFromFile(0x01A697, "Skyrim.esm") as Actor
    If !follower || follower != original
        Return False
    EndIf
    If !YLIWF_SKSE.IsManagedFollower(follower)
        Return True
    EndIf
    Quest owner = Game.GetFormFromFile(0x0008D4, "FDE Aela.esp") as Quest
    If !owner || !YLIWF_SKSE.HasQuestPolicy(follower, owner)
        Debug.Trace("YLIWF Aela FDE: quest release unavailable; departure was not applied.")
        Return False
    EndIf
    Return YLIWF_SKSE.ReleaseActorForQuest(follower, owner)
EndFunction
