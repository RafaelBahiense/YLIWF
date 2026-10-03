using Mutagen.Bethesda.Plugins;
using Mutagen.Bethesda.Skyrim;
using Mutagen.Bethesda.Strings;
using Noggog;

internal static partial class FollowerPlugin
{
    private static void AddDialogTopics(SkyrimMod mod)
    {
        mod.DialogTopics.Add(CreateFreeformSkyHavenTempleAFollowerTopic());
        mod.DialogTopics.Add(CreateBladesPick00Topic());
        mod.DialogTopics.Add(CreateBladesPick01Topic());
        mod.DialogTopics.Add(CreateBladesPick02Topic());
        mod.DialogTopics.Add(CreateBladesPick03Topic());
        mod.DialogTopics.Add(CreateBladesPick04Topic());
        mod.DialogTopics.Add(CreateBladesPick05Topic());
        mod.DialogTopics.Add(CreateBladesPick06Topic());
        mod.DialogTopics.Add(CreateBladesPick07Topic());
        mod.DialogTopics.Add(CreateHomeAssignTopic());
        mod.DialogTopics.Add(CreateHomeClearTopic());
    }

    private static DialogTopic CreateFreeformSkyHavenTempleAFollowerTopic()
    {
        var freeformSkyHavenTempleAFollowerTopic = new DialogTopic(SkyrimForm(0x0E38BC), SkyrimRelease.SkyrimSE)
        {
            EditorID = "FreeformSkyHavenTempleAFollowerTopic",
            Name = new TranslatedString(Language.English, "I brought someone to induct into the Blades."),
            Priority = 80.0f,
            Branch = new FormLinkNullable<IDialogBranchGetter>(SkyrimForm(0x0E38CC)),
            Quest = new FormLinkNullable<IQuestGetter>(DialogueFollowerQuestForm),
            SubtypeName = new RecordType(CustomTopicSubtype)
        };
        var topicResponses = new DialogResponses(SkyrimForm(0x0E38C0), SkyrimRelease.SkyrimSE);
        var responseFlags = new DialogResponseFlags();
        topicResponses.Flags = responseFlags;
        topicResponses.PreviousDialog = new FormLinkNullable<IDialogResponsesGetter>(FormKey.Null);
        topicResponses.FavorLevel = FavorLevel.None;
        topicResponses.LinkTo.Add(new FormLink<IDialogTopicGetter>(OwnForm(0x000911)));
        topicResponses.LinkTo.Add(new FormLink<IDialogTopicGetter>(OwnForm(0x000912)));
        topicResponses.LinkTo.Add(new FormLink<IDialogTopicGetter>(OwnForm(0x000913)));
        topicResponses.LinkTo.Add(new FormLink<IDialogTopicGetter>(OwnForm(0x000914)));
        topicResponses.LinkTo.Add(new FormLink<IDialogTopicGetter>(OwnForm(0x000915)));
        topicResponses.LinkTo.Add(new FormLink<IDialogTopicGetter>(OwnForm(0x000916)));
        topicResponses.LinkTo.Add(new FormLink<IDialogTopicGetter>(OwnForm(0x000917)));
        topicResponses.LinkTo.Add(new FormLink<IDialogTopicGetter>(OwnForm(0x000918)));
        topicResponses.LinkTo.Add(new FormLink<IDialogTopicGetter>(SkyrimForm(0x0E38BA)));
        var spokenResponse = new DialogResponse
        {
            EmotionValue = 50,
            ResponseNumber = (byte)(1),
            Unknown2 = new MemorySlice<byte>(Convert.FromHexString("000000")),
            Flags = DialogResponse.Flag.UseEmotionAnimation,
            Unknown3 = new MemorySlice<byte>(Convert.FromHexString("000000")),
            Text = new TranslatedString(Language.English, "Are you sure? I'll need to ask them to take an oath to leave their old life behind and stay here from now on."),
            ScriptNotes = "",
            Edits = ""
        };
        topicResponses.Responses.Add(spokenResponse);
        var inFactionCondition = new ConditionFloat();
        var inFactionData = new GetInFactionConditionData
        {
            RunOnType = Condition.RunOnType.QuestAlias,
            RunOnTypeIndex = 0
        };
        inFactionData.Faction.Link.SetTo(SkyrimForm(0x072834));
        inFactionCondition.Data = inFactionData;
        topicResponses.Conditions.Add(inFactionCondition);
        var vmQuestVariableCondition = new ConditionFloat();
        var vmQuestVariableData = new GetVMQuestVariableConditionData();
        vmQuestVariableData.Quest.Link.SetTo(BladesQuestForm);
        vmQuestVariableData.VariableName = "::BladesCount_var";
        vmQuestVariableCondition.Data = vmQuestVariableData;
        topicResponses.Conditions.Add(vmQuestVariableCondition);
        var globalValueCondition = new ConditionFloat
        {
            CompareOperator = CompareOperator.GreaterThanOrEqualTo
        };
        var globalValueData = new GetGlobalValueConditionData();
        globalValueData.Global.Link.SetTo(PlayerFollowerCountForm);
        globalValueCondition.Data = globalValueData;
        globalValueCondition.ComparisonValue = 1.0f;
        topicResponses.Conditions.Add(globalValueCondition);
        var isIDCondition = new ConditionFloat();
        var isIDData = new GetIsIDConditionData();
        isIDData.Object.Link.SetTo(SkyrimForm(0x013478));
        isIDCondition.Data = isIDData;
        isIDCondition.ComparisonValue = 1.0f;
        topicResponses.Conditions.Add(isIDCondition);
        var stageDoneCondition = new ConditionFloat();
        var stageDoneData = new GetStageDoneConditionData();
        stageDoneData.Quest.Link.SetTo(BladesQuestForm);
        stageDoneData.Stage = 10;
        stageDoneCondition.Data = stageDoneData;
        stageDoneCondition.ComparisonValue = 1.0f;
        topicResponses.Conditions.Add(stageDoneCondition);
        var stageDoneCondition2 = new ConditionFloat();
        var stageDoneData2 = new GetStageDoneConditionData();
        stageDoneData2.Quest.Link.SetTo(BladesQuestForm);
        stageDoneData2.Stage = 20;
        stageDoneCondition2.Data = stageDoneData2;
        topicResponses.Conditions.Add(stageDoneCondition2);
        freeformSkyHavenTempleAFollowerTopic.Responses.Add(topicResponses);
        var topicResponses2 = new DialogResponses(SkyrimForm(0x0E67F6), SkyrimRelease.SkyrimSE);
        var responseFlags2 = new DialogResponseFlags();
        topicResponses2.Flags = responseFlags2;
        topicResponses2.PreviousDialog = new FormLinkNullable<IDialogResponsesGetter>(SkyrimForm(0x0E38C0));
        topicResponses2.FavorLevel = FavorLevel.None;
        topicResponses2.LinkTo.Add(new FormLink<IDialogTopicGetter>(OwnForm(0x000911)));
        topicResponses2.LinkTo.Add(new FormLink<IDialogTopicGetter>(OwnForm(0x000912)));
        topicResponses2.LinkTo.Add(new FormLink<IDialogTopicGetter>(OwnForm(0x000913)));
        topicResponses2.LinkTo.Add(new FormLink<IDialogTopicGetter>(OwnForm(0x000914)));
        topicResponses2.LinkTo.Add(new FormLink<IDialogTopicGetter>(OwnForm(0x000915)));
        topicResponses2.LinkTo.Add(new FormLink<IDialogTopicGetter>(OwnForm(0x000916)));
        topicResponses2.LinkTo.Add(new FormLink<IDialogTopicGetter>(OwnForm(0x000917)));
        topicResponses2.LinkTo.Add(new FormLink<IDialogTopicGetter>(OwnForm(0x000918)));
        topicResponses2.LinkTo.Add(new FormLink<IDialogTopicGetter>(SkyrimForm(0x0E38BA)));
        var spokenResponse2 = new DialogResponse
        {
            EmotionValue = 50,
            ResponseNumber = (byte)(1),
            Unknown2 = new MemorySlice<byte>(Convert.FromHexString("000000")),
            Flags = DialogResponse.Flag.UseEmotionAnimation,
            Unknown3 = new MemorySlice<byte>(Convert.FromHexString("000000")),
            Text = new TranslatedString(Language.English, "You want this one to become a Blade? You're sure?"),
            ScriptNotes = "",
            Edits = ""
        };
        topicResponses2.Responses.Add(spokenResponse2);
        var inFactionCondition2 = new ConditionFloat();
        var inFactionData2 = new GetInFactionConditionData
        {
            RunOnType = Condition.RunOnType.QuestAlias,
            RunOnTypeIndex = 0
        };
        inFactionData2.Faction.Link.SetTo(SkyrimForm(0x072834));
        inFactionCondition2.Data = inFactionData2;
        topicResponses2.Conditions.Add(inFactionCondition2);
        var vmQuestVariableCondition2 = new ConditionFloat
        {
            CompareOperator = CompareOperator.LessThan
        };
        var vmQuestVariableData2 = new GetVMQuestVariableConditionData();
        vmQuestVariableData2.Quest.Link.SetTo(BladesQuestForm);
        vmQuestVariableData2.VariableName = "::BladesCount_var";
        vmQuestVariableCondition2.Data = vmQuestVariableData2;
        vmQuestVariableCondition2.ComparisonValue = 3.0f;
        topicResponses2.Conditions.Add(vmQuestVariableCondition2);
        var globalValueCondition2 = new ConditionFloat
        {
            CompareOperator = CompareOperator.GreaterThanOrEqualTo
        };
        var globalValueData2 = new GetGlobalValueConditionData();
        globalValueData2.Global.Link.SetTo(PlayerFollowerCountForm);
        globalValueCondition2.Data = globalValueData2;
        globalValueCondition2.ComparisonValue = 1.0f;
        topicResponses2.Conditions.Add(globalValueCondition2);
        var isIDCondition2 = new ConditionFloat();
        var isIDData2 = new GetIsIDConditionData();
        isIDData2.Object.Link.SetTo(SkyrimForm(0x013478));
        isIDCondition2.Data = isIDData2;
        isIDCondition2.ComparisonValue = 1.0f;
        topicResponses2.Conditions.Add(isIDCondition2);
        var stageDoneCondition3 = new ConditionFloat();
        var stageDoneData3 = new GetStageDoneConditionData();
        stageDoneData3.Quest.Link.SetTo(BladesQuestForm);
        stageDoneData3.Stage = 10;
        stageDoneCondition3.Data = stageDoneData3;
        stageDoneCondition3.ComparisonValue = 1.0f;
        topicResponses2.Conditions.Add(stageDoneCondition3);
        var stageDoneCondition4 = new ConditionFloat();
        var stageDoneData4 = new GetStageDoneConditionData();
        stageDoneData4.Quest.Link.SetTo(BladesQuestForm);
        stageDoneData4.Stage = 40;
        stageDoneCondition4.Data = stageDoneData4;
        topicResponses2.Conditions.Add(stageDoneCondition4);
        freeformSkyHavenTempleAFollowerTopic.Responses.Add(topicResponses2);
        return freeformSkyHavenTempleAFollowerTopic;
    }

    private static DialogTopic CreateBladesPick00Topic()
    {
        var bladesPick00Topic = new DialogTopic(OwnForm(0x000911), SkyrimRelease.SkyrimSE)
        {
            EditorID = "YLIWF_BladesPick00Topic",
            Name = new TranslatedString(Language.English, "Send <Alias=Follower> to join the Blades."),
            Branch = new FormLinkNullable<IDialogBranchGetter>(OwnForm(0x000900)),
            Quest = new FormLinkNullable<IQuestGetter>(DialogueFollowerQuestForm),
            SubtypeName = new RecordType(CustomTopicSubtype)
        };
        var topicResponses = new DialogResponses(OwnForm(0x000921), SkyrimRelease.SkyrimSE);
        var responseScriptBindings = new DialogResponsesAdapter();
        var yliwfBladesPickScript = new ScriptEntry
        {
            Name = PapyrusNames.Scripts.BladesPick
        };
        var dialogueFollowerProperty = new ScriptObjectProperty
        {
            Name = PapyrusNames.Properties.DialogueFollower,
            Object = new FormLink<ISkyrimMajorRecordGetter>(DialogueFollowerQuestForm)
        };
        yliwfBladesPickScript.Properties.Add(dialogueFollowerProperty);
        var freeformSkyhavenTempleAProperty = new ScriptObjectProperty
        {
            Name = PapyrusNames.Properties.BladesQuest,
            Object = new FormLink<ISkyrimMajorRecordGetter>(BladesQuestForm)
        };
        yliwfBladesPickScript.Properties.Add(freeformSkyhavenTempleAProperty);
        var slotIndexProperty = new ScriptIntProperty
        {
            Name = PapyrusNames.Properties.SlotIndex
        };
        yliwfBladesPickScript.Properties.Add(slotIndexProperty);
        responseScriptBindings.Scripts.Add(yliwfBladesPickScript);
        var scriptFragments = new ScriptFragments
        {
            FileName = PapyrusNames.Scripts.BladesPick
        };
        var scriptFragment = new ScriptFragment
        {
            ExtraBindDataVersion = (sbyte)(1),
            ScriptName = PapyrusNames.Scripts.BladesPick,
            FragmentName = PapyrusNames.TopicFragment
        };
        scriptFragments.OnBegin = scriptFragment;
        responseScriptBindings.ScriptFragments = scriptFragments;
        topicResponses.VirtualMachineAdapter = responseScriptBindings;
        var responseFlags = new DialogResponseFlags();
        topicResponses.Flags = responseFlags;
        topicResponses.PreviousDialog = new FormLinkNullable<IDialogResponsesGetter>(FormKey.Null);
        topicResponses.FavorLevel = FavorLevel.None;
        topicResponses.ResponseData = new FormLinkNullable<IDialogResponsesGetter>(SkyrimForm(0x0E38C4));
        var isIDCondition = new ConditionFloat();
        var isIDData = new GetIsIDConditionData();
        isIDData.Object.Link.SetTo(SkyrimForm(0x013478));
        isIDCondition.Data = isIDData;
        isIDCondition.ComparisonValue = 1.0f;
        topicResponses.Conditions.Add(isIDCondition);
        var inFactionCondition = new ConditionFloat();
        var inFactionData = new GetInFactionConditionData
        {
            RunOnType = Condition.RunOnType.QuestAlias,
            RunOnTypeIndex = 0
        };
        inFactionData.Faction.Link.SetTo(CurrentFollowerFactionForm);
        inFactionCondition.Data = inFactionData;
        inFactionCondition.ComparisonValue = 1.0f;
        topicResponses.Conditions.Add(inFactionCondition);
        var inFactionCondition2 = new ConditionFloat();
        var inFactionData2 = new GetInFactionConditionData
        {
            RunOnType = Condition.RunOnType.QuestAlias,
            RunOnTypeIndex = 0
        };
        inFactionData2.Faction.Link.SetTo(SkyrimForm(0x072834));
        inFactionCondition2.Data = inFactionData2;
        topicResponses.Conditions.Add(inFactionCondition2);
        bladesPick00Topic.Responses.Add(topicResponses);
        return bladesPick00Topic;
    }

    private static DialogTopic CreateBladesPick01Topic()
    {
        var bladesPick01Topic = new DialogTopic(OwnForm(0x000912), SkyrimRelease.SkyrimSE)
        {
            EditorID = "YLIWF_BladesPick01Topic",
            Name = new TranslatedString(Language.English, "Send <Alias=ExtraFollower01> to join the Blades."),
            Branch = new FormLinkNullable<IDialogBranchGetter>(OwnForm(0x000900)),
            Quest = new FormLinkNullable<IQuestGetter>(DialogueFollowerQuestForm),
            SubtypeName = new RecordType(CustomTopicSubtype)
        };
        var topicResponses = new DialogResponses(OwnForm(0x000922), SkyrimRelease.SkyrimSE);
        var responseScriptBindings = new DialogResponsesAdapter();
        var yliwfBladesPickScript = new ScriptEntry
        {
            Name = PapyrusNames.Scripts.BladesPick
        };
        var dialogueFollowerProperty = new ScriptObjectProperty
        {
            Name = PapyrusNames.Properties.DialogueFollower,
            Object = new FormLink<ISkyrimMajorRecordGetter>(DialogueFollowerQuestForm)
        };
        yliwfBladesPickScript.Properties.Add(dialogueFollowerProperty);
        var freeformSkyhavenTempleAProperty = new ScriptObjectProperty
        {
            Name = PapyrusNames.Properties.BladesQuest,
            Object = new FormLink<ISkyrimMajorRecordGetter>(BladesQuestForm)
        };
        yliwfBladesPickScript.Properties.Add(freeformSkyhavenTempleAProperty);
        var slotIndexProperty = new ScriptIntProperty
        {
            Name = PapyrusNames.Properties.SlotIndex,
            Data = 2
        };
        yliwfBladesPickScript.Properties.Add(slotIndexProperty);
        responseScriptBindings.Scripts.Add(yliwfBladesPickScript);
        var scriptFragments = new ScriptFragments
        {
            FileName = PapyrusNames.Scripts.BladesPick
        };
        var scriptFragment = new ScriptFragment
        {
            ExtraBindDataVersion = (sbyte)(1),
            ScriptName = PapyrusNames.Scripts.BladesPick,
            FragmentName = PapyrusNames.TopicFragment
        };
        scriptFragments.OnBegin = scriptFragment;
        responseScriptBindings.ScriptFragments = scriptFragments;
        topicResponses.VirtualMachineAdapter = responseScriptBindings;
        var responseFlags = new DialogResponseFlags();
        topicResponses.Flags = responseFlags;
        topicResponses.PreviousDialog = new FormLinkNullable<IDialogResponsesGetter>(FormKey.Null);
        topicResponses.FavorLevel = FavorLevel.None;
        topicResponses.ResponseData = new FormLinkNullable<IDialogResponsesGetter>(SkyrimForm(0x0E38C4));
        var isIDCondition = new ConditionFloat();
        var isIDData = new GetIsIDConditionData();
        isIDData.Object.Link.SetTo(SkyrimForm(0x013478));
        isIDCondition.Data = isIDData;
        isIDCondition.ComparisonValue = 1.0f;
        topicResponses.Conditions.Add(isIDCondition);
        var inFactionCondition = new ConditionFloat();
        var inFactionData = new GetInFactionConditionData
        {
            RunOnType = Condition.RunOnType.QuestAlias,
            RunOnTypeIndex = 2
        };
        inFactionData.Faction.Link.SetTo(CurrentFollowerFactionForm);
        inFactionCondition.Data = inFactionData;
        inFactionCondition.ComparisonValue = 1.0f;
        topicResponses.Conditions.Add(inFactionCondition);
        var inFactionCondition2 = new ConditionFloat();
        var inFactionData2 = new GetInFactionConditionData
        {
            RunOnType = Condition.RunOnType.QuestAlias,
            RunOnTypeIndex = 2
        };
        inFactionData2.Faction.Link.SetTo(SkyrimForm(0x072834));
        inFactionCondition2.Data = inFactionData2;
        topicResponses.Conditions.Add(inFactionCondition2);
        bladesPick01Topic.Responses.Add(topicResponses);
        return bladesPick01Topic;
    }

    private static DialogTopic CreateBladesPick02Topic()
    {
        var bladesPick02Topic = new DialogTopic(OwnForm(0x000913), SkyrimRelease.SkyrimSE)
        {
            EditorID = "YLIWF_BladesPick02Topic",
            Name = new TranslatedString(Language.English, "Send <Alias=ExtraFollower02> to join the Blades."),
            Branch = new FormLinkNullable<IDialogBranchGetter>(OwnForm(0x000900)),
            Quest = new FormLinkNullable<IQuestGetter>(DialogueFollowerQuestForm),
            SubtypeName = new RecordType(CustomTopicSubtype)
        };
        var topicResponses = new DialogResponses(OwnForm(0x000923), SkyrimRelease.SkyrimSE);
        var responseScriptBindings = new DialogResponsesAdapter();
        var yliwfBladesPickScript = new ScriptEntry
        {
            Name = PapyrusNames.Scripts.BladesPick
        };
        var dialogueFollowerProperty = new ScriptObjectProperty
        {
            Name = PapyrusNames.Properties.DialogueFollower,
            Object = new FormLink<ISkyrimMajorRecordGetter>(DialogueFollowerQuestForm)
        };
        yliwfBladesPickScript.Properties.Add(dialogueFollowerProperty);
        var freeformSkyhavenTempleAProperty = new ScriptObjectProperty
        {
            Name = PapyrusNames.Properties.BladesQuest,
            Object = new FormLink<ISkyrimMajorRecordGetter>(BladesQuestForm)
        };
        yliwfBladesPickScript.Properties.Add(freeformSkyhavenTempleAProperty);
        var slotIndexProperty = new ScriptIntProperty
        {
            Name = PapyrusNames.Properties.SlotIndex,
            Data = 3
        };
        yliwfBladesPickScript.Properties.Add(slotIndexProperty);
        responseScriptBindings.Scripts.Add(yliwfBladesPickScript);
        var scriptFragments = new ScriptFragments
        {
            FileName = PapyrusNames.Scripts.BladesPick
        };
        var scriptFragment = new ScriptFragment
        {
            ExtraBindDataVersion = (sbyte)(1),
            ScriptName = PapyrusNames.Scripts.BladesPick,
            FragmentName = PapyrusNames.TopicFragment
        };
        scriptFragments.OnBegin = scriptFragment;
        responseScriptBindings.ScriptFragments = scriptFragments;
        topicResponses.VirtualMachineAdapter = responseScriptBindings;
        var responseFlags = new DialogResponseFlags();
        topicResponses.Flags = responseFlags;
        topicResponses.PreviousDialog = new FormLinkNullable<IDialogResponsesGetter>(FormKey.Null);
        topicResponses.FavorLevel = FavorLevel.None;
        topicResponses.ResponseData = new FormLinkNullable<IDialogResponsesGetter>(SkyrimForm(0x0E38C4));
        var isIDCondition = new ConditionFloat();
        var isIDData = new GetIsIDConditionData();
        isIDData.Object.Link.SetTo(SkyrimForm(0x013478));
        isIDCondition.Data = isIDData;
        isIDCondition.ComparisonValue = 1.0f;
        topicResponses.Conditions.Add(isIDCondition);
        var inFactionCondition = new ConditionFloat();
        var inFactionData = new GetInFactionConditionData
        {
            RunOnType = Condition.RunOnType.QuestAlias,
            RunOnTypeIndex = 3
        };
        inFactionData.Faction.Link.SetTo(CurrentFollowerFactionForm);
        inFactionCondition.Data = inFactionData;
        inFactionCondition.ComparisonValue = 1.0f;
        topicResponses.Conditions.Add(inFactionCondition);
        var inFactionCondition2 = new ConditionFloat();
        var inFactionData2 = new GetInFactionConditionData
        {
            RunOnType = Condition.RunOnType.QuestAlias,
            RunOnTypeIndex = 3
        };
        inFactionData2.Faction.Link.SetTo(SkyrimForm(0x072834));
        inFactionCondition2.Data = inFactionData2;
        topicResponses.Conditions.Add(inFactionCondition2);
        bladesPick02Topic.Responses.Add(topicResponses);
        return bladesPick02Topic;
    }

    private static DialogTopic CreateBladesPick03Topic()
    {
        var bladesPick03Topic = new DialogTopic(OwnForm(0x000914), SkyrimRelease.SkyrimSE)
        {
            EditorID = "YLIWF_BladesPick03Topic",
            Name = new TranslatedString(Language.English, "Send <Alias=ExtraFollower03> to join the Blades."),
            Branch = new FormLinkNullable<IDialogBranchGetter>(OwnForm(0x000900)),
            Quest = new FormLinkNullable<IQuestGetter>(DialogueFollowerQuestForm),
            SubtypeName = new RecordType(CustomTopicSubtype)
        };
        var topicResponses = new DialogResponses(OwnForm(0x000924), SkyrimRelease.SkyrimSE);
        var responseScriptBindings = new DialogResponsesAdapter();
        var yliwfBladesPickScript = new ScriptEntry
        {
            Name = PapyrusNames.Scripts.BladesPick
        };
        var dialogueFollowerProperty = new ScriptObjectProperty
        {
            Name = PapyrusNames.Properties.DialogueFollower,
            Object = new FormLink<ISkyrimMajorRecordGetter>(DialogueFollowerQuestForm)
        };
        yliwfBladesPickScript.Properties.Add(dialogueFollowerProperty);
        var freeformSkyhavenTempleAProperty = new ScriptObjectProperty
        {
            Name = PapyrusNames.Properties.BladesQuest,
            Object = new FormLink<ISkyrimMajorRecordGetter>(BladesQuestForm)
        };
        yliwfBladesPickScript.Properties.Add(freeformSkyhavenTempleAProperty);
        var slotIndexProperty = new ScriptIntProperty
        {
            Name = PapyrusNames.Properties.SlotIndex,
            Data = 4
        };
        yliwfBladesPickScript.Properties.Add(slotIndexProperty);
        responseScriptBindings.Scripts.Add(yliwfBladesPickScript);
        var scriptFragments = new ScriptFragments
        {
            FileName = PapyrusNames.Scripts.BladesPick
        };
        var scriptFragment = new ScriptFragment
        {
            ExtraBindDataVersion = (sbyte)(1),
            ScriptName = PapyrusNames.Scripts.BladesPick,
            FragmentName = PapyrusNames.TopicFragment
        };
        scriptFragments.OnBegin = scriptFragment;
        responseScriptBindings.ScriptFragments = scriptFragments;
        topicResponses.VirtualMachineAdapter = responseScriptBindings;
        var responseFlags = new DialogResponseFlags();
        topicResponses.Flags = responseFlags;
        topicResponses.PreviousDialog = new FormLinkNullable<IDialogResponsesGetter>(FormKey.Null);
        topicResponses.FavorLevel = FavorLevel.None;
        topicResponses.ResponseData = new FormLinkNullable<IDialogResponsesGetter>(SkyrimForm(0x0E38C4));
        var isIDCondition = new ConditionFloat();
        var isIDData = new GetIsIDConditionData();
        isIDData.Object.Link.SetTo(SkyrimForm(0x013478));
        isIDCondition.Data = isIDData;
        isIDCondition.ComparisonValue = 1.0f;
        topicResponses.Conditions.Add(isIDCondition);
        var inFactionCondition = new ConditionFloat();
        var inFactionData = new GetInFactionConditionData
        {
            RunOnType = Condition.RunOnType.QuestAlias,
            RunOnTypeIndex = 4
        };
        inFactionData.Faction.Link.SetTo(CurrentFollowerFactionForm);
        inFactionCondition.Data = inFactionData;
        inFactionCondition.ComparisonValue = 1.0f;
        topicResponses.Conditions.Add(inFactionCondition);
        var inFactionCondition2 = new ConditionFloat();
        var inFactionData2 = new GetInFactionConditionData
        {
            RunOnType = Condition.RunOnType.QuestAlias,
            RunOnTypeIndex = 4
        };
        inFactionData2.Faction.Link.SetTo(SkyrimForm(0x072834));
        inFactionCondition2.Data = inFactionData2;
        topicResponses.Conditions.Add(inFactionCondition2);
        bladesPick03Topic.Responses.Add(topicResponses);
        return bladesPick03Topic;
    }

    private static DialogTopic CreateBladesPick04Topic()
    {
        var bladesPick04Topic = new DialogTopic(OwnForm(0x000915), SkyrimRelease.SkyrimSE)
        {
            EditorID = "YLIWF_BladesPick04Topic",
            Name = new TranslatedString(Language.English, "Send <Alias=ExtraFollower04> to join the Blades."),
            Branch = new FormLinkNullable<IDialogBranchGetter>(OwnForm(0x000900)),
            Quest = new FormLinkNullable<IQuestGetter>(DialogueFollowerQuestForm),
            SubtypeName = new RecordType(CustomTopicSubtype)
        };
        var topicResponses = new DialogResponses(OwnForm(0x000925), SkyrimRelease.SkyrimSE);
        var responseScriptBindings = new DialogResponsesAdapter();
        var yliwfBladesPickScript = new ScriptEntry
        {
            Name = PapyrusNames.Scripts.BladesPick
        };
        var dialogueFollowerProperty = new ScriptObjectProperty
        {
            Name = PapyrusNames.Properties.DialogueFollower,
            Object = new FormLink<ISkyrimMajorRecordGetter>(DialogueFollowerQuestForm)
        };
        yliwfBladesPickScript.Properties.Add(dialogueFollowerProperty);
        var freeformSkyhavenTempleAProperty = new ScriptObjectProperty
        {
            Name = PapyrusNames.Properties.BladesQuest,
            Object = new FormLink<ISkyrimMajorRecordGetter>(BladesQuestForm)
        };
        yliwfBladesPickScript.Properties.Add(freeformSkyhavenTempleAProperty);
        var slotIndexProperty = new ScriptIntProperty
        {
            Name = PapyrusNames.Properties.SlotIndex,
            Data = 5
        };
        yliwfBladesPickScript.Properties.Add(slotIndexProperty);
        responseScriptBindings.Scripts.Add(yliwfBladesPickScript);
        var scriptFragments = new ScriptFragments
        {
            FileName = PapyrusNames.Scripts.BladesPick
        };
        var scriptFragment = new ScriptFragment
        {
            ExtraBindDataVersion = (sbyte)(1),
            ScriptName = PapyrusNames.Scripts.BladesPick,
            FragmentName = PapyrusNames.TopicFragment
        };
        scriptFragments.OnBegin = scriptFragment;
        responseScriptBindings.ScriptFragments = scriptFragments;
        topicResponses.VirtualMachineAdapter = responseScriptBindings;
        var responseFlags = new DialogResponseFlags();
        topicResponses.Flags = responseFlags;
        topicResponses.PreviousDialog = new FormLinkNullable<IDialogResponsesGetter>(FormKey.Null);
        topicResponses.FavorLevel = FavorLevel.None;
        topicResponses.ResponseData = new FormLinkNullable<IDialogResponsesGetter>(SkyrimForm(0x0E38C4));
        var isIDCondition = new ConditionFloat();
        var isIDData = new GetIsIDConditionData();
        isIDData.Object.Link.SetTo(SkyrimForm(0x013478));
        isIDCondition.Data = isIDData;
        isIDCondition.ComparisonValue = 1.0f;
        topicResponses.Conditions.Add(isIDCondition);
        var inFactionCondition = new ConditionFloat();
        var inFactionData = new GetInFactionConditionData
        {
            RunOnType = Condition.RunOnType.QuestAlias,
            RunOnTypeIndex = 5
        };
        inFactionData.Faction.Link.SetTo(CurrentFollowerFactionForm);
        inFactionCondition.Data = inFactionData;
        inFactionCondition.ComparisonValue = 1.0f;
        topicResponses.Conditions.Add(inFactionCondition);
        var inFactionCondition2 = new ConditionFloat();
        var inFactionData2 = new GetInFactionConditionData
        {
            RunOnType = Condition.RunOnType.QuestAlias,
            RunOnTypeIndex = 5
        };
        inFactionData2.Faction.Link.SetTo(SkyrimForm(0x072834));
        inFactionCondition2.Data = inFactionData2;
        topicResponses.Conditions.Add(inFactionCondition2);
        bladesPick04Topic.Responses.Add(topicResponses);
        return bladesPick04Topic;
    }

    private static DialogTopic CreateBladesPick05Topic()
    {
        var bladesPick05Topic = new DialogTopic(OwnForm(0x000916), SkyrimRelease.SkyrimSE)
        {
            EditorID = "YLIWF_BladesPick05Topic",
            Name = new TranslatedString(Language.English, "Send <Alias=ExtraFollower05> to join the Blades."),
            Branch = new FormLinkNullable<IDialogBranchGetter>(OwnForm(0x000900)),
            Quest = new FormLinkNullable<IQuestGetter>(DialogueFollowerQuestForm),
            SubtypeName = new RecordType(CustomTopicSubtype)
        };
        var topicResponses = new DialogResponses(OwnForm(0x000926), SkyrimRelease.SkyrimSE);
        var responseScriptBindings = new DialogResponsesAdapter();
        var yliwfBladesPickScript = new ScriptEntry
        {
            Name = PapyrusNames.Scripts.BladesPick
        };
        var dialogueFollowerProperty = new ScriptObjectProperty
        {
            Name = PapyrusNames.Properties.DialogueFollower,
            Object = new FormLink<ISkyrimMajorRecordGetter>(DialogueFollowerQuestForm)
        };
        yliwfBladesPickScript.Properties.Add(dialogueFollowerProperty);
        var freeformSkyhavenTempleAProperty = new ScriptObjectProperty
        {
            Name = PapyrusNames.Properties.BladesQuest,
            Object = new FormLink<ISkyrimMajorRecordGetter>(BladesQuestForm)
        };
        yliwfBladesPickScript.Properties.Add(freeformSkyhavenTempleAProperty);
        var slotIndexProperty = new ScriptIntProperty
        {
            Name = PapyrusNames.Properties.SlotIndex,
            Data = 6
        };
        yliwfBladesPickScript.Properties.Add(slotIndexProperty);
        responseScriptBindings.Scripts.Add(yliwfBladesPickScript);
        var scriptFragments = new ScriptFragments
        {
            FileName = PapyrusNames.Scripts.BladesPick
        };
        var scriptFragment = new ScriptFragment
        {
            ExtraBindDataVersion = (sbyte)(1),
            ScriptName = PapyrusNames.Scripts.BladesPick,
            FragmentName = PapyrusNames.TopicFragment
        };
        scriptFragments.OnBegin = scriptFragment;
        responseScriptBindings.ScriptFragments = scriptFragments;
        topicResponses.VirtualMachineAdapter = responseScriptBindings;
        var responseFlags = new DialogResponseFlags();
        topicResponses.Flags = responseFlags;
        topicResponses.PreviousDialog = new FormLinkNullable<IDialogResponsesGetter>(FormKey.Null);
        topicResponses.FavorLevel = FavorLevel.None;
        topicResponses.ResponseData = new FormLinkNullable<IDialogResponsesGetter>(SkyrimForm(0x0E38C4));
        var isIDCondition = new ConditionFloat();
        var isIDData = new GetIsIDConditionData();
        isIDData.Object.Link.SetTo(SkyrimForm(0x013478));
        isIDCondition.Data = isIDData;
        isIDCondition.ComparisonValue = 1.0f;
        topicResponses.Conditions.Add(isIDCondition);
        var inFactionCondition = new ConditionFloat();
        var inFactionData = new GetInFactionConditionData
        {
            RunOnType = Condition.RunOnType.QuestAlias,
            RunOnTypeIndex = 6
        };
        inFactionData.Faction.Link.SetTo(CurrentFollowerFactionForm);
        inFactionCondition.Data = inFactionData;
        inFactionCondition.ComparisonValue = 1.0f;
        topicResponses.Conditions.Add(inFactionCondition);
        var inFactionCondition2 = new ConditionFloat();
        var inFactionData2 = new GetInFactionConditionData
        {
            RunOnType = Condition.RunOnType.QuestAlias,
            RunOnTypeIndex = 6
        };
        inFactionData2.Faction.Link.SetTo(SkyrimForm(0x072834));
        inFactionCondition2.Data = inFactionData2;
        topicResponses.Conditions.Add(inFactionCondition2);
        bladesPick05Topic.Responses.Add(topicResponses);
        return bladesPick05Topic;
    }

    private static DialogTopic CreateBladesPick06Topic()
    {
        var bladesPick06Topic = new DialogTopic(OwnForm(0x000917), SkyrimRelease.SkyrimSE)
        {
            EditorID = "YLIWF_BladesPick06Topic",
            Name = new TranslatedString(Language.English, "Send <Alias=ExtraFollower06> to join the Blades."),
            Branch = new FormLinkNullable<IDialogBranchGetter>(OwnForm(0x000900)),
            Quest = new FormLinkNullable<IQuestGetter>(DialogueFollowerQuestForm),
            SubtypeName = new RecordType(CustomTopicSubtype)
        };
        var topicResponses = new DialogResponses(OwnForm(0x000927), SkyrimRelease.SkyrimSE);
        var responseScriptBindings = new DialogResponsesAdapter();
        var yliwfBladesPickScript = new ScriptEntry
        {
            Name = PapyrusNames.Scripts.BladesPick
        };
        var dialogueFollowerProperty = new ScriptObjectProperty
        {
            Name = PapyrusNames.Properties.DialogueFollower,
            Object = new FormLink<ISkyrimMajorRecordGetter>(DialogueFollowerQuestForm)
        };
        yliwfBladesPickScript.Properties.Add(dialogueFollowerProperty);
        var freeformSkyhavenTempleAProperty = new ScriptObjectProperty
        {
            Name = PapyrusNames.Properties.BladesQuest,
            Object = new FormLink<ISkyrimMajorRecordGetter>(BladesQuestForm)
        };
        yliwfBladesPickScript.Properties.Add(freeformSkyhavenTempleAProperty);
        var slotIndexProperty = new ScriptIntProperty
        {
            Name = PapyrusNames.Properties.SlotIndex,
            Data = 7
        };
        yliwfBladesPickScript.Properties.Add(slotIndexProperty);
        responseScriptBindings.Scripts.Add(yliwfBladesPickScript);
        var scriptFragments = new ScriptFragments
        {
            FileName = PapyrusNames.Scripts.BladesPick
        };
        var scriptFragment = new ScriptFragment
        {
            ExtraBindDataVersion = (sbyte)(1),
            ScriptName = PapyrusNames.Scripts.BladesPick,
            FragmentName = PapyrusNames.TopicFragment
        };
        scriptFragments.OnBegin = scriptFragment;
        responseScriptBindings.ScriptFragments = scriptFragments;
        topicResponses.VirtualMachineAdapter = responseScriptBindings;
        var responseFlags = new DialogResponseFlags();
        topicResponses.Flags = responseFlags;
        topicResponses.PreviousDialog = new FormLinkNullable<IDialogResponsesGetter>(FormKey.Null);
        topicResponses.FavorLevel = FavorLevel.None;
        topicResponses.ResponseData = new FormLinkNullable<IDialogResponsesGetter>(SkyrimForm(0x0E38C4));
        var isIDCondition = new ConditionFloat();
        var isIDData = new GetIsIDConditionData();
        isIDData.Object.Link.SetTo(SkyrimForm(0x013478));
        isIDCondition.Data = isIDData;
        isIDCondition.ComparisonValue = 1.0f;
        topicResponses.Conditions.Add(isIDCondition);
        var inFactionCondition = new ConditionFloat();
        var inFactionData = new GetInFactionConditionData
        {
            RunOnType = Condition.RunOnType.QuestAlias,
            RunOnTypeIndex = 7
        };
        inFactionData.Faction.Link.SetTo(CurrentFollowerFactionForm);
        inFactionCondition.Data = inFactionData;
        inFactionCondition.ComparisonValue = 1.0f;
        topicResponses.Conditions.Add(inFactionCondition);
        var inFactionCondition2 = new ConditionFloat();
        var inFactionData2 = new GetInFactionConditionData
        {
            RunOnType = Condition.RunOnType.QuestAlias,
            RunOnTypeIndex = 7
        };
        inFactionData2.Faction.Link.SetTo(SkyrimForm(0x072834));
        inFactionCondition2.Data = inFactionData2;
        topicResponses.Conditions.Add(inFactionCondition2);
        bladesPick06Topic.Responses.Add(topicResponses);
        return bladesPick06Topic;
    }

    private static DialogTopic CreateBladesPick07Topic()
    {
        var bladesPick07Topic = new DialogTopic(OwnForm(0x000918), SkyrimRelease.SkyrimSE)
        {
            EditorID = "YLIWF_BladesPick07Topic",
            Name = new TranslatedString(Language.English, "Send <Alias=ExtraFollower07> to join the Blades."),
            Branch = new FormLinkNullable<IDialogBranchGetter>(OwnForm(0x000900)),
            Quest = new FormLinkNullable<IQuestGetter>(DialogueFollowerQuestForm),
            SubtypeName = new RecordType(CustomTopicSubtype)
        };
        var topicResponses = new DialogResponses(OwnForm(0x000928), SkyrimRelease.SkyrimSE);
        var responseScriptBindings = new DialogResponsesAdapter();
        var yliwfBladesPickScript = new ScriptEntry
        {
            Name = PapyrusNames.Scripts.BladesPick
        };
        var dialogueFollowerProperty = new ScriptObjectProperty
        {
            Name = PapyrusNames.Properties.DialogueFollower,
            Object = new FormLink<ISkyrimMajorRecordGetter>(DialogueFollowerQuestForm)
        };
        yliwfBladesPickScript.Properties.Add(dialogueFollowerProperty);
        var freeformSkyhavenTempleAProperty = new ScriptObjectProperty
        {
            Name = PapyrusNames.Properties.BladesQuest,
            Object = new FormLink<ISkyrimMajorRecordGetter>(BladesQuestForm)
        };
        yliwfBladesPickScript.Properties.Add(freeformSkyhavenTempleAProperty);
        var slotIndexProperty = new ScriptIntProperty
        {
            Name = PapyrusNames.Properties.SlotIndex,
            Data = 8
        };
        yliwfBladesPickScript.Properties.Add(slotIndexProperty);
        responseScriptBindings.Scripts.Add(yliwfBladesPickScript);
        var scriptFragments = new ScriptFragments
        {
            FileName = PapyrusNames.Scripts.BladesPick
        };
        var scriptFragment = new ScriptFragment
        {
            ExtraBindDataVersion = (sbyte)(1),
            ScriptName = PapyrusNames.Scripts.BladesPick,
            FragmentName = PapyrusNames.TopicFragment
        };
        scriptFragments.OnBegin = scriptFragment;
        responseScriptBindings.ScriptFragments = scriptFragments;
        topicResponses.VirtualMachineAdapter = responseScriptBindings;
        var responseFlags = new DialogResponseFlags();
        topicResponses.Flags = responseFlags;
        topicResponses.PreviousDialog = new FormLinkNullable<IDialogResponsesGetter>(FormKey.Null);
        topicResponses.FavorLevel = FavorLevel.None;
        topicResponses.ResponseData = new FormLinkNullable<IDialogResponsesGetter>(SkyrimForm(0x0E38C4));
        var isIDCondition = new ConditionFloat();
        var isIDData = new GetIsIDConditionData();
        isIDData.Object.Link.SetTo(SkyrimForm(0x013478));
        isIDCondition.Data = isIDData;
        isIDCondition.ComparisonValue = 1.0f;
        topicResponses.Conditions.Add(isIDCondition);
        var inFactionCondition = new ConditionFloat();
        var inFactionData = new GetInFactionConditionData
        {
            RunOnType = Condition.RunOnType.QuestAlias,
            RunOnTypeIndex = 8
        };
        inFactionData.Faction.Link.SetTo(CurrentFollowerFactionForm);
        inFactionCondition.Data = inFactionData;
        inFactionCondition.ComparisonValue = 1.0f;
        topicResponses.Conditions.Add(inFactionCondition);
        var inFactionCondition2 = new ConditionFloat();
        var inFactionData2 = new GetInFactionConditionData
        {
            RunOnType = Condition.RunOnType.QuestAlias,
            RunOnTypeIndex = 8
        };
        inFactionData2.Faction.Link.SetTo(SkyrimForm(0x072834));
        inFactionCondition2.Data = inFactionData2;
        topicResponses.Conditions.Add(inFactionCondition2);
        bladesPick07Topic.Responses.Add(topicResponses);
        return bladesPick07Topic;
    }

    private static DialogTopic CreateHomeAssignTopic()
    {
        var homeAssignTopic = new DialogTopic(OwnForm(0x000982), SkyrimRelease.SkyrimSE)
        {
            EditorID = "YLIWF_HomeAssignTopic",
            Name = new TranslatedString(Language.English, "I want you to live here."),
            Branch = new FormLinkNullable<IDialogBranchGetter>(OwnForm(0x000980)),
            Quest = new FormLinkNullable<IQuestGetter>(DialogueFollowerQuestForm),
            SubtypeName = new RecordType(CustomTopicSubtype)
        };
        var topicResponses = new DialogResponses(OwnForm(0x000984), SkyrimRelease.SkyrimSE);
        var responseScriptBindings = new DialogResponsesAdapter();
        var yliwfHomeTopicScript = new ScriptEntry
        {
            Name = PapyrusNames.Scripts.HomeTopic
        };
        var homeActionProperty = new ScriptIntProperty
        {
            Name = "HomeAction"
        };
        yliwfHomeTopicScript.Properties.Add(homeActionProperty);
        var homeFactionProperty = new ScriptObjectProperty
        {
            Name = "HomeFaction",
            Object = new FormLink<ISkyrimMajorRecordGetter>(OwnForm(0x000960))
        };
        yliwfHomeTopicScript.Properties.Add(homeFactionProperty);
        var homeQuestProperty = new ScriptObjectProperty
        {
            Name = "HomeQuest",
            Object = new FormLink<ISkyrimMajorRecordGetter>(OwnForm(0x000961))
        };
        yliwfHomeTopicScript.Properties.Add(homeQuestProperty);
        responseScriptBindings.Scripts.Add(yliwfHomeTopicScript);
        var scriptFragments = new ScriptFragments
        {
            FileName = PapyrusNames.Scripts.HomeTopic
        };
        var scriptFragment = new ScriptFragment
        {
            ExtraBindDataVersion = (sbyte)(1),
            ScriptName = PapyrusNames.Scripts.HomeTopic,
            FragmentName = PapyrusNames.TopicFragment
        };
        scriptFragments.OnBegin = scriptFragment;
        responseScriptBindings.ScriptFragments = scriptFragments;
        topicResponses.VirtualMachineAdapter = responseScriptBindings;
        var responseFlags = new DialogResponseFlags();
        topicResponses.Flags = responseFlags;
        topicResponses.PreviousDialog = new FormLinkNullable<IDialogResponsesGetter>(FormKey.Null);
        topicResponses.FavorLevel = FavorLevel.None;
        var globalValueCondition = new ConditionFloat();
        var globalValueData = new GetGlobalValueConditionData();
        globalValueData.Global.Link.SetTo(OwnForm(0x000986));
        globalValueCondition.Data = globalValueData;
        globalValueCondition.ComparisonValue = 1.0f;
        topicResponses.Conditions.Add(globalValueCondition);
        var inFactionCondition = new ConditionFloat();
        var inFactionData = new GetInFactionConditionData();
        inFactionData.Faction.Link.SetTo(CurrentFollowerFactionForm);
        inFactionCondition.Data = inFactionData;
        inFactionCondition.ComparisonValue = 1.0f;
        topicResponses.Conditions.Add(inFactionCondition);
        var inFactionCondition2 = new ConditionFloat();
        var inFactionData2 = new GetInFactionConditionData();
        inFactionData2.Faction.Link.SetTo(SkyrimForm(0x05C84D));
        inFactionCondition2.Data = inFactionData2;
        inFactionCondition2.ComparisonValue = 1.0f;
        topicResponses.Conditions.Add(inFactionCondition2);
        homeAssignTopic.Responses.Add(topicResponses);
        return homeAssignTopic;
    }

    private static DialogTopic CreateHomeClearTopic()
    {
        var homeClearTopic = new DialogTopic(OwnForm(0x000983), SkyrimRelease.SkyrimSE)
        {
            EditorID = "YLIWF_HomeClearTopic",
            Name = new TranslatedString(Language.English, "Forget the home I gave you."),
            Branch = new FormLinkNullable<IDialogBranchGetter>(OwnForm(0x000981)),
            Quest = new FormLinkNullable<IQuestGetter>(DialogueFollowerQuestForm),
            SubtypeName = new RecordType(CustomTopicSubtype)
        };
        var topicResponses = new DialogResponses(OwnForm(0x000985), SkyrimRelease.SkyrimSE);
        var responseScriptBindings = new DialogResponsesAdapter();
        var yliwfHomeTopicScript = new ScriptEntry
        {
            Name = PapyrusNames.Scripts.HomeTopic
        };
        var homeActionProperty = new ScriptIntProperty
        {
            Name = "HomeAction",
            Data = 1
        };
        yliwfHomeTopicScript.Properties.Add(homeActionProperty);
        var homeFactionProperty = new ScriptObjectProperty
        {
            Name = "HomeFaction",
            Object = new FormLink<ISkyrimMajorRecordGetter>(OwnForm(0x000960))
        };
        yliwfHomeTopicScript.Properties.Add(homeFactionProperty);
        var homeQuestProperty = new ScriptObjectProperty
        {
            Name = "HomeQuest",
            Object = new FormLink<ISkyrimMajorRecordGetter>(OwnForm(0x000961))
        };
        yliwfHomeTopicScript.Properties.Add(homeQuestProperty);
        responseScriptBindings.Scripts.Add(yliwfHomeTopicScript);
        var scriptFragments = new ScriptFragments
        {
            FileName = PapyrusNames.Scripts.HomeTopic
        };
        var scriptFragment = new ScriptFragment
        {
            ExtraBindDataVersion = (sbyte)(1),
            ScriptName = PapyrusNames.Scripts.HomeTopic,
            FragmentName = PapyrusNames.TopicFragment
        };
        scriptFragments.OnBegin = scriptFragment;
        responseScriptBindings.ScriptFragments = scriptFragments;
        topicResponses.VirtualMachineAdapter = responseScriptBindings;
        var responseFlags = new DialogResponseFlags();
        topicResponses.Flags = responseFlags;
        topicResponses.PreviousDialog = new FormLinkNullable<IDialogResponsesGetter>(FormKey.Null);
        topicResponses.FavorLevel = FavorLevel.None;
        topicResponses.ResponseData = new FormLinkNullable<IDialogResponsesGetter>(SkyrimForm(0x0E0CBB));
        var globalValueCondition = new ConditionFloat();
        var globalValueData = new GetGlobalValueConditionData();
        globalValueData.Global.Link.SetTo(OwnForm(0x000986));
        globalValueCondition.Data = globalValueData;
        globalValueCondition.ComparisonValue = 1.0f;
        topicResponses.Conditions.Add(globalValueCondition);
        var inFactionCondition = new ConditionFloat();
        var inFactionData = new GetInFactionConditionData();
        inFactionData.Faction.Link.SetTo(CurrentFollowerFactionForm);
        inFactionCondition.Data = inFactionData;
        inFactionCondition.ComparisonValue = 1.0f;
        topicResponses.Conditions.Add(inFactionCondition);
        var inFactionCondition2 = new ConditionFloat();
        var inFactionData2 = new GetInFactionConditionData();
        inFactionData2.Faction.Link.SetTo(OwnForm(0x000960));
        inFactionCondition2.Data = inFactionData2;
        inFactionCondition2.ComparisonValue = 1.0f;
        topicResponses.Conditions.Add(inFactionCondition2);
        var inFactionCondition3 = new ConditionFloat();
        var inFactionData3 = new GetInFactionConditionData();
        inFactionData3.Faction.Link.SetTo(SkyrimForm(0x05C84D));
        inFactionCondition3.Data = inFactionData3;
        inFactionCondition3.ComparisonValue = 1.0f;
        topicResponses.Conditions.Add(inFactionCondition3);
        homeClearTopic.Responses.Add(topicResponses);
        return homeClearTopic;
    }
}
