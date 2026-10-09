using Mutagen.Bethesda.Plugins;
using Mutagen.Bethesda.Skyrim;
using Mutagen.Bethesda.Strings;

internal static partial class FollowerPlugin
{
    private static void AddQuests(SkyrimMod mod)
    {
        mod.Quests.Add(CreateDialogueFollowerQuest());
        mod.Quests.Add(CreateHomeQuest());
    }

    private static Quest CreateDialogueFollowerQuest()
    {
        var dialogueFollowerQuest = new Quest(DialogueFollowerQuestForm, SkyrimRelease.SkyrimSE)
        {
            EditorID = "DialogueFollower"
        };
        var questScriptBindings = new QuestAdapter();
        var dialogueFollowerScript = new ScriptEntry
        {
            Name = PapyrusNames.Scripts.DialogueFollower
        };
        var animalDismissMessageProperty = new ScriptObjectProperty
        {
            Name = PapyrusNames.Properties.AnimalDismissMessage,
            Object = new FormLink<ISkyrimMajorRecordGetter>(SkyrimForm(0x0FF23B))
        };
        dialogueFollowerScript.Properties.Add(animalDismissMessageProperty);
        var followerDismissMessageProperty = new ScriptObjectProperty
        {
            Name = PapyrusNames.Properties.FollowerDismissMessage,
            Object = new FormLink<ISkyrimMajorRecordGetter>(SkyrimForm(0x0FF23A))
        };
        dialogueFollowerScript.Properties.Add(followerDismissMessageProperty);
        var followerDismissMessageCompanionsProperty = new ScriptObjectProperty
        {
            Name = PapyrusNames.Properties.CompanionsDismissMessage,
            Object = new FormLink<ISkyrimMajorRecordGetter>(SkyrimForm(0x0FFCE1))
        };
        dialogueFollowerScript.Properties.Add(followerDismissMessageCompanionsProperty);
        var followerDismissMessageCompanionsFemaleProperty = new ScriptObjectProperty
        {
            Name = PapyrusNames.Properties.CompanionsFemaleDismissMessage,
            Object = new FormLink<ISkyrimMajorRecordGetter>(SkyrimForm(0x0FFCE3))
        };
        dialogueFollowerScript.Properties.Add(followerDismissMessageCompanionsFemaleProperty);
        var followerDismissMessageCompanionsMaleProperty = new ScriptObjectProperty
        {
            Name = PapyrusNames.Properties.CompanionsMaleDismissMessage,
            Object = new FormLink<ISkyrimMajorRecordGetter>(SkyrimForm(0x0FFCE2))
        };
        dialogueFollowerScript.Properties.Add(followerDismissMessageCompanionsMaleProperty);
        var followerDismissMessageWaitProperty = new ScriptObjectProperty
        {
            Name = PapyrusNames.Properties.WaitDismissMessage,
            Object = new FormLink<ISkyrimMajorRecordGetter>(SkyrimForm(0x0FFCE4))
        };
        dialogueFollowerScript.Properties.Add(followerDismissMessageWaitProperty);
        var followerDismissMessageWeddingProperty = new ScriptObjectProperty
        {
            Name = PapyrusNames.Properties.WeddingDismissMessage,
            Object = new FormLink<ISkyrimMajorRecordGetter>(SkyrimForm(0x0FFCE0))
        };
        dialogueFollowerScript.Properties.Add(followerDismissMessageWeddingProperty);
        var followerHuntingBowProperty = new ScriptObjectProperty
        {
            Name = PapyrusNames.Properties.HuntingBow,
            Object = new FormLink<ISkyrimMajorRecordGetter>(SkyrimForm(0x10E2DD))
        };
        dialogueFollowerScript.Properties.Add(followerHuntingBowProperty);
        var followerIronArrowProperty = new ScriptObjectProperty
        {
            Name = PapyrusNames.Properties.IronArrow,
            Object = new FormLink<ISkyrimMajorRecordGetter>(SkyrimForm(0x10E2DE))
        };
        dialogueFollowerScript.Properties.Add(followerIronArrowProperty);
        var hirelingRehireScriptProperty = new ScriptObjectProperty
        {
            Name = PapyrusNames.Properties.HirelingRehire,
            Object = new FormLink<ISkyrimMajorRecordGetter>(DialogueFollowerQuestForm)
        };
        dialogueFollowerScript.Properties.Add(hirelingRehireScriptProperty);
        var animalAliasProperty = new ScriptObjectProperty
        {
            Name = PapyrusNames.Properties.AnimalAlias,
            Object = new FormLink<ISkyrimMajorRecordGetter>(DialogueFollowerQuestForm),
            Alias = 1
        };
        dialogueFollowerScript.Properties.Add(animalAliasProperty);
        var currentHirelingProperty = new ScriptObjectProperty
        {
            Name = PapyrusNames.Properties.CurrentHireling,
            Object = new FormLink<ISkyrimMajorRecordGetter>(CurrentHirelingFactionForm)
        };
        dialogueFollowerScript.Properties.Add(currentHirelingProperty);
        var dismissedFollowerProperty = new ScriptObjectProperty
        {
            Name = PapyrusNames.Properties.DismissedFollower,
            Object = new FormLink<ISkyrimMajorRecordGetter>(SkyrimForm(0x05C84C))
        };
        dialogueFollowerScript.Properties.Add(dismissedFollowerProperty);
        var followerAliasProperty = new ScriptObjectProperty
        {
            Name = PapyrusNames.Properties.FollowerAlias,
            Object = new FormLink<ISkyrimMajorRecordGetter>(DialogueFollowerQuestForm),
            Alias = 0
        };
        dialogueFollowerScript.Properties.Add(followerAliasProperty);
        var playerAnimalCountProperty = new ScriptObjectProperty
        {
            Name = PapyrusNames.Properties.PlayerAnimalCount,
            Object = new FormLink<ISkyrimMajorRecordGetter>(SkyrimForm(0x05C84A))
        };
        dialogueFollowerScript.Properties.Add(playerAnimalCountProperty);
        var playerFollowerCountProperty = new ScriptObjectProperty
        {
            Name = PapyrusNames.Properties.PlayerFollowerCount,
            Object = new FormLink<ISkyrimMajorRecordGetter>(PlayerFollowerCountForm)
        };
        dialogueFollowerScript.Properties.Add(playerFollowerCountProperty);
        questScriptBindings.Scripts.Add(dialogueFollowerScript);
        var hirelingCommentScript = new ScriptEntry
        {
            Name = "HirelingCommentScript"
        };
        var gameDaysPassedProperty = new ScriptObjectProperty
        {
            Name = "GameDaysPassed",
            Object = new FormLink<ISkyrimMajorRecordGetter>(SkyrimForm(0x000039))
        };
        hirelingCommentScript.Properties.Add(gameDaysPassedProperty);
        var hirelingCommentNextAllowedProperty = new ScriptObjectProperty
        {
            Name = "HirelingCommentNextAllowed",
            Object = new FormLink<ISkyrimMajorRecordGetter>(SkyrimForm(0x104817))
        };
        hirelingCommentScript.Properties.Add(hirelingCommentNextAllowedProperty);
        questScriptBindings.Scripts.Add(hirelingCommentScript);
        var dialogueFollowerStageScript = new ScriptEntry
        {
            Name = PapyrusNames.Scripts.DialogueFollowerQuestFragment
        };
        var aliasAnimalProperty = new ScriptObjectProperty
        {
            Name = "Alias_Animal",
            Object = new FormLink<ISkyrimMajorRecordGetter>(DialogueFollowerQuestForm),
            Alias = 1
        };
        dialogueFollowerStageScript.Properties.Add(aliasAnimalProperty);
        var aliasExtraFollower01Property = new ScriptObjectProperty
        {
            Name = "Alias_ExtraFollower01",
            Object = new FormLink<ISkyrimMajorRecordGetter>(DialogueFollowerQuestForm),
            Alias = 2
        };
        dialogueFollowerStageScript.Properties.Add(aliasExtraFollower01Property);
        var aliasExtraFollower02Property = new ScriptObjectProperty
        {
            Name = "Alias_ExtraFollower02",
            Object = new FormLink<ISkyrimMajorRecordGetter>(DialogueFollowerQuestForm),
            Alias = 3
        };
        dialogueFollowerStageScript.Properties.Add(aliasExtraFollower02Property);
        var aliasExtraFollower03Property = new ScriptObjectProperty
        {
            Name = "Alias_ExtraFollower03",
            Object = new FormLink<ISkyrimMajorRecordGetter>(DialogueFollowerQuestForm),
            Alias = 4
        };
        dialogueFollowerStageScript.Properties.Add(aliasExtraFollower03Property);
        var aliasExtraFollower04Property = new ScriptObjectProperty
        {
            Name = "Alias_ExtraFollower04",
            Object = new FormLink<ISkyrimMajorRecordGetter>(DialogueFollowerQuestForm),
            Alias = 5
        };
        dialogueFollowerStageScript.Properties.Add(aliasExtraFollower04Property);
        var aliasExtraFollower05Property = new ScriptObjectProperty
        {
            Name = "Alias_ExtraFollower05",
            Object = new FormLink<ISkyrimMajorRecordGetter>(DialogueFollowerQuestForm),
            Alias = 6
        };
        dialogueFollowerStageScript.Properties.Add(aliasExtraFollower05Property);
        var aliasExtraFollower06Property = new ScriptObjectProperty
        {
            Name = "Alias_ExtraFollower06",
            Object = new FormLink<ISkyrimMajorRecordGetter>(DialogueFollowerQuestForm),
            Alias = 7
        };
        dialogueFollowerStageScript.Properties.Add(aliasExtraFollower06Property);
        var aliasExtraFollower07Property = new ScriptObjectProperty
        {
            Name = "Alias_ExtraFollower07",
            Object = new FormLink<ISkyrimMajorRecordGetter>(DialogueFollowerQuestForm),
            Alias = 8
        };
        dialogueFollowerStageScript.Properties.Add(aliasExtraFollower07Property);
        var aliasFollowerProperty = new ScriptObjectProperty
        {
            Name = "Alias_Follower",
            Object = new FormLink<ISkyrimMajorRecordGetter>(DialogueFollowerQuestForm),
            Alias = 0
        };
        dialogueFollowerStageScript.Properties.Add(aliasFollowerProperty);
        var freeformSkyhavenTempleAProperty = new ScriptObjectProperty
        {
            Name = PapyrusNames.Properties.BladesQuest,
            Object = new FormLink<ISkyrimMajorRecordGetter>(BladesQuestForm)
        };
        dialogueFollowerStageScript.Properties.Add(freeformSkyhavenTempleAProperty);
        questScriptBindings.Scripts.Add(dialogueFollowerStageScript);
        var setHirelingRehire = new ScriptEntry
        {
            Name = "SetHirelingRehire"
        };
        var belrandProperty = new ScriptObjectProperty
        {
            Name = "Belrand",
            Object = new FormLink<ISkyrimMajorRecordGetter>(SkyrimForm(0x0B9981))
        };
        setHirelingRehire.Properties.Add(belrandProperty);
        var canReHireProperty = new ScriptObjectProperty
        {
            Name = "CanReHire",
            Object = new FormLink<ISkyrimMajorRecordGetter>(SkyrimForm(0x104F35))
        };
        setHirelingRehire.Properties.Add(canReHireProperty);
        var canRehireBelrandProperty = new ScriptObjectProperty
        {
            Name = "CanRehireBelrand",
            Object = new FormLink<ISkyrimMajorRecordGetter>(SkyrimForm(0x10AB34))
        };
        setHirelingRehire.Properties.Add(canRehireBelrandProperty);
        var canRehireErikProperty = new ScriptObjectProperty
        {
            Name = "CanRehireErik",
            Object = new FormLink<ISkyrimMajorRecordGetter>(SkyrimForm(0x10AB37))
        };
        setHirelingRehire.Properties.Add(canRehireErikProperty);
        var canRehireJenassaProperty = new ScriptObjectProperty
        {
            Name = "CanRehireJenassa",
            Object = new FormLink<ISkyrimMajorRecordGetter>(SkyrimForm(0x10AB32))
        };
        setHirelingRehire.Properties.Add(canRehireJenassaProperty);
        var canRehireMarcurioProperty = new ScriptObjectProperty
        {
            Name = "CanRehireMarcurio",
            Object = new FormLink<ISkyrimMajorRecordGetter>(SkyrimForm(0x10AB33))
        };
        setHirelingRehire.Properties.Add(canRehireMarcurioProperty);
        var canRehireStenvarProperty = new ScriptObjectProperty
        {
            Name = "CanRehireStenvar",
            Object = new FormLink<ISkyrimMajorRecordGetter>(SkyrimForm(0x10AB35))
        };
        setHirelingRehire.Properties.Add(canRehireStenvarProperty);
        var canRehireVorstagProperty = new ScriptObjectProperty
        {
            Name = "CanRehireVorstag",
            Object = new FormLink<ISkyrimMajorRecordGetter>(SkyrimForm(0x10AB36))
        };
        setHirelingRehire.Properties.Add(canRehireVorstagProperty);
        var erikProperty = new ScriptObjectProperty
        {
            Name = "Erik",
            Object = new FormLink<ISkyrimMajorRecordGetter>(SkyrimForm(0x065657))
        };
        setHirelingRehire.Properties.Add(erikProperty);
        var gameDaysPassedProperty2 = new ScriptObjectProperty
        {
            Name = "GameDaysPassed",
            Object = new FormLink<ISkyrimMajorRecordGetter>(SkyrimForm(0x000039))
        };
        setHirelingRehire.Properties.Add(gameDaysPassedProperty2);
        var jenassaProperty = new ScriptObjectProperty
        {
            Name = "Jenassa",
            Object = new FormLink<ISkyrimMajorRecordGetter>(SkyrimForm(0x0B9982))
        };
        setHirelingRehire.Properties.Add(jenassaProperty);
        var marcurioProperty = new ScriptObjectProperty
        {
            Name = "Marcurio",
            Object = new FormLink<ISkyrimMajorRecordGetter>(SkyrimForm(0x0B9980))
        };
        setHirelingRehire.Properties.Add(marcurioProperty);
        var rehireWindowProperty = new ScriptIntProperty
        {
            Name = "RehireWindow",
            Data = 24
        };
        setHirelingRehire.Properties.Add(rehireWindowProperty);
        var stenvarProperty = new ScriptObjectProperty
        {
            Name = "Stenvar",
            Object = new FormLink<ISkyrimMajorRecordGetter>(SkyrimForm(0x0B9983))
        };
        setHirelingRehire.Properties.Add(stenvarProperty);
        var vorstagProperty = new ScriptObjectProperty
        {
            Name = "Vorstag",
            Object = new FormLink<ISkyrimMajorRecordGetter>(SkyrimForm(0x0B997F))
        };
        setHirelingRehire.Properties.Add(vorstagProperty);
        questScriptBindings.Scripts.Add(setHirelingRehire);
        questScriptBindings.FileName = PapyrusNames.Scripts.DialogueFollowerQuestFragment;
        var stage10Fragment = new QuestScriptFragment
        {
            Stage = 10,
            Unknown2 = (sbyte)(1),
            ScriptName = PapyrusNames.Scripts.DialogueFollowerQuestFragment,
            FragmentName = PapyrusNames.TopicFragment
        };
        questScriptBindings.Fragments.Add(stage10Fragment);
        var stage15Fragment = new QuestScriptFragment
        {
            Stage = 15,
            Unknown2 = (sbyte)(1),
            ScriptName = PapyrusNames.Scripts.DialogueFollowerQuestFragment,
            FragmentName = "Fragment_3"
        };
        questScriptBindings.Fragments.Add(stage15Fragment);
        var stage20Fragment = new QuestScriptFragment
        {
            Stage = 20,
            Unknown2 = (sbyte)(1),
            ScriptName = PapyrusNames.Scripts.DialogueFollowerQuestFragment,
            FragmentName = "Fragment_4"
        };
        questScriptBindings.Fragments.Add(stage20Fragment);
        var stage30Fragment = new QuestScriptFragment
        {
            Stage = 30,
            Unknown2 = (sbyte)(1),
            ScriptName = PapyrusNames.Scripts.DialogueFollowerQuestFragment,
            FragmentName = "Fragment_13"
        };
        questScriptBindings.Fragments.Add(stage30Fragment);
        var stage400Fragment = new QuestScriptFragment
        {
            Stage = 400,
            Unknown2 = (sbyte)(1),
            ScriptName = PapyrusNames.Scripts.DialogueFollowerQuestFragment,
            FragmentName = "Fragment_5"
        };
        questScriptBindings.Fragments.Add(stage400Fragment);
        var alias1ScriptBindings = new QuestFragmentAlias();
        var alias1Reference = new ScriptObjectProperty
        {
            Name = "",
            Object = new FormLink<ISkyrimMajorRecordGetter>(DialogueFollowerQuestForm),
            Alias = 1
        };
        alias1ScriptBindings.Property = alias1Reference;
        var alias1TrainedAnimalScript = new ScriptEntry
        {
            Name = "TrainedAnimalScript"
        };
        var alias1DialogueFollowerProperty = new ScriptObjectProperty
        {
            Name = PapyrusNames.Properties.DialogueFollower,
            Object = new FormLink<ISkyrimMajorRecordGetter>(DialogueFollowerQuestForm)
        };
        alias1TrainedAnimalScript.Properties.Add(alias1DialogueFollowerProperty);
        var alias1PlayerAnimalCountProperty = new ScriptObjectProperty
        {
            Name = "PlayerAnimalCount",
            Object = new FormLink<ISkyrimMajorRecordGetter>(SkyrimForm(0x05C84A))
        };
        alias1TrainedAnimalScript.Properties.Add(alias1PlayerAnimalCountProperty);
        alias1ScriptBindings.Scripts.Add(alias1TrainedAnimalScript);
        questScriptBindings.Aliases.Add(alias1ScriptBindings);
        dialogueFollowerQuest.VirtualMachineAdapter = questScriptBindings;
        dialogueFollowerQuest.Flags = Quest.Flag.StartGameEnabled | Quest.Flag.AllowRepeatedStages | (Quest.Flag)0x10;
        dialogueFollowerQuest.Priority = (byte)(50);
        dialogueFollowerQuest.QuestFormVersion = (byte)(65);
        dialogueFollowerQuest.Type = Quest.TypeEnum.Misc;
        dialogueFollowerQuest.Filter = "Generic\\";
        var stage10 = new QuestStage
        {
            Index = 10
        };
        var questLogEntry = new QuestLogEntry
        {
            Flags = (QuestLogEntry.Flag)0
        };
        stage10.LogEntries.Add(questLogEntry);
        dialogueFollowerQuest.Stages.Add(stage10);
        var stage15 = new QuestStage
        {
            Index = 15
        };
        var questLogEntry2 = new QuestLogEntry
        {
            Flags = (QuestLogEntry.Flag)0
        };
        stage15.LogEntries.Add(questLogEntry2);
        dialogueFollowerQuest.Stages.Add(stage15);
        var stage20 = new QuestStage
        {
            Index = 20
        };
        var questLogEntry3 = new QuestLogEntry
        {
            Flags = (QuestLogEntry.Flag)0
        };
        stage20.LogEntries.Add(questLogEntry3);
        dialogueFollowerQuest.Stages.Add(stage20);
        var stage30 = new QuestStage
        {
            Index = 30
        };
        var questLogEntry4 = new QuestLogEntry
        {
            Flags = (QuestLogEntry.Flag)0
        };
        stage30.LogEntries.Add(questLogEntry4);
        dialogueFollowerQuest.Stages.Add(stage30);
        var stage400 = new QuestStage
        {
            Index = 400
        };
        var questLogEntry5 = new QuestLogEntry
        {
            Flags = (QuestLogEntry.Flag)0
        };
        stage400.LogEntries.Add(questLogEntry5);
        dialogueFollowerQuest.Stages.Add(stage400);
        var questObjective = new QuestObjective
        {
            Index = 10,
            Flags = (QuestObjective.Flag)0,
            DisplayText = new TranslatedString(Language.English, "<Alias=Follower> is waiting for you")
        };
        var questObjectiveTarget = new QuestObjectiveTarget();
        var actorValueCondition = new ConditionFloat();
        var actorValueData = new GetActorValueConditionData
        {
            ActorValue = ActorValue.WaitingForPlayer
        };
        actorValueCondition.Data = actorValueData;
        actorValueCondition.ComparisonValue = 1.0f;
        questObjectiveTarget.Conditions.Add(actorValueCondition);
        questObjective.Targets.Add(questObjectiveTarget);
        dialogueFollowerQuest.Objectives.Add(questObjective);
        var questObjective2 = new QuestObjective
        {
            Index = 20,
            Flags = (QuestObjective.Flag)0,
            DisplayText = new TranslatedString(Language.English, "<Alias=Animal> is waiting for you")
        };
        var questObjectiveTarget2 = new QuestObjectiveTarget
        {
            AliasID = 1
        };
        var actorValueCondition2 = new ConditionFloat();
        var actorValueData2 = new GetActorValueConditionData
        {
            ActorValue = ActorValue.WaitingForPlayer
        };
        actorValueCondition2.Data = actorValueData2;
        actorValueCondition2.ComparisonValue = 1.0f;
        questObjectiveTarget2.Conditions.Add(actorValueCondition2);
        questObjective2.Targets.Add(questObjectiveTarget2);
        dialogueFollowerQuest.Objectives.Add(questObjective2);
        dialogueFollowerQuest.NextAliasID = 9;
        var followerAlias = new QuestAlias
        {
            Name = RecordNames.Aliases.Follower,
            Flags = QuestAlias.Flag.Optional | QuestAlias.Flag.StoresText | QuestAlias.Flag.Protected,
            CombatOverridePackageList = new FormLinkNullable<IFormListGetter>(SkyrimForm(0x05C852))
        };
        followerAlias.Factions.Add(new FormLink<IFactionGetter>(CurrentFollowerFactionForm));
        followerAlias.Factions.Add(new FormLink<IFactionGetter>(SkyrimForm(0x084D1B)));
        followerAlias.Factions.Add(new FormLink<IFactionGetter>(SkyrimForm(0x0750B8)));
        followerAlias.PackageData.Add(new FormLink<IPackageGetter>(OwnForm(0x000806)));
        followerAlias.PackageData.Add(new FormLink<IPackageGetter>(OwnForm(0x000803)));
        AddFollowDistancePackages(followerAlias);
        followerAlias.PackageData.Add(new FormLink<IPackageGetter>(SkyrimForm(0x05C84B)));
        followerAlias.VoiceTypes = new FormLinkNullable<IAliasVoiceTypeGetter>(FormKey.Null);
        dialogueFollowerQuest.Aliases.Add(followerAlias);
        var extraFollower01Alias = new QuestAlias
        {
            ID = 2,
            Name = RecordNames.Aliases.Extra(1),
            Flags = QuestAlias.Flag.Optional | QuestAlias.Flag.StoresText | QuestAlias.Flag.Protected,
            CombatOverridePackageList = new FormLinkNullable<IFormListGetter>(SkyrimForm(0x05C852))
        };
        extraFollower01Alias.Factions.Add(new FormLink<IFactionGetter>(CurrentFollowerFactionForm));
        extraFollower01Alias.Factions.Add(new FormLink<IFactionGetter>(SkyrimForm(0x084D1B)));
        extraFollower01Alias.Factions.Add(new FormLink<IFactionGetter>(SkyrimForm(0x0750B8)));
        extraFollower01Alias.PackageData.Add(new FormLink<IPackageGetter>(OwnForm(0x000806)));
        extraFollower01Alias.PackageData.Add(new FormLink<IPackageGetter>(OwnForm(0x000004)));
        AddFollowDistancePackages(extraFollower01Alias);
        extraFollower01Alias.PackageData.Add(new FormLink<IPackageGetter>(SkyrimForm(0x05C84B)));
        extraFollower01Alias.VoiceTypes = new FormLinkNullable<IAliasVoiceTypeGetter>(FormKey.Null);
        dialogueFollowerQuest.Aliases.Add(extraFollower01Alias);
        var extraFollower02Alias = new QuestAlias
        {
            ID = 3,
            Name = RecordNames.Aliases.Extra(2),
            Flags = QuestAlias.Flag.Optional | QuestAlias.Flag.StoresText | QuestAlias.Flag.Protected,
            CombatOverridePackageList = new FormLinkNullable<IFormListGetter>(SkyrimForm(0x05C852))
        };
        extraFollower02Alias.Factions.Add(new FormLink<IFactionGetter>(CurrentFollowerFactionForm));
        extraFollower02Alias.Factions.Add(new FormLink<IFactionGetter>(SkyrimForm(0x084D1B)));
        extraFollower02Alias.Factions.Add(new FormLink<IFactionGetter>(SkyrimForm(0x0750B8)));
        extraFollower02Alias.PackageData.Add(new FormLink<IPackageGetter>(OwnForm(0x000806)));
        extraFollower02Alias.PackageData.Add(new FormLink<IPackageGetter>(OwnForm(0x000005)));
        AddFollowDistancePackages(extraFollower02Alias);
        extraFollower02Alias.PackageData.Add(new FormLink<IPackageGetter>(SkyrimForm(0x05C84B)));
        extraFollower02Alias.VoiceTypes = new FormLinkNullable<IAliasVoiceTypeGetter>(FormKey.Null);
        dialogueFollowerQuest.Aliases.Add(extraFollower02Alias);
        var extraFollower03Alias = new QuestAlias
        {
            ID = 4,
            Name = RecordNames.Aliases.Extra(3),
            Flags = QuestAlias.Flag.Optional | QuestAlias.Flag.StoresText | QuestAlias.Flag.Protected,
            CombatOverridePackageList = new FormLinkNullable<IFormListGetter>(SkyrimForm(0x05C852))
        };
        extraFollower03Alias.Factions.Add(new FormLink<IFactionGetter>(CurrentFollowerFactionForm));
        extraFollower03Alias.Factions.Add(new FormLink<IFactionGetter>(SkyrimForm(0x084D1B)));
        extraFollower03Alias.Factions.Add(new FormLink<IFactionGetter>(SkyrimForm(0x0750B8)));
        extraFollower03Alias.PackageData.Add(new FormLink<IPackageGetter>(OwnForm(0x000806)));
        extraFollower03Alias.PackageData.Add(new FormLink<IPackageGetter>(OwnForm(0x000006)));
        AddFollowDistancePackages(extraFollower03Alias);
        extraFollower03Alias.PackageData.Add(new FormLink<IPackageGetter>(SkyrimForm(0x05C84B)));
        extraFollower03Alias.VoiceTypes = new FormLinkNullable<IAliasVoiceTypeGetter>(FormKey.Null);
        dialogueFollowerQuest.Aliases.Add(extraFollower03Alias);
        var extraFollower04Alias = new QuestAlias
        {
            ID = 5,
            Name = RecordNames.Aliases.Extra(4),
            Flags = QuestAlias.Flag.Optional | QuestAlias.Flag.StoresText | QuestAlias.Flag.Protected,
            CombatOverridePackageList = new FormLinkNullable<IFormListGetter>(SkyrimForm(0x05C852))
        };
        extraFollower04Alias.Factions.Add(new FormLink<IFactionGetter>(CurrentFollowerFactionForm));
        extraFollower04Alias.Factions.Add(new FormLink<IFactionGetter>(SkyrimForm(0x084D1B)));
        extraFollower04Alias.Factions.Add(new FormLink<IFactionGetter>(SkyrimForm(0x0750B8)));
        extraFollower04Alias.PackageData.Add(new FormLink<IPackageGetter>(OwnForm(0x000806)));
        extraFollower04Alias.PackageData.Add(new FormLink<IPackageGetter>(OwnForm(0x000007)));
        AddFollowDistancePackages(extraFollower04Alias);
        extraFollower04Alias.PackageData.Add(new FormLink<IPackageGetter>(SkyrimForm(0x05C84B)));
        extraFollower04Alias.VoiceTypes = new FormLinkNullable<IAliasVoiceTypeGetter>(FormKey.Null);
        dialogueFollowerQuest.Aliases.Add(extraFollower04Alias);
        var extraFollower05Alias = new QuestAlias
        {
            ID = 6,
            Name = RecordNames.Aliases.Extra(5),
            Flags = QuestAlias.Flag.Optional | QuestAlias.Flag.StoresText | QuestAlias.Flag.Protected,
            CombatOverridePackageList = new FormLinkNullable<IFormListGetter>(SkyrimForm(0x05C852))
        };
        extraFollower05Alias.Factions.Add(new FormLink<IFactionGetter>(CurrentFollowerFactionForm));
        extraFollower05Alias.Factions.Add(new FormLink<IFactionGetter>(SkyrimForm(0x084D1B)));
        extraFollower05Alias.Factions.Add(new FormLink<IFactionGetter>(SkyrimForm(0x0750B8)));
        extraFollower05Alias.PackageData.Add(new FormLink<IPackageGetter>(OwnForm(0x000806)));
        extraFollower05Alias.PackageData.Add(new FormLink<IPackageGetter>(OwnForm(0x000008)));
        AddFollowDistancePackages(extraFollower05Alias);
        extraFollower05Alias.PackageData.Add(new FormLink<IPackageGetter>(SkyrimForm(0x05C84B)));
        extraFollower05Alias.VoiceTypes = new FormLinkNullable<IAliasVoiceTypeGetter>(FormKey.Null);
        dialogueFollowerQuest.Aliases.Add(extraFollower05Alias);
        var extraFollower06Alias = new QuestAlias
        {
            ID = 7,
            Name = RecordNames.Aliases.Extra(6),
            Flags = QuestAlias.Flag.Optional | QuestAlias.Flag.StoresText | QuestAlias.Flag.Protected,
            CombatOverridePackageList = new FormLinkNullable<IFormListGetter>(SkyrimForm(0x05C852))
        };
        extraFollower06Alias.Factions.Add(new FormLink<IFactionGetter>(CurrentFollowerFactionForm));
        extraFollower06Alias.Factions.Add(new FormLink<IFactionGetter>(SkyrimForm(0x084D1B)));
        extraFollower06Alias.Factions.Add(new FormLink<IFactionGetter>(SkyrimForm(0x0750B8)));
        extraFollower06Alias.PackageData.Add(new FormLink<IPackageGetter>(OwnForm(0x000806)));
        extraFollower06Alias.PackageData.Add(new FormLink<IPackageGetter>(OwnForm(0x000009)));
        AddFollowDistancePackages(extraFollower06Alias);
        extraFollower06Alias.PackageData.Add(new FormLink<IPackageGetter>(SkyrimForm(0x05C84B)));
        extraFollower06Alias.VoiceTypes = new FormLinkNullable<IAliasVoiceTypeGetter>(FormKey.Null);
        dialogueFollowerQuest.Aliases.Add(extraFollower06Alias);
        var extraFollower07Alias = new QuestAlias
        {
            ID = 8,
            Name = RecordNames.Aliases.Extra(7),
            Flags = QuestAlias.Flag.Optional | QuestAlias.Flag.StoresText | QuestAlias.Flag.Protected,
            CombatOverridePackageList = new FormLinkNullable<IFormListGetter>(SkyrimForm(0x05C852))
        };
        extraFollower07Alias.Factions.Add(new FormLink<IFactionGetter>(CurrentFollowerFactionForm));
        extraFollower07Alias.Factions.Add(new FormLink<IFactionGetter>(SkyrimForm(0x084D1B)));
        extraFollower07Alias.Factions.Add(new FormLink<IFactionGetter>(SkyrimForm(0x0750B8)));
        extraFollower07Alias.PackageData.Add(new FormLink<IPackageGetter>(OwnForm(0x000806)));
        extraFollower07Alias.PackageData.Add(new FormLink<IPackageGetter>(OwnForm(0x000003)));
        AddFollowDistancePackages(extraFollower07Alias);
        extraFollower07Alias.PackageData.Add(new FormLink<IPackageGetter>(SkyrimForm(0x05C84B)));
        extraFollower07Alias.VoiceTypes = new FormLinkNullable<IAliasVoiceTypeGetter>(FormKey.Null);
        dialogueFollowerQuest.Aliases.Add(extraFollower07Alias);
        var animalAlias = new QuestAlias
        {
            ID = 1,
            Name = RecordNames.Aliases.Animal,
            Flags = QuestAlias.Flag.Optional | QuestAlias.Flag.StoresText | QuestAlias.Flag.Protected
        };
        animalAlias.Factions.Add(new FormLink<IFactionGetter>(CurrentFollowerFactionForm));
        animalAlias.Factions.Add(new FormLink<IFactionGetter>(SkyrimForm(0x084D1B)));
        animalAlias.PackageData.Add(new FormLink<IPackageGetter>(SkyrimForm(0x05C84B)));
        animalAlias.VoiceTypes = new FormLinkNullable<IAliasVoiceTypeGetter>(FormKey.Null);
        dialogueFollowerQuest.Aliases.Add(animalAlias);
        return dialogueFollowerQuest;
    }

    private static Quest CreateHomeQuest()
    {
        var homeQuest = new Quest(OwnForm(0x000961), SkyrimRelease.SkyrimSE)
        {
            EditorID = RecordNames.HomeQuest,
            Flags = Quest.Flag.StartGameEnabled | Quest.Flag.AllowRepeatedStages | (Quest.Flag)0x10,
            Priority = (byte)(10),
            Type = Quest.TypeEnum.Misc,
            NextAliasID = 16
        };
        var yliwfHome00Alias = new QuestAlias
        {
            Name = RecordNames.Aliases.Home(0),
            Flags = QuestAlias.Flag.Optional | QuestAlias.Flag.AllowReserved
        };
        yliwfHome00Alias.PackageData.Add(new FormLink<IPackageGetter>(OwnForm(0x000970)));
        yliwfHome00Alias.VoiceTypes = new FormLinkNullable<IAliasVoiceTypeGetter>(FormKey.Null);
        homeQuest.Aliases.Add(yliwfHome00Alias);
        var yliwfHome01Alias = new QuestAlias
        {
            ID = 1,
            Name = RecordNames.Aliases.Home(1),
            Flags = QuestAlias.Flag.Optional | QuestAlias.Flag.AllowReserved
        };
        yliwfHome01Alias.PackageData.Add(new FormLink<IPackageGetter>(OwnForm(0x000971)));
        yliwfHome01Alias.VoiceTypes = new FormLinkNullable<IAliasVoiceTypeGetter>(FormKey.Null);
        homeQuest.Aliases.Add(yliwfHome01Alias);
        var yliwfHome02Alias = new QuestAlias
        {
            ID = 2,
            Name = RecordNames.Aliases.Home(2),
            Flags = QuestAlias.Flag.Optional | QuestAlias.Flag.AllowReserved
        };
        yliwfHome02Alias.PackageData.Add(new FormLink<IPackageGetter>(OwnForm(0x000972)));
        yliwfHome02Alias.VoiceTypes = new FormLinkNullable<IAliasVoiceTypeGetter>(FormKey.Null);
        homeQuest.Aliases.Add(yliwfHome02Alias);
        var yliwfHome03Alias = new QuestAlias
        {
            ID = 3,
            Name = RecordNames.Aliases.Home(3),
            Flags = QuestAlias.Flag.Optional | QuestAlias.Flag.AllowReserved
        };
        yliwfHome03Alias.PackageData.Add(new FormLink<IPackageGetter>(OwnForm(0x000973)));
        yliwfHome03Alias.VoiceTypes = new FormLinkNullable<IAliasVoiceTypeGetter>(FormKey.Null);
        homeQuest.Aliases.Add(yliwfHome03Alias);
        var yliwfHome04Alias = new QuestAlias
        {
            ID = 4,
            Name = RecordNames.Aliases.Home(4),
            Flags = QuestAlias.Flag.Optional | QuestAlias.Flag.AllowReserved
        };
        yliwfHome04Alias.PackageData.Add(new FormLink<IPackageGetter>(OwnForm(0x000974)));
        yliwfHome04Alias.VoiceTypes = new FormLinkNullable<IAliasVoiceTypeGetter>(FormKey.Null);
        homeQuest.Aliases.Add(yliwfHome04Alias);
        var yliwfHome05Alias = new QuestAlias
        {
            ID = 5,
            Name = RecordNames.Aliases.Home(5),
            Flags = QuestAlias.Flag.Optional | QuestAlias.Flag.AllowReserved
        };
        yliwfHome05Alias.PackageData.Add(new FormLink<IPackageGetter>(OwnForm(0x000975)));
        yliwfHome05Alias.VoiceTypes = new FormLinkNullable<IAliasVoiceTypeGetter>(FormKey.Null);
        homeQuest.Aliases.Add(yliwfHome05Alias);
        var yliwfHome06Alias = new QuestAlias
        {
            ID = 6,
            Name = RecordNames.Aliases.Home(6),
            Flags = QuestAlias.Flag.Optional | QuestAlias.Flag.AllowReserved
        };
        yliwfHome06Alias.PackageData.Add(new FormLink<IPackageGetter>(OwnForm(0x000976)));
        yliwfHome06Alias.VoiceTypes = new FormLinkNullable<IAliasVoiceTypeGetter>(FormKey.Null);
        homeQuest.Aliases.Add(yliwfHome06Alias);
        var yliwfHome07Alias = new QuestAlias
        {
            ID = 7,
            Name = RecordNames.Aliases.Home(7),
            Flags = QuestAlias.Flag.Optional | QuestAlias.Flag.AllowReserved
        };
        yliwfHome07Alias.PackageData.Add(new FormLink<IPackageGetter>(OwnForm(0x000977)));
        yliwfHome07Alias.VoiceTypes = new FormLinkNullable<IAliasVoiceTypeGetter>(FormKey.Null);
        homeQuest.Aliases.Add(yliwfHome07Alias);
        var yliwfHomeMarkerAlias00Alias = new QuestAlias
        {
            ID = 8,
            Name = RecordNames.Aliases.HomeMarker(0),
            Flags = QuestAlias.Flag.Optional | QuestAlias.Flag.AllowReserved,
            ForcedReference = new FormLinkNullable<IPlacedGetter>(OwnForm(0x000951)),
            VoiceTypes = new FormLinkNullable<IAliasVoiceTypeGetter>(FormKey.Null)
        };
        homeQuest.Aliases.Add(yliwfHomeMarkerAlias00Alias);
        var yliwfHomeMarkerAlias01Alias = new QuestAlias
        {
            ID = 9,
            Name = RecordNames.Aliases.HomeMarker(1),
            Flags = QuestAlias.Flag.Optional | QuestAlias.Flag.AllowReserved,
            ForcedReference = new FormLinkNullable<IPlacedGetter>(OwnForm(0x000952)),
            VoiceTypes = new FormLinkNullable<IAliasVoiceTypeGetter>(FormKey.Null)
        };
        homeQuest.Aliases.Add(yliwfHomeMarkerAlias01Alias);
        var yliwfHomeMarkerAlias02Alias = new QuestAlias
        {
            ID = 10,
            Name = RecordNames.Aliases.HomeMarker(2),
            Flags = QuestAlias.Flag.Optional | QuestAlias.Flag.AllowReserved,
            ForcedReference = new FormLinkNullable<IPlacedGetter>(OwnForm(0x000953)),
            VoiceTypes = new FormLinkNullable<IAliasVoiceTypeGetter>(FormKey.Null)
        };
        homeQuest.Aliases.Add(yliwfHomeMarkerAlias02Alias);
        var yliwfHomeMarkerAlias03Alias = new QuestAlias
        {
            ID = 11,
            Name = RecordNames.Aliases.HomeMarker(3),
            Flags = QuestAlias.Flag.Optional | QuestAlias.Flag.AllowReserved,
            ForcedReference = new FormLinkNullable<IPlacedGetter>(OwnForm(0x000954)),
            VoiceTypes = new FormLinkNullable<IAliasVoiceTypeGetter>(FormKey.Null)
        };
        homeQuest.Aliases.Add(yliwfHomeMarkerAlias03Alias);
        var yliwfHomeMarkerAlias04Alias = new QuestAlias
        {
            ID = 12,
            Name = RecordNames.Aliases.HomeMarker(4),
            Flags = QuestAlias.Flag.Optional | QuestAlias.Flag.AllowReserved,
            ForcedReference = new FormLinkNullable<IPlacedGetter>(OwnForm(0x000955)),
            VoiceTypes = new FormLinkNullable<IAliasVoiceTypeGetter>(FormKey.Null)
        };
        homeQuest.Aliases.Add(yliwfHomeMarkerAlias04Alias);
        var yliwfHomeMarkerAlias05Alias = new QuestAlias
        {
            ID = 13,
            Name = RecordNames.Aliases.HomeMarker(5),
            Flags = QuestAlias.Flag.Optional | QuestAlias.Flag.AllowReserved,
            ForcedReference = new FormLinkNullable<IPlacedGetter>(OwnForm(0x000956)),
            VoiceTypes = new FormLinkNullable<IAliasVoiceTypeGetter>(FormKey.Null)
        };
        homeQuest.Aliases.Add(yliwfHomeMarkerAlias05Alias);
        var yliwfHomeMarkerAlias06Alias = new QuestAlias
        {
            ID = 14,
            Name = RecordNames.Aliases.HomeMarker(6),
            Flags = QuestAlias.Flag.Optional | QuestAlias.Flag.AllowReserved,
            ForcedReference = new FormLinkNullable<IPlacedGetter>(OwnForm(0x000957)),
            VoiceTypes = new FormLinkNullable<IAliasVoiceTypeGetter>(FormKey.Null)
        };
        homeQuest.Aliases.Add(yliwfHomeMarkerAlias06Alias);
        var yliwfHomeMarkerAlias07Alias = new QuestAlias
        {
            ID = 15,
            Name = RecordNames.Aliases.HomeMarker(7),
            Flags = QuestAlias.Flag.Optional | QuestAlias.Flag.AllowReserved,
            ForcedReference = new FormLinkNullable<IPlacedGetter>(OwnForm(0x000958)),
            VoiceTypes = new FormLinkNullable<IAliasVoiceTypeGetter>(FormKey.Null)
        };
        homeQuest.Aliases.Add(yliwfHomeMarkerAlias07Alias);
        return homeQuest;
    }
}
