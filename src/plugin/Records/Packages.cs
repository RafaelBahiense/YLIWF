using Mutagen.Bethesda.Plugins;
using Mutagen.Bethesda.Skyrim;
using Noggog;

internal static partial class FollowerPlugin
{
    // Binary layout references for the unknown fields below:
    // Mutagen 0.54.4: https://github.com/Mutagen-Modding/Mutagen/blob/0.54.4/Mutagen.Bethesda.Skyrim/Records/Major%20Records/Package.xml#L5-L25
    // xEdit PKDT: https://github.com/TES5Edit/TES5Edit/blob/dev-4.1.5/Core/wbDefinitionsTES5.pas#L10532-L10554

    private static void AddPackages(SkyrimMod mod)
    {
        mod.Packages.Add(CreateFollowDistancePackage(0x000990, "Close", 0, 128, 192));
        mod.Packages.Add(CreateFollowDistancePackage(0x000991, "Normal", 1, 256, 384));
        mod.Packages.Add(CreateFollowDistancePackage(0x000992, "Far", 2, 512, 768));
        mod.Packages.Add(CreateFollowerIdleSandboxPackage());
        mod.Packages.Add(CreateFollowerIdleSandbox01Package());
        mod.Packages.Add(CreateFollowerIdleSandbox02Package());
        mod.Packages.Add(CreateFollowerIdleSandbox03Package());
        mod.Packages.Add(CreateFollowerIdleSandbox04Package());
        mod.Packages.Add(CreateFollowerIdleSandbox05Package());
        mod.Packages.Add(CreateFollowerIdleSandbox06Package());
        mod.Packages.Add(CreateFollowerIdleSandbox07Package());
        mod.Packages.Add(CreateFollowerSayDismissPackage());
        mod.Packages.Add(CreateHomeSandbox00Package());
        mod.Packages.Add(CreateHomeSandbox01Package());
        mod.Packages.Add(CreateHomeSandbox02Package());
        mod.Packages.Add(CreateHomeSandbox03Package());
        mod.Packages.Add(CreateHomeSandbox04Package());
        mod.Packages.Add(CreateHomeSandbox05Package());
        mod.Packages.Add(CreateHomeSandbox06Package());
        mod.Packages.Add(CreateHomeSandbox07Package());
    }

    private static Package CreateFollowerIdleSandboxPackage()
    {
        var followerIdleSandboxPackage = new Package(OwnForm(0x000803), SkyrimRelease.SkyrimSE)
        {
            EditorID = "YLIWF_FollowerIdleSandbox",
            Type = Package.Types.Package,
            PreferredSpeed = Package.Speed.Run,
            // PKDT: undocumented byte; preserve the original value. See layout references above.
            Unknown = (byte)(44),
            InterruptFlags = Package.InterruptFlag.HellosToPlayer | Package.InterruptFlag.RandomConversations | Package.InterruptFlag.ObserveCombatBehavior | Package.InterruptFlag.GreetCorpseBehavior | Package.InterruptFlag.ReactionToPlayerActions | Package.InterruptFlag.FriendlyFireComments | Package.InterruptFlag.AggroRadiusBehavior | Package.InterruptFlag.AllowIdleChatter | Package.InterruptFlag.WorldInteractions,
            ScheduleMonth = (sbyte)(-1),
            ScheduleDayOfWeek = Package.DayOfWeek.Any,
            ScheduleHour = (sbyte)(-1),
            ScheduleMinute = (sbyte)(-1),
        }.WithUnusedScheduleBytes();
        var globalValueCondition = new ConditionFloat();
        var globalValueData = new GetGlobalValueConditionData();
        globalValueData.Global.Link.SetTo(OwnForm(0x000805));
        globalValueCondition.Data = globalValueData;
        globalValueCondition.ComparisonValue = 1.0f;
        followerIdleSandboxPackage.Conditions.Add(globalValueCondition);
        var actorValueCondition = new ConditionFloat();
        var actorValueData = new GetActorValueConditionData
        {
            ActorValue = ActorValue.WaitingForPlayer
        };
        actorValueCondition.Data = actorValueData;
        followerIdleSandboxPackage.Conditions.Add(actorValueCondition);
        var locationHasKeywordCondition = new ConditionFloat
        {
            Flags = Condition.Flag.OR
        };
        var locationHasKeywordData = new LocationHasKeywordConditionData();
        locationHasKeywordData.Keyword.Link.SetTo(SkyrimForm(0x039793));
        locationHasKeywordCondition.Data = locationHasKeywordData;
        locationHasKeywordCondition.ComparisonValue = 1.0f;
        followerIdleSandboxPackage.Conditions.Add(locationHasKeywordCondition);
        var locationHasKeywordCondition2 = new ConditionFloat();
        var locationHasKeywordData2 = new LocationHasKeywordConditionData();
        locationHasKeywordData2.Keyword.Link.SetTo(SkyrimForm(0x0130DC));
        locationHasKeywordCondition2.Data = locationHasKeywordData2;
        locationHasKeywordCondition2.ComparisonValue = 1.0f;
        followerIdleSandboxPackage.Conditions.Add(locationHasKeywordCondition2);
        var distanceCondition = new ConditionFloat
        {
            CompareOperator = CompareOperator.LessThanOrEqualTo
        };
        var distanceData = new GetDistanceConditionData();
        distanceData.Target.Link.SetTo(PlayerReferenceForm);
        distanceCondition.Data = distanceData;
        distanceCondition.ComparisonValue = 1024.0f;
        followerIdleSandboxPackage.Conditions.Add(distanceCondition);
        var weaponMagicOutCondition = new ConditionFloat
        {
            CompareOperator = CompareOperator.NotEqualTo
        };
        var weaponMagicOutData = new IsWeaponMagicOutConditionData
        {
            RunOnType = Condition.RunOnType.Reference,
            Reference = new FormLink<ISkyrimMajorRecordGetter>(PlayerReferenceForm)
        };
        weaponMagicOutCondition.Data = weaponMagicOutData;
        weaponMagicOutCondition.ComparisonValue = 1.0f;
        followerIdleSandboxPackage.Conditions.Add(weaponMagicOutCondition);
        var inCombatCondition = new ConditionFloat
        {
            CompareOperator = CompareOperator.NotEqualTo
        };
        var inCombatData = new IsInCombatConditionData
        {
            RunOnType = Condition.RunOnType.Reference,
            Reference = new FormLink<ISkyrimMajorRecordGetter>(PlayerReferenceForm)
        };
        inCombatCondition.Data = inCombatData;
        inCombatCondition.ComparisonValue = 1.0f;
        followerIdleSandboxPackage.Conditions.Add(inCombatCondition);
        var overEncumberedCondition = new ConditionFloat
        {
            CompareOperator = CompareOperator.NotEqualTo
        };
        var overEncumberedData = new IsOverEncumberedConditionData
        {
            RunOnType = Condition.RunOnType.Reference,
            Reference = new FormLink<ISkyrimMajorRecordGetter>(PlayerReferenceForm)
        };
        overEncumberedCondition.Data = overEncumberedData;
        overEncumberedCondition.ComparisonValue = 1.0f;
        followerIdleSandboxPackage.Conditions.Add(overEncumberedCondition);
        var swimmingCondition = new ConditionFloat
        {
            CompareOperator = CompareOperator.NotEqualTo
        };
        var swimmingData = new IsSwimmingConditionData
        {
            RunOnType = Condition.RunOnType.Reference,
            Reference = new FormLink<ISkyrimMajorRecordGetter>(PlayerReferenceForm)
        };
        swimmingCondition.Data = swimmingData;
        swimmingCondition.ComparisonValue = 1.0f;
        followerIdleSandboxPackage.Conditions.Add(swimmingCondition);
        var trespassingCondition = new ConditionFloat
        {
            CompareOperator = CompareOperator.NotEqualTo
        };
        var trespassingData = new IsTrespassingConditionData
        {
            RunOnType = Condition.RunOnType.Reference,
            Reference = new FormLink<ISkyrimMajorRecordGetter>(PlayerReferenceForm)
        };
        trespassingCondition.Data = trespassingData;
        trespassingCondition.ComparisonValue = 1.0f;
        followerIdleSandboxPackage.Conditions.Add(trespassingCondition);
        var weaponOutCondition = new ConditionFloat
        {
            CompareOperator = CompareOperator.NotEqualTo
        };
        var weaponOutData = new IsWeaponOutConditionData
        {
            RunOnType = Condition.RunOnType.Reference,
            Reference = new FormLink<ISkyrimMajorRecordGetter>(PlayerReferenceForm)
        };
        weaponOutCondition.Data = weaponOutData;
        weaponOutCondition.ComparisonValue = 1.0f;
        followerIdleSandboxPackage.Conditions.Add(weaponOutCondition);
        followerIdleSandboxPackage.PackageTemplate = new FormLink<IPackageGetter>(SkyrimForm(0x01C254));
        followerIdleSandboxPackage.DataInputVersion = 10;
        var locationInput0 = new PackageDataLocation();
        var locationTargetRadius = new LocationTargetRadius();
        var locationTarget = new LocationTarget
        {
            Link = new FormLink<IPlacedGetter>(PlayerReferenceForm)
        };
        locationTargetRadius.Target = locationTarget;
        locationTargetRadius.Radius = 1024;
        locationInput0.Location = locationTargetRadius;
        followerIdleSandboxPackage.Data.Add((sbyte)(0), locationInput0);
        var boolInput14 = new PackageDataBool();
        followerIdleSandboxPackage.Data.Add((sbyte)(14), boolInput14);
        var boolInput1 = new PackageDataBool
        {
            Data = true
        };
        followerIdleSandboxPackage.Data.Add((sbyte)(1), boolInput1);
        var boolInput3 = new PackageDataBool();
        followerIdleSandboxPackage.Data.Add((sbyte)(3), boolInput3);
        var boolInput4 = new PackageDataBool
        {
            Data = true
        };
        followerIdleSandboxPackage.Data.Add((sbyte)(4), boolInput4);
        var boolInput5 = new PackageDataBool
        {
            Data = true
        };
        followerIdleSandboxPackage.Data.Add((sbyte)(5), boolInput5);
        var boolInput6 = new PackageDataBool
        {
            Data = true
        };
        followerIdleSandboxPackage.Data.Add((sbyte)(6), boolInput6);
        var boolInput31 = new PackageDataBool
        {
            Data = true
        };
        followerIdleSandboxPackage.Data.Add((sbyte)(31), boolInput31);
        var boolInput7 = new PackageDataBool();
        followerIdleSandboxPackage.Data.Add((sbyte)(7), boolInput7);
        var boolInput25 = new PackageDataBool();
        followerIdleSandboxPackage.Data.Add((sbyte)(25), boolInput25);
        var boolInput27 = new PackageDataBool();
        followerIdleSandboxPackage.Data.Add((sbyte)(27), boolInput27);
        var floatInput29 = new PackageDataFloat
        {
            Data = 50.0f
        };
        followerIdleSandboxPackage.Data.Add((sbyte)(29), floatInput29);
        followerIdleSandboxPackage.XnamMarker = new MemorySlice<byte>(Convert.FromHexString("20"));
        var onBeginEventEvent = new PackageEvent();
        var eventTopic = new TopicReference();
        onBeginEventEvent.Topics.Add(eventTopic);
        followerIdleSandboxPackage.OnBegin = onBeginEventEvent;
        var onEndEventEvent = new PackageEvent();
        var eventTopic2 = new TopicReference();
        onEndEventEvent.Topics.Add(eventTopic2);
        followerIdleSandboxPackage.OnEnd = onEndEventEvent;
        var onChangeEventEvent = new PackageEvent();
        var eventTopic3 = new TopicReference();
        onChangeEventEvent.Topics.Add(eventTopic3);
        followerIdleSandboxPackage.OnChange = onChangeEventEvent;
        return followerIdleSandboxPackage;
    }

    private static Package CreateFollowerIdleSandbox01Package()
    {
        var followerIdleSandbox01Package = new Package(OwnForm(0x000004), SkyrimRelease.SkyrimSE)
        {
            EditorID = "YLIWF_FollowerIdleSandbox01",
            Type = Package.Types.Package,
            PreferredSpeed = Package.Speed.Run,
            InterruptFlags = Package.InterruptFlag.HellosToPlayer | Package.InterruptFlag.RandomConversations | Package.InterruptFlag.ObserveCombatBehavior | Package.InterruptFlag.GreetCorpseBehavior | Package.InterruptFlag.ReactionToPlayerActions | Package.InterruptFlag.FriendlyFireComments | Package.InterruptFlag.AggroRadiusBehavior | Package.InterruptFlag.AllowIdleChatter | Package.InterruptFlag.WorldInteractions,
            ScheduleMonth = (sbyte)(-1),
            ScheduleDayOfWeek = Package.DayOfWeek.Any,
            ScheduleHour = (sbyte)(-1),
            ScheduleMinute = (sbyte)(-1),
        }.WithUnusedScheduleBytes();
        var globalValueCondition = new ConditionFloat();
        var globalValueData = new GetGlobalValueConditionData();
        globalValueData.Global.Link.SetTo(OwnForm(0x000805));
        globalValueCondition.Data = globalValueData;
        globalValueCondition.ComparisonValue = 1.0f;
        followerIdleSandbox01Package.Conditions.Add(globalValueCondition);
        var actorValueCondition = new ConditionFloat();
        var actorValueData = new GetActorValueConditionData
        {
            ActorValue = ActorValue.WaitingForPlayer
        };
        actorValueCondition.Data = actorValueData;
        followerIdleSandbox01Package.Conditions.Add(actorValueCondition);
        var locationHasKeywordCondition = new ConditionFloat
        {
            Flags = Condition.Flag.OR
        };
        var locationHasKeywordData = new LocationHasKeywordConditionData();
        locationHasKeywordData.Keyword.Link.SetTo(SkyrimForm(0x039793));
        locationHasKeywordCondition.Data = locationHasKeywordData;
        locationHasKeywordCondition.ComparisonValue = 1.0f;
        followerIdleSandbox01Package.Conditions.Add(locationHasKeywordCondition);
        var locationHasKeywordCondition2 = new ConditionFloat();
        var locationHasKeywordData2 = new LocationHasKeywordConditionData();
        locationHasKeywordData2.Keyword.Link.SetTo(SkyrimForm(0x0130DC));
        locationHasKeywordCondition2.Data = locationHasKeywordData2;
        locationHasKeywordCondition2.ComparisonValue = 1.0f;
        followerIdleSandbox01Package.Conditions.Add(locationHasKeywordCondition2);
        var distanceCondition = new ConditionFloat
        {
            CompareOperator = CompareOperator.LessThanOrEqualTo
        };
        var distanceData = new GetDistanceConditionData();
        distanceData.Target.Link.SetTo(PlayerReferenceForm);
        distanceCondition.Data = distanceData;
        distanceCondition.ComparisonValue = 1024.0f;
        followerIdleSandbox01Package.Conditions.Add(distanceCondition);
        var weaponMagicOutCondition = new ConditionFloat
        {
            CompareOperator = CompareOperator.NotEqualTo
        };
        var weaponMagicOutData = new IsWeaponMagicOutConditionData
        {
            RunOnType = Condition.RunOnType.Reference,
            Reference = new FormLink<ISkyrimMajorRecordGetter>(PlayerReferenceForm)
        };
        weaponMagicOutCondition.Data = weaponMagicOutData;
        weaponMagicOutCondition.ComparisonValue = 1.0f;
        followerIdleSandbox01Package.Conditions.Add(weaponMagicOutCondition);
        var inCombatCondition = new ConditionFloat
        {
            CompareOperator = CompareOperator.NotEqualTo
        };
        var inCombatData = new IsInCombatConditionData
        {
            RunOnType = Condition.RunOnType.Reference,
            Reference = new FormLink<ISkyrimMajorRecordGetter>(PlayerReferenceForm)
        };
        inCombatCondition.Data = inCombatData;
        inCombatCondition.ComparisonValue = 1.0f;
        followerIdleSandbox01Package.Conditions.Add(inCombatCondition);
        var overEncumberedCondition = new ConditionFloat
        {
            CompareOperator = CompareOperator.NotEqualTo
        };
        var overEncumberedData = new IsOverEncumberedConditionData
        {
            RunOnType = Condition.RunOnType.Reference,
            Reference = new FormLink<ISkyrimMajorRecordGetter>(PlayerReferenceForm)
        };
        overEncumberedCondition.Data = overEncumberedData;
        overEncumberedCondition.ComparisonValue = 1.0f;
        followerIdleSandbox01Package.Conditions.Add(overEncumberedCondition);
        var swimmingCondition = new ConditionFloat
        {
            CompareOperator = CompareOperator.NotEqualTo
        };
        var swimmingData = new IsSwimmingConditionData
        {
            RunOnType = Condition.RunOnType.Reference,
            Reference = new FormLink<ISkyrimMajorRecordGetter>(PlayerReferenceForm)
        };
        swimmingCondition.Data = swimmingData;
        swimmingCondition.ComparisonValue = 1.0f;
        followerIdleSandbox01Package.Conditions.Add(swimmingCondition);
        var trespassingCondition = new ConditionFloat
        {
            CompareOperator = CompareOperator.NotEqualTo
        };
        var trespassingData = new IsTrespassingConditionData
        {
            RunOnType = Condition.RunOnType.Reference,
            Reference = new FormLink<ISkyrimMajorRecordGetter>(PlayerReferenceForm)
        };
        trespassingCondition.Data = trespassingData;
        trespassingCondition.ComparisonValue = 1.0f;
        followerIdleSandbox01Package.Conditions.Add(trespassingCondition);
        var weaponOutCondition = new ConditionFloat
        {
            CompareOperator = CompareOperator.NotEqualTo
        };
        var weaponOutData = new IsWeaponOutConditionData
        {
            RunOnType = Condition.RunOnType.Reference,
            Reference = new FormLink<ISkyrimMajorRecordGetter>(PlayerReferenceForm)
        };
        weaponOutCondition.Data = weaponOutData;
        weaponOutCondition.ComparisonValue = 1.0f;
        followerIdleSandbox01Package.Conditions.Add(weaponOutCondition);
        followerIdleSandbox01Package.PackageTemplate = new FormLink<IPackageGetter>(SkyrimForm(0x01C254));
        followerIdleSandbox01Package.DataInputVersion = 10;
        var locationInput0 = new PackageDataLocation();
        var locationTargetRadius = new LocationTargetRadius();
        var locationTarget = new LocationTarget
        {
            Link = new FormLink<IPlacedGetter>(PlayerReferenceForm)
        };
        locationTargetRadius.Target = locationTarget;
        locationTargetRadius.Radius = 1024;
        locationInput0.Location = locationTargetRadius;
        followerIdleSandbox01Package.Data.Add((sbyte)(0), locationInput0);
        var boolInput14 = new PackageDataBool();
        followerIdleSandbox01Package.Data.Add((sbyte)(14), boolInput14);
        var boolInput1 = new PackageDataBool
        {
            Data = true
        };
        followerIdleSandbox01Package.Data.Add((sbyte)(1), boolInput1);
        var boolInput3 = new PackageDataBool();
        followerIdleSandbox01Package.Data.Add((sbyte)(3), boolInput3);
        var boolInput4 = new PackageDataBool
        {
            Data = true
        };
        followerIdleSandbox01Package.Data.Add((sbyte)(4), boolInput4);
        var boolInput5 = new PackageDataBool
        {
            Data = true
        };
        followerIdleSandbox01Package.Data.Add((sbyte)(5), boolInput5);
        var boolInput6 = new PackageDataBool
        {
            Data = true
        };
        followerIdleSandbox01Package.Data.Add((sbyte)(6), boolInput6);
        var boolInput31 = new PackageDataBool
        {
            Data = true
        };
        followerIdleSandbox01Package.Data.Add((sbyte)(31), boolInput31);
        var boolInput7 = new PackageDataBool();
        followerIdleSandbox01Package.Data.Add((sbyte)(7), boolInput7);
        var boolInput25 = new PackageDataBool();
        followerIdleSandbox01Package.Data.Add((sbyte)(25), boolInput25);
        var boolInput27 = new PackageDataBool();
        followerIdleSandbox01Package.Data.Add((sbyte)(27), boolInput27);
        var floatInput29 = new PackageDataFloat
        {
            Data = 50.0f
        };
        followerIdleSandbox01Package.Data.Add((sbyte)(29), floatInput29);
        followerIdleSandbox01Package.XnamMarker = new MemorySlice<byte>(Convert.FromHexString("20"));
        var onBeginEventEvent = new PackageEvent();
        var eventTopic = new TopicReference();
        onBeginEventEvent.Topics.Add(eventTopic);
        followerIdleSandbox01Package.OnBegin = onBeginEventEvent;
        var onEndEventEvent = new PackageEvent();
        var eventTopic2 = new TopicReference();
        onEndEventEvent.Topics.Add(eventTopic2);
        followerIdleSandbox01Package.OnEnd = onEndEventEvent;
        var onChangeEventEvent = new PackageEvent();
        var eventTopic3 = new TopicReference();
        onChangeEventEvent.Topics.Add(eventTopic3);
        followerIdleSandbox01Package.OnChange = onChangeEventEvent;
        return followerIdleSandbox01Package;
    }

    private static Package CreateFollowerIdleSandbox02Package()
    {
        var followerIdleSandbox02Package = new Package(OwnForm(0x000005), SkyrimRelease.SkyrimSE)
        {
            EditorID = "YLIWF_FollowerIdleSandbox02",
            Type = Package.Types.Package,
            PreferredSpeed = Package.Speed.Run,
            InterruptFlags = Package.InterruptFlag.HellosToPlayer | Package.InterruptFlag.RandomConversations | Package.InterruptFlag.ObserveCombatBehavior | Package.InterruptFlag.GreetCorpseBehavior | Package.InterruptFlag.ReactionToPlayerActions | Package.InterruptFlag.FriendlyFireComments | Package.InterruptFlag.AggroRadiusBehavior | Package.InterruptFlag.AllowIdleChatter | Package.InterruptFlag.WorldInteractions,
            ScheduleMonth = (sbyte)(-1),
            ScheduleDayOfWeek = Package.DayOfWeek.Any,
            ScheduleHour = (sbyte)(-1),
            ScheduleMinute = (sbyte)(-1),
        }.WithUnusedScheduleBytes();
        var globalValueCondition = new ConditionFloat();
        var globalValueData = new GetGlobalValueConditionData();
        globalValueData.Global.Link.SetTo(OwnForm(0x000805));
        globalValueCondition.Data = globalValueData;
        globalValueCondition.ComparisonValue = 1.0f;
        followerIdleSandbox02Package.Conditions.Add(globalValueCondition);
        var actorValueCondition = new ConditionFloat();
        var actorValueData = new GetActorValueConditionData
        {
            ActorValue = ActorValue.WaitingForPlayer
        };
        actorValueCondition.Data = actorValueData;
        followerIdleSandbox02Package.Conditions.Add(actorValueCondition);
        var locationHasKeywordCondition = new ConditionFloat
        {
            Flags = Condition.Flag.OR
        };
        var locationHasKeywordData = new LocationHasKeywordConditionData();
        locationHasKeywordData.Keyword.Link.SetTo(SkyrimForm(0x039793));
        locationHasKeywordCondition.Data = locationHasKeywordData;
        locationHasKeywordCondition.ComparisonValue = 1.0f;
        followerIdleSandbox02Package.Conditions.Add(locationHasKeywordCondition);
        var locationHasKeywordCondition2 = new ConditionFloat();
        var locationHasKeywordData2 = new LocationHasKeywordConditionData();
        locationHasKeywordData2.Keyword.Link.SetTo(SkyrimForm(0x0130DC));
        locationHasKeywordCondition2.Data = locationHasKeywordData2;
        locationHasKeywordCondition2.ComparisonValue = 1.0f;
        followerIdleSandbox02Package.Conditions.Add(locationHasKeywordCondition2);
        var distanceCondition = new ConditionFloat
        {
            CompareOperator = CompareOperator.LessThanOrEqualTo
        };
        var distanceData = new GetDistanceConditionData();
        distanceData.Target.Link.SetTo(PlayerReferenceForm);
        distanceCondition.Data = distanceData;
        distanceCondition.ComparisonValue = 1024.0f;
        followerIdleSandbox02Package.Conditions.Add(distanceCondition);
        var weaponMagicOutCondition = new ConditionFloat
        {
            CompareOperator = CompareOperator.NotEqualTo
        };
        var weaponMagicOutData = new IsWeaponMagicOutConditionData
        {
            RunOnType = Condition.RunOnType.Reference,
            Reference = new FormLink<ISkyrimMajorRecordGetter>(PlayerReferenceForm)
        };
        weaponMagicOutCondition.Data = weaponMagicOutData;
        weaponMagicOutCondition.ComparisonValue = 1.0f;
        followerIdleSandbox02Package.Conditions.Add(weaponMagicOutCondition);
        var inCombatCondition = new ConditionFloat
        {
            CompareOperator = CompareOperator.NotEqualTo
        };
        var inCombatData = new IsInCombatConditionData
        {
            RunOnType = Condition.RunOnType.Reference,
            Reference = new FormLink<ISkyrimMajorRecordGetter>(PlayerReferenceForm)
        };
        inCombatCondition.Data = inCombatData;
        inCombatCondition.ComparisonValue = 1.0f;
        followerIdleSandbox02Package.Conditions.Add(inCombatCondition);
        var overEncumberedCondition = new ConditionFloat
        {
            CompareOperator = CompareOperator.NotEqualTo
        };
        var overEncumberedData = new IsOverEncumberedConditionData
        {
            RunOnType = Condition.RunOnType.Reference,
            Reference = new FormLink<ISkyrimMajorRecordGetter>(PlayerReferenceForm)
        };
        overEncumberedCondition.Data = overEncumberedData;
        overEncumberedCondition.ComparisonValue = 1.0f;
        followerIdleSandbox02Package.Conditions.Add(overEncumberedCondition);
        var swimmingCondition = new ConditionFloat
        {
            CompareOperator = CompareOperator.NotEqualTo
        };
        var swimmingData = new IsSwimmingConditionData
        {
            RunOnType = Condition.RunOnType.Reference,
            Reference = new FormLink<ISkyrimMajorRecordGetter>(PlayerReferenceForm)
        };
        swimmingCondition.Data = swimmingData;
        swimmingCondition.ComparisonValue = 1.0f;
        followerIdleSandbox02Package.Conditions.Add(swimmingCondition);
        var trespassingCondition = new ConditionFloat
        {
            CompareOperator = CompareOperator.NotEqualTo
        };
        var trespassingData = new IsTrespassingConditionData
        {
            RunOnType = Condition.RunOnType.Reference,
            Reference = new FormLink<ISkyrimMajorRecordGetter>(PlayerReferenceForm)
        };
        trespassingCondition.Data = trespassingData;
        trespassingCondition.ComparisonValue = 1.0f;
        followerIdleSandbox02Package.Conditions.Add(trespassingCondition);
        var weaponOutCondition = new ConditionFloat
        {
            CompareOperator = CompareOperator.NotEqualTo
        };
        var weaponOutData = new IsWeaponOutConditionData
        {
            RunOnType = Condition.RunOnType.Reference,
            Reference = new FormLink<ISkyrimMajorRecordGetter>(PlayerReferenceForm)
        };
        weaponOutCondition.Data = weaponOutData;
        weaponOutCondition.ComparisonValue = 1.0f;
        followerIdleSandbox02Package.Conditions.Add(weaponOutCondition);
        followerIdleSandbox02Package.PackageTemplate = new FormLink<IPackageGetter>(SkyrimForm(0x01C254));
        followerIdleSandbox02Package.DataInputVersion = 10;
        var locationInput0 = new PackageDataLocation();
        var locationTargetRadius = new LocationTargetRadius();
        var locationTarget = new LocationTarget
        {
            Link = new FormLink<IPlacedGetter>(PlayerReferenceForm)
        };
        locationTargetRadius.Target = locationTarget;
        locationTargetRadius.Radius = 1024;
        locationInput0.Location = locationTargetRadius;
        followerIdleSandbox02Package.Data.Add((sbyte)(0), locationInput0);
        var boolInput14 = new PackageDataBool();
        followerIdleSandbox02Package.Data.Add((sbyte)(14), boolInput14);
        var boolInput1 = new PackageDataBool
        {
            Data = true
        };
        followerIdleSandbox02Package.Data.Add((sbyte)(1), boolInput1);
        var boolInput3 = new PackageDataBool();
        followerIdleSandbox02Package.Data.Add((sbyte)(3), boolInput3);
        var boolInput4 = new PackageDataBool
        {
            Data = true
        };
        followerIdleSandbox02Package.Data.Add((sbyte)(4), boolInput4);
        var boolInput5 = new PackageDataBool
        {
            Data = true
        };
        followerIdleSandbox02Package.Data.Add((sbyte)(5), boolInput5);
        var boolInput6 = new PackageDataBool
        {
            Data = true
        };
        followerIdleSandbox02Package.Data.Add((sbyte)(6), boolInput6);
        var boolInput31 = new PackageDataBool
        {
            Data = true
        };
        followerIdleSandbox02Package.Data.Add((sbyte)(31), boolInput31);
        var boolInput7 = new PackageDataBool();
        followerIdleSandbox02Package.Data.Add((sbyte)(7), boolInput7);
        var boolInput25 = new PackageDataBool();
        followerIdleSandbox02Package.Data.Add((sbyte)(25), boolInput25);
        var boolInput27 = new PackageDataBool();
        followerIdleSandbox02Package.Data.Add((sbyte)(27), boolInput27);
        var floatInput29 = new PackageDataFloat
        {
            Data = 50.0f
        };
        followerIdleSandbox02Package.Data.Add((sbyte)(29), floatInput29);
        followerIdleSandbox02Package.XnamMarker = new MemorySlice<byte>(Convert.FromHexString("20"));
        var onBeginEventEvent = new PackageEvent();
        var eventTopic = new TopicReference();
        onBeginEventEvent.Topics.Add(eventTopic);
        followerIdleSandbox02Package.OnBegin = onBeginEventEvent;
        var onEndEventEvent = new PackageEvent();
        var eventTopic2 = new TopicReference();
        onEndEventEvent.Topics.Add(eventTopic2);
        followerIdleSandbox02Package.OnEnd = onEndEventEvent;
        var onChangeEventEvent = new PackageEvent();
        var eventTopic3 = new TopicReference();
        onChangeEventEvent.Topics.Add(eventTopic3);
        followerIdleSandbox02Package.OnChange = onChangeEventEvent;
        return followerIdleSandbox02Package;
    }

    private static Package CreateFollowerIdleSandbox03Package()
    {
        var followerIdleSandbox03Package = new Package(OwnForm(0x000006), SkyrimRelease.SkyrimSE)
        {
            EditorID = "YLIWF_FollowerIdleSandbox03",
            Type = Package.Types.Package,
            PreferredSpeed = Package.Speed.Run,
            InterruptFlags = Package.InterruptFlag.HellosToPlayer | Package.InterruptFlag.RandomConversations | Package.InterruptFlag.ObserveCombatBehavior | Package.InterruptFlag.GreetCorpseBehavior | Package.InterruptFlag.ReactionToPlayerActions | Package.InterruptFlag.FriendlyFireComments | Package.InterruptFlag.AggroRadiusBehavior | Package.InterruptFlag.AllowIdleChatter | Package.InterruptFlag.WorldInteractions,
            ScheduleMonth = (sbyte)(-1),
            ScheduleDayOfWeek = Package.DayOfWeek.Any,
            ScheduleHour = (sbyte)(-1),
            ScheduleMinute = (sbyte)(-1),
        }.WithUnusedScheduleBytes();
        var globalValueCondition = new ConditionFloat();
        var globalValueData = new GetGlobalValueConditionData();
        globalValueData.Global.Link.SetTo(OwnForm(0x000805));
        globalValueCondition.Data = globalValueData;
        globalValueCondition.ComparisonValue = 1.0f;
        followerIdleSandbox03Package.Conditions.Add(globalValueCondition);
        var actorValueCondition = new ConditionFloat();
        var actorValueData = new GetActorValueConditionData
        {
            ActorValue = ActorValue.WaitingForPlayer
        };
        actorValueCondition.Data = actorValueData;
        followerIdleSandbox03Package.Conditions.Add(actorValueCondition);
        var locationHasKeywordCondition = new ConditionFloat
        {
            Flags = Condition.Flag.OR
        };
        var locationHasKeywordData = new LocationHasKeywordConditionData();
        locationHasKeywordData.Keyword.Link.SetTo(SkyrimForm(0x039793));
        locationHasKeywordCondition.Data = locationHasKeywordData;
        locationHasKeywordCondition.ComparisonValue = 1.0f;
        followerIdleSandbox03Package.Conditions.Add(locationHasKeywordCondition);
        var locationHasKeywordCondition2 = new ConditionFloat();
        var locationHasKeywordData2 = new LocationHasKeywordConditionData();
        locationHasKeywordData2.Keyword.Link.SetTo(SkyrimForm(0x0130DC));
        locationHasKeywordCondition2.Data = locationHasKeywordData2;
        locationHasKeywordCondition2.ComparisonValue = 1.0f;
        followerIdleSandbox03Package.Conditions.Add(locationHasKeywordCondition2);
        var distanceCondition = new ConditionFloat
        {
            CompareOperator = CompareOperator.LessThanOrEqualTo
        };
        var distanceData = new GetDistanceConditionData();
        distanceData.Target.Link.SetTo(PlayerReferenceForm);
        distanceCondition.Data = distanceData;
        distanceCondition.ComparisonValue = 1024.0f;
        followerIdleSandbox03Package.Conditions.Add(distanceCondition);
        var weaponMagicOutCondition = new ConditionFloat
        {
            CompareOperator = CompareOperator.NotEqualTo
        };
        var weaponMagicOutData = new IsWeaponMagicOutConditionData
        {
            RunOnType = Condition.RunOnType.Reference,
            Reference = new FormLink<ISkyrimMajorRecordGetter>(PlayerReferenceForm)
        };
        weaponMagicOutCondition.Data = weaponMagicOutData;
        weaponMagicOutCondition.ComparisonValue = 1.0f;
        followerIdleSandbox03Package.Conditions.Add(weaponMagicOutCondition);
        var inCombatCondition = new ConditionFloat
        {
            CompareOperator = CompareOperator.NotEqualTo
        };
        var inCombatData = new IsInCombatConditionData
        {
            RunOnType = Condition.RunOnType.Reference,
            Reference = new FormLink<ISkyrimMajorRecordGetter>(PlayerReferenceForm)
        };
        inCombatCondition.Data = inCombatData;
        inCombatCondition.ComparisonValue = 1.0f;
        followerIdleSandbox03Package.Conditions.Add(inCombatCondition);
        var overEncumberedCondition = new ConditionFloat
        {
            CompareOperator = CompareOperator.NotEqualTo
        };
        var overEncumberedData = new IsOverEncumberedConditionData
        {
            RunOnType = Condition.RunOnType.Reference,
            Reference = new FormLink<ISkyrimMajorRecordGetter>(PlayerReferenceForm)
        };
        overEncumberedCondition.Data = overEncumberedData;
        overEncumberedCondition.ComparisonValue = 1.0f;
        followerIdleSandbox03Package.Conditions.Add(overEncumberedCondition);
        var swimmingCondition = new ConditionFloat
        {
            CompareOperator = CompareOperator.NotEqualTo
        };
        var swimmingData = new IsSwimmingConditionData
        {
            RunOnType = Condition.RunOnType.Reference,
            Reference = new FormLink<ISkyrimMajorRecordGetter>(PlayerReferenceForm)
        };
        swimmingCondition.Data = swimmingData;
        swimmingCondition.ComparisonValue = 1.0f;
        followerIdleSandbox03Package.Conditions.Add(swimmingCondition);
        var trespassingCondition = new ConditionFloat
        {
            CompareOperator = CompareOperator.NotEqualTo
        };
        var trespassingData = new IsTrespassingConditionData
        {
            RunOnType = Condition.RunOnType.Reference,
            Reference = new FormLink<ISkyrimMajorRecordGetter>(PlayerReferenceForm)
        };
        trespassingCondition.Data = trespassingData;
        trespassingCondition.ComparisonValue = 1.0f;
        followerIdleSandbox03Package.Conditions.Add(trespassingCondition);
        var weaponOutCondition = new ConditionFloat
        {
            CompareOperator = CompareOperator.NotEqualTo
        };
        var weaponOutData = new IsWeaponOutConditionData
        {
            RunOnType = Condition.RunOnType.Reference,
            Reference = new FormLink<ISkyrimMajorRecordGetter>(PlayerReferenceForm)
        };
        weaponOutCondition.Data = weaponOutData;
        weaponOutCondition.ComparisonValue = 1.0f;
        followerIdleSandbox03Package.Conditions.Add(weaponOutCondition);
        followerIdleSandbox03Package.PackageTemplate = new FormLink<IPackageGetter>(SkyrimForm(0x01C254));
        followerIdleSandbox03Package.DataInputVersion = 10;
        var locationInput0 = new PackageDataLocation();
        var locationTargetRadius = new LocationTargetRadius();
        var locationTarget = new LocationTarget
        {
            Link = new FormLink<IPlacedGetter>(PlayerReferenceForm)
        };
        locationTargetRadius.Target = locationTarget;
        locationTargetRadius.Radius = 1024;
        locationInput0.Location = locationTargetRadius;
        followerIdleSandbox03Package.Data.Add((sbyte)(0), locationInput0);
        var boolInput14 = new PackageDataBool();
        followerIdleSandbox03Package.Data.Add((sbyte)(14), boolInput14);
        var boolInput1 = new PackageDataBool
        {
            Data = true
        };
        followerIdleSandbox03Package.Data.Add((sbyte)(1), boolInput1);
        var boolInput3 = new PackageDataBool();
        followerIdleSandbox03Package.Data.Add((sbyte)(3), boolInput3);
        var boolInput4 = new PackageDataBool
        {
            Data = true
        };
        followerIdleSandbox03Package.Data.Add((sbyte)(4), boolInput4);
        var boolInput5 = new PackageDataBool
        {
            Data = true
        };
        followerIdleSandbox03Package.Data.Add((sbyte)(5), boolInput5);
        var boolInput6 = new PackageDataBool
        {
            Data = true
        };
        followerIdleSandbox03Package.Data.Add((sbyte)(6), boolInput6);
        var boolInput31 = new PackageDataBool
        {
            Data = true
        };
        followerIdleSandbox03Package.Data.Add((sbyte)(31), boolInput31);
        var boolInput7 = new PackageDataBool();
        followerIdleSandbox03Package.Data.Add((sbyte)(7), boolInput7);
        var boolInput25 = new PackageDataBool();
        followerIdleSandbox03Package.Data.Add((sbyte)(25), boolInput25);
        var boolInput27 = new PackageDataBool();
        followerIdleSandbox03Package.Data.Add((sbyte)(27), boolInput27);
        var floatInput29 = new PackageDataFloat
        {
            Data = 50.0f
        };
        followerIdleSandbox03Package.Data.Add((sbyte)(29), floatInput29);
        followerIdleSandbox03Package.XnamMarker = new MemorySlice<byte>(Convert.FromHexString("20"));
        var onBeginEventEvent = new PackageEvent();
        var eventTopic = new TopicReference();
        onBeginEventEvent.Topics.Add(eventTopic);
        followerIdleSandbox03Package.OnBegin = onBeginEventEvent;
        var onEndEventEvent = new PackageEvent();
        var eventTopic2 = new TopicReference();
        onEndEventEvent.Topics.Add(eventTopic2);
        followerIdleSandbox03Package.OnEnd = onEndEventEvent;
        var onChangeEventEvent = new PackageEvent();
        var eventTopic3 = new TopicReference();
        onChangeEventEvent.Topics.Add(eventTopic3);
        followerIdleSandbox03Package.OnChange = onChangeEventEvent;
        return followerIdleSandbox03Package;
    }

    private static Package CreateFollowerIdleSandbox04Package()
    {
        var followerIdleSandbox04Package = new Package(OwnForm(0x000007), SkyrimRelease.SkyrimSE)
        {
            EditorID = "YLIWF_FollowerIdleSandbox04",
            Type = Package.Types.Package,
            PreferredSpeed = Package.Speed.Run,
            InterruptFlags = Package.InterruptFlag.HellosToPlayer | Package.InterruptFlag.RandomConversations | Package.InterruptFlag.ObserveCombatBehavior | Package.InterruptFlag.GreetCorpseBehavior | Package.InterruptFlag.ReactionToPlayerActions | Package.InterruptFlag.FriendlyFireComments | Package.InterruptFlag.AggroRadiusBehavior | Package.InterruptFlag.AllowIdleChatter | Package.InterruptFlag.WorldInteractions,
            ScheduleMonth = (sbyte)(-1),
            ScheduleDayOfWeek = Package.DayOfWeek.Any,
            ScheduleHour = (sbyte)(-1),
            ScheduleMinute = (sbyte)(-1),
        }.WithUnusedScheduleBytes();
        var globalValueCondition = new ConditionFloat();
        var globalValueData = new GetGlobalValueConditionData();
        globalValueData.Global.Link.SetTo(OwnForm(0x000805));
        globalValueCondition.Data = globalValueData;
        globalValueCondition.ComparisonValue = 1.0f;
        followerIdleSandbox04Package.Conditions.Add(globalValueCondition);
        var actorValueCondition = new ConditionFloat();
        var actorValueData = new GetActorValueConditionData
        {
            ActorValue = ActorValue.WaitingForPlayer
        };
        actorValueCondition.Data = actorValueData;
        followerIdleSandbox04Package.Conditions.Add(actorValueCondition);
        var locationHasKeywordCondition = new ConditionFloat
        {
            Flags = Condition.Flag.OR
        };
        var locationHasKeywordData = new LocationHasKeywordConditionData();
        locationHasKeywordData.Keyword.Link.SetTo(SkyrimForm(0x039793));
        locationHasKeywordCondition.Data = locationHasKeywordData;
        locationHasKeywordCondition.ComparisonValue = 1.0f;
        followerIdleSandbox04Package.Conditions.Add(locationHasKeywordCondition);
        var locationHasKeywordCondition2 = new ConditionFloat();
        var locationHasKeywordData2 = new LocationHasKeywordConditionData();
        locationHasKeywordData2.Keyword.Link.SetTo(SkyrimForm(0x0130DC));
        locationHasKeywordCondition2.Data = locationHasKeywordData2;
        locationHasKeywordCondition2.ComparisonValue = 1.0f;
        followerIdleSandbox04Package.Conditions.Add(locationHasKeywordCondition2);
        var distanceCondition = new ConditionFloat
        {
            CompareOperator = CompareOperator.LessThanOrEqualTo
        };
        var distanceData = new GetDistanceConditionData();
        distanceData.Target.Link.SetTo(PlayerReferenceForm);
        distanceCondition.Data = distanceData;
        distanceCondition.ComparisonValue = 1024.0f;
        followerIdleSandbox04Package.Conditions.Add(distanceCondition);
        var weaponMagicOutCondition = new ConditionFloat
        {
            CompareOperator = CompareOperator.NotEqualTo
        };
        var weaponMagicOutData = new IsWeaponMagicOutConditionData
        {
            RunOnType = Condition.RunOnType.Reference,
            Reference = new FormLink<ISkyrimMajorRecordGetter>(PlayerReferenceForm)
        };
        weaponMagicOutCondition.Data = weaponMagicOutData;
        weaponMagicOutCondition.ComparisonValue = 1.0f;
        followerIdleSandbox04Package.Conditions.Add(weaponMagicOutCondition);
        var inCombatCondition = new ConditionFloat
        {
            CompareOperator = CompareOperator.NotEqualTo
        };
        var inCombatData = new IsInCombatConditionData
        {
            RunOnType = Condition.RunOnType.Reference,
            Reference = new FormLink<ISkyrimMajorRecordGetter>(PlayerReferenceForm)
        };
        inCombatCondition.Data = inCombatData;
        inCombatCondition.ComparisonValue = 1.0f;
        followerIdleSandbox04Package.Conditions.Add(inCombatCondition);
        var overEncumberedCondition = new ConditionFloat
        {
            CompareOperator = CompareOperator.NotEqualTo
        };
        var overEncumberedData = new IsOverEncumberedConditionData
        {
            RunOnType = Condition.RunOnType.Reference,
            Reference = new FormLink<ISkyrimMajorRecordGetter>(PlayerReferenceForm)
        };
        overEncumberedCondition.Data = overEncumberedData;
        overEncumberedCondition.ComparisonValue = 1.0f;
        followerIdleSandbox04Package.Conditions.Add(overEncumberedCondition);
        var swimmingCondition = new ConditionFloat
        {
            CompareOperator = CompareOperator.NotEqualTo
        };
        var swimmingData = new IsSwimmingConditionData
        {
            RunOnType = Condition.RunOnType.Reference,
            Reference = new FormLink<ISkyrimMajorRecordGetter>(PlayerReferenceForm)
        };
        swimmingCondition.Data = swimmingData;
        swimmingCondition.ComparisonValue = 1.0f;
        followerIdleSandbox04Package.Conditions.Add(swimmingCondition);
        var trespassingCondition = new ConditionFloat
        {
            CompareOperator = CompareOperator.NotEqualTo
        };
        var trespassingData = new IsTrespassingConditionData
        {
            RunOnType = Condition.RunOnType.Reference,
            Reference = new FormLink<ISkyrimMajorRecordGetter>(PlayerReferenceForm)
        };
        trespassingCondition.Data = trespassingData;
        trespassingCondition.ComparisonValue = 1.0f;
        followerIdleSandbox04Package.Conditions.Add(trespassingCondition);
        var weaponOutCondition = new ConditionFloat
        {
            CompareOperator = CompareOperator.NotEqualTo
        };
        var weaponOutData = new IsWeaponOutConditionData
        {
            RunOnType = Condition.RunOnType.Reference,
            Reference = new FormLink<ISkyrimMajorRecordGetter>(PlayerReferenceForm)
        };
        weaponOutCondition.Data = weaponOutData;
        weaponOutCondition.ComparisonValue = 1.0f;
        followerIdleSandbox04Package.Conditions.Add(weaponOutCondition);
        followerIdleSandbox04Package.PackageTemplate = new FormLink<IPackageGetter>(SkyrimForm(0x01C254));
        followerIdleSandbox04Package.DataInputVersion = 10;
        var locationInput0 = new PackageDataLocation();
        var locationTargetRadius = new LocationTargetRadius();
        var locationTarget = new LocationTarget
        {
            Link = new FormLink<IPlacedGetter>(PlayerReferenceForm)
        };
        locationTargetRadius.Target = locationTarget;
        locationTargetRadius.Radius = 1024;
        locationInput0.Location = locationTargetRadius;
        followerIdleSandbox04Package.Data.Add((sbyte)(0), locationInput0);
        var boolInput14 = new PackageDataBool();
        followerIdleSandbox04Package.Data.Add((sbyte)(14), boolInput14);
        var boolInput1 = new PackageDataBool
        {
            Data = true
        };
        followerIdleSandbox04Package.Data.Add((sbyte)(1), boolInput1);
        var boolInput3 = new PackageDataBool();
        followerIdleSandbox04Package.Data.Add((sbyte)(3), boolInput3);
        var boolInput4 = new PackageDataBool
        {
            Data = true
        };
        followerIdleSandbox04Package.Data.Add((sbyte)(4), boolInput4);
        var boolInput5 = new PackageDataBool
        {
            Data = true
        };
        followerIdleSandbox04Package.Data.Add((sbyte)(5), boolInput5);
        var boolInput6 = new PackageDataBool
        {
            Data = true
        };
        followerIdleSandbox04Package.Data.Add((sbyte)(6), boolInput6);
        var boolInput31 = new PackageDataBool
        {
            Data = true
        };
        followerIdleSandbox04Package.Data.Add((sbyte)(31), boolInput31);
        var boolInput7 = new PackageDataBool();
        followerIdleSandbox04Package.Data.Add((sbyte)(7), boolInput7);
        var boolInput25 = new PackageDataBool();
        followerIdleSandbox04Package.Data.Add((sbyte)(25), boolInput25);
        var boolInput27 = new PackageDataBool();
        followerIdleSandbox04Package.Data.Add((sbyte)(27), boolInput27);
        var floatInput29 = new PackageDataFloat
        {
            Data = 50.0f
        };
        followerIdleSandbox04Package.Data.Add((sbyte)(29), floatInput29);
        followerIdleSandbox04Package.XnamMarker = new MemorySlice<byte>(Convert.FromHexString("20"));
        var onBeginEventEvent = new PackageEvent();
        var eventTopic = new TopicReference();
        onBeginEventEvent.Topics.Add(eventTopic);
        followerIdleSandbox04Package.OnBegin = onBeginEventEvent;
        var onEndEventEvent = new PackageEvent();
        var eventTopic2 = new TopicReference();
        onEndEventEvent.Topics.Add(eventTopic2);
        followerIdleSandbox04Package.OnEnd = onEndEventEvent;
        var onChangeEventEvent = new PackageEvent();
        var eventTopic3 = new TopicReference();
        onChangeEventEvent.Topics.Add(eventTopic3);
        followerIdleSandbox04Package.OnChange = onChangeEventEvent;
        return followerIdleSandbox04Package;
    }

    private static Package CreateFollowerIdleSandbox05Package()
    {
        var followerIdleSandbox05Package = new Package(OwnForm(0x000008), SkyrimRelease.SkyrimSE)
        {
            EditorID = "YLIWF_FollowerIdleSandbox05",
            Type = Package.Types.Package,
            PreferredSpeed = Package.Speed.Run,
            InterruptFlags = Package.InterruptFlag.HellosToPlayer | Package.InterruptFlag.RandomConversations | Package.InterruptFlag.ObserveCombatBehavior | Package.InterruptFlag.GreetCorpseBehavior | Package.InterruptFlag.ReactionToPlayerActions | Package.InterruptFlag.FriendlyFireComments | Package.InterruptFlag.AggroRadiusBehavior | Package.InterruptFlag.AllowIdleChatter | Package.InterruptFlag.WorldInteractions,
            ScheduleMonth = (sbyte)(-1),
            ScheduleDayOfWeek = Package.DayOfWeek.Any,
            ScheduleHour = (sbyte)(-1),
            ScheduleMinute = (sbyte)(-1),
        }.WithUnusedScheduleBytes();
        var globalValueCondition = new ConditionFloat();
        var globalValueData = new GetGlobalValueConditionData();
        globalValueData.Global.Link.SetTo(OwnForm(0x000805));
        globalValueCondition.Data = globalValueData;
        globalValueCondition.ComparisonValue = 1.0f;
        followerIdleSandbox05Package.Conditions.Add(globalValueCondition);
        var actorValueCondition = new ConditionFloat();
        var actorValueData = new GetActorValueConditionData
        {
            ActorValue = ActorValue.WaitingForPlayer
        };
        actorValueCondition.Data = actorValueData;
        followerIdleSandbox05Package.Conditions.Add(actorValueCondition);
        var locationHasKeywordCondition = new ConditionFloat
        {
            Flags = Condition.Flag.OR
        };
        var locationHasKeywordData = new LocationHasKeywordConditionData();
        locationHasKeywordData.Keyword.Link.SetTo(SkyrimForm(0x039793));
        locationHasKeywordCondition.Data = locationHasKeywordData;
        locationHasKeywordCondition.ComparisonValue = 1.0f;
        followerIdleSandbox05Package.Conditions.Add(locationHasKeywordCondition);
        var locationHasKeywordCondition2 = new ConditionFloat();
        var locationHasKeywordData2 = new LocationHasKeywordConditionData();
        locationHasKeywordData2.Keyword.Link.SetTo(SkyrimForm(0x0130DC));
        locationHasKeywordCondition2.Data = locationHasKeywordData2;
        locationHasKeywordCondition2.ComparisonValue = 1.0f;
        followerIdleSandbox05Package.Conditions.Add(locationHasKeywordCondition2);
        var distanceCondition = new ConditionFloat
        {
            CompareOperator = CompareOperator.LessThanOrEqualTo
        };
        var distanceData = new GetDistanceConditionData();
        distanceData.Target.Link.SetTo(PlayerReferenceForm);
        distanceCondition.Data = distanceData;
        distanceCondition.ComparisonValue = 1024.0f;
        followerIdleSandbox05Package.Conditions.Add(distanceCondition);
        var weaponMagicOutCondition = new ConditionFloat
        {
            CompareOperator = CompareOperator.NotEqualTo
        };
        var weaponMagicOutData = new IsWeaponMagicOutConditionData
        {
            RunOnType = Condition.RunOnType.Reference,
            Reference = new FormLink<ISkyrimMajorRecordGetter>(PlayerReferenceForm)
        };
        weaponMagicOutCondition.Data = weaponMagicOutData;
        weaponMagicOutCondition.ComparisonValue = 1.0f;
        followerIdleSandbox05Package.Conditions.Add(weaponMagicOutCondition);
        var inCombatCondition = new ConditionFloat
        {
            CompareOperator = CompareOperator.NotEqualTo
        };
        var inCombatData = new IsInCombatConditionData
        {
            RunOnType = Condition.RunOnType.Reference,
            Reference = new FormLink<ISkyrimMajorRecordGetter>(PlayerReferenceForm)
        };
        inCombatCondition.Data = inCombatData;
        inCombatCondition.ComparisonValue = 1.0f;
        followerIdleSandbox05Package.Conditions.Add(inCombatCondition);
        var overEncumberedCondition = new ConditionFloat
        {
            CompareOperator = CompareOperator.NotEqualTo
        };
        var overEncumberedData = new IsOverEncumberedConditionData
        {
            RunOnType = Condition.RunOnType.Reference,
            Reference = new FormLink<ISkyrimMajorRecordGetter>(PlayerReferenceForm)
        };
        overEncumberedCondition.Data = overEncumberedData;
        overEncumberedCondition.ComparisonValue = 1.0f;
        followerIdleSandbox05Package.Conditions.Add(overEncumberedCondition);
        var swimmingCondition = new ConditionFloat
        {
            CompareOperator = CompareOperator.NotEqualTo
        };
        var swimmingData = new IsSwimmingConditionData
        {
            RunOnType = Condition.RunOnType.Reference,
            Reference = new FormLink<ISkyrimMajorRecordGetter>(PlayerReferenceForm)
        };
        swimmingCondition.Data = swimmingData;
        swimmingCondition.ComparisonValue = 1.0f;
        followerIdleSandbox05Package.Conditions.Add(swimmingCondition);
        var trespassingCondition = new ConditionFloat
        {
            CompareOperator = CompareOperator.NotEqualTo
        };
        var trespassingData = new IsTrespassingConditionData
        {
            RunOnType = Condition.RunOnType.Reference,
            Reference = new FormLink<ISkyrimMajorRecordGetter>(PlayerReferenceForm)
        };
        trespassingCondition.Data = trespassingData;
        trespassingCondition.ComparisonValue = 1.0f;
        followerIdleSandbox05Package.Conditions.Add(trespassingCondition);
        var weaponOutCondition = new ConditionFloat
        {
            CompareOperator = CompareOperator.NotEqualTo
        };
        var weaponOutData = new IsWeaponOutConditionData
        {
            RunOnType = Condition.RunOnType.Reference,
            Reference = new FormLink<ISkyrimMajorRecordGetter>(PlayerReferenceForm)
        };
        weaponOutCondition.Data = weaponOutData;
        weaponOutCondition.ComparisonValue = 1.0f;
        followerIdleSandbox05Package.Conditions.Add(weaponOutCondition);
        followerIdleSandbox05Package.PackageTemplate = new FormLink<IPackageGetter>(SkyrimForm(0x01C254));
        followerIdleSandbox05Package.DataInputVersion = 10;
        var locationInput0 = new PackageDataLocation();
        var locationTargetRadius = new LocationTargetRadius();
        var locationTarget = new LocationTarget
        {
            Link = new FormLink<IPlacedGetter>(PlayerReferenceForm)
        };
        locationTargetRadius.Target = locationTarget;
        locationTargetRadius.Radius = 1024;
        locationInput0.Location = locationTargetRadius;
        followerIdleSandbox05Package.Data.Add((sbyte)(0), locationInput0);
        var boolInput14 = new PackageDataBool();
        followerIdleSandbox05Package.Data.Add((sbyte)(14), boolInput14);
        var boolInput1 = new PackageDataBool
        {
            Data = true
        };
        followerIdleSandbox05Package.Data.Add((sbyte)(1), boolInput1);
        var boolInput3 = new PackageDataBool();
        followerIdleSandbox05Package.Data.Add((sbyte)(3), boolInput3);
        var boolInput4 = new PackageDataBool
        {
            Data = true
        };
        followerIdleSandbox05Package.Data.Add((sbyte)(4), boolInput4);
        var boolInput5 = new PackageDataBool
        {
            Data = true
        };
        followerIdleSandbox05Package.Data.Add((sbyte)(5), boolInput5);
        var boolInput6 = new PackageDataBool
        {
            Data = true
        };
        followerIdleSandbox05Package.Data.Add((sbyte)(6), boolInput6);
        var boolInput31 = new PackageDataBool
        {
            Data = true
        };
        followerIdleSandbox05Package.Data.Add((sbyte)(31), boolInput31);
        var boolInput7 = new PackageDataBool();
        followerIdleSandbox05Package.Data.Add((sbyte)(7), boolInput7);
        var boolInput25 = new PackageDataBool();
        followerIdleSandbox05Package.Data.Add((sbyte)(25), boolInput25);
        var boolInput27 = new PackageDataBool();
        followerIdleSandbox05Package.Data.Add((sbyte)(27), boolInput27);
        var floatInput29 = new PackageDataFloat
        {
            Data = 50.0f
        };
        followerIdleSandbox05Package.Data.Add((sbyte)(29), floatInput29);
        followerIdleSandbox05Package.XnamMarker = new MemorySlice<byte>(Convert.FromHexString("20"));
        var onBeginEventEvent = new PackageEvent();
        var eventTopic = new TopicReference();
        onBeginEventEvent.Topics.Add(eventTopic);
        followerIdleSandbox05Package.OnBegin = onBeginEventEvent;
        var onEndEventEvent = new PackageEvent();
        var eventTopic2 = new TopicReference();
        onEndEventEvent.Topics.Add(eventTopic2);
        followerIdleSandbox05Package.OnEnd = onEndEventEvent;
        var onChangeEventEvent = new PackageEvent();
        var eventTopic3 = new TopicReference();
        onChangeEventEvent.Topics.Add(eventTopic3);
        followerIdleSandbox05Package.OnChange = onChangeEventEvent;
        return followerIdleSandbox05Package;
    }

    private static Package CreateFollowerIdleSandbox06Package()
    {
        var followerIdleSandbox06Package = new Package(OwnForm(0x000009), SkyrimRelease.SkyrimSE)
        {
            EditorID = "YLIWF_FollowerIdleSandbox06",
            Type = Package.Types.Package,
            PreferredSpeed = Package.Speed.Run,
            InterruptFlags = Package.InterruptFlag.HellosToPlayer | Package.InterruptFlag.RandomConversations | Package.InterruptFlag.ObserveCombatBehavior | Package.InterruptFlag.GreetCorpseBehavior | Package.InterruptFlag.ReactionToPlayerActions | Package.InterruptFlag.FriendlyFireComments | Package.InterruptFlag.AggroRadiusBehavior | Package.InterruptFlag.AllowIdleChatter | Package.InterruptFlag.WorldInteractions,
            ScheduleMonth = (sbyte)(-1),
            ScheduleDayOfWeek = Package.DayOfWeek.Any,
            ScheduleHour = (sbyte)(-1),
            ScheduleMinute = (sbyte)(-1),
        }.WithUnusedScheduleBytes();
        var globalValueCondition = new ConditionFloat();
        var globalValueData = new GetGlobalValueConditionData();
        globalValueData.Global.Link.SetTo(OwnForm(0x000805));
        globalValueCondition.Data = globalValueData;
        globalValueCondition.ComparisonValue = 1.0f;
        followerIdleSandbox06Package.Conditions.Add(globalValueCondition);
        var actorValueCondition = new ConditionFloat();
        var actorValueData = new GetActorValueConditionData
        {
            ActorValue = ActorValue.WaitingForPlayer
        };
        actorValueCondition.Data = actorValueData;
        followerIdleSandbox06Package.Conditions.Add(actorValueCondition);
        var locationHasKeywordCondition = new ConditionFloat
        {
            Flags = Condition.Flag.OR
        };
        var locationHasKeywordData = new LocationHasKeywordConditionData();
        locationHasKeywordData.Keyword.Link.SetTo(SkyrimForm(0x039793));
        locationHasKeywordCondition.Data = locationHasKeywordData;
        locationHasKeywordCondition.ComparisonValue = 1.0f;
        followerIdleSandbox06Package.Conditions.Add(locationHasKeywordCondition);
        var locationHasKeywordCondition2 = new ConditionFloat();
        var locationHasKeywordData2 = new LocationHasKeywordConditionData();
        locationHasKeywordData2.Keyword.Link.SetTo(SkyrimForm(0x0130DC));
        locationHasKeywordCondition2.Data = locationHasKeywordData2;
        locationHasKeywordCondition2.ComparisonValue = 1.0f;
        followerIdleSandbox06Package.Conditions.Add(locationHasKeywordCondition2);
        var distanceCondition = new ConditionFloat
        {
            CompareOperator = CompareOperator.LessThanOrEqualTo
        };
        var distanceData = new GetDistanceConditionData();
        distanceData.Target.Link.SetTo(PlayerReferenceForm);
        distanceCondition.Data = distanceData;
        distanceCondition.ComparisonValue = 1024.0f;
        followerIdleSandbox06Package.Conditions.Add(distanceCondition);
        var weaponMagicOutCondition = new ConditionFloat
        {
            CompareOperator = CompareOperator.NotEqualTo
        };
        var weaponMagicOutData = new IsWeaponMagicOutConditionData
        {
            RunOnType = Condition.RunOnType.Reference,
            Reference = new FormLink<ISkyrimMajorRecordGetter>(PlayerReferenceForm)
        };
        weaponMagicOutCondition.Data = weaponMagicOutData;
        weaponMagicOutCondition.ComparisonValue = 1.0f;
        followerIdleSandbox06Package.Conditions.Add(weaponMagicOutCondition);
        var inCombatCondition = new ConditionFloat
        {
            CompareOperator = CompareOperator.NotEqualTo
        };
        var inCombatData = new IsInCombatConditionData
        {
            RunOnType = Condition.RunOnType.Reference,
            Reference = new FormLink<ISkyrimMajorRecordGetter>(PlayerReferenceForm)
        };
        inCombatCondition.Data = inCombatData;
        inCombatCondition.ComparisonValue = 1.0f;
        followerIdleSandbox06Package.Conditions.Add(inCombatCondition);
        var overEncumberedCondition = new ConditionFloat
        {
            CompareOperator = CompareOperator.NotEqualTo
        };
        var overEncumberedData = new IsOverEncumberedConditionData
        {
            RunOnType = Condition.RunOnType.Reference,
            Reference = new FormLink<ISkyrimMajorRecordGetter>(PlayerReferenceForm)
        };
        overEncumberedCondition.Data = overEncumberedData;
        overEncumberedCondition.ComparisonValue = 1.0f;
        followerIdleSandbox06Package.Conditions.Add(overEncumberedCondition);
        var swimmingCondition = new ConditionFloat
        {
            CompareOperator = CompareOperator.NotEqualTo
        };
        var swimmingData = new IsSwimmingConditionData
        {
            RunOnType = Condition.RunOnType.Reference,
            Reference = new FormLink<ISkyrimMajorRecordGetter>(PlayerReferenceForm)
        };
        swimmingCondition.Data = swimmingData;
        swimmingCondition.ComparisonValue = 1.0f;
        followerIdleSandbox06Package.Conditions.Add(swimmingCondition);
        var trespassingCondition = new ConditionFloat
        {
            CompareOperator = CompareOperator.NotEqualTo
        };
        var trespassingData = new IsTrespassingConditionData
        {
            RunOnType = Condition.RunOnType.Reference,
            Reference = new FormLink<ISkyrimMajorRecordGetter>(PlayerReferenceForm)
        };
        trespassingCondition.Data = trespassingData;
        trespassingCondition.ComparisonValue = 1.0f;
        followerIdleSandbox06Package.Conditions.Add(trespassingCondition);
        var weaponOutCondition = new ConditionFloat
        {
            CompareOperator = CompareOperator.NotEqualTo
        };
        var weaponOutData = new IsWeaponOutConditionData
        {
            RunOnType = Condition.RunOnType.Reference,
            Reference = new FormLink<ISkyrimMajorRecordGetter>(PlayerReferenceForm)
        };
        weaponOutCondition.Data = weaponOutData;
        weaponOutCondition.ComparisonValue = 1.0f;
        followerIdleSandbox06Package.Conditions.Add(weaponOutCondition);
        followerIdleSandbox06Package.PackageTemplate = new FormLink<IPackageGetter>(SkyrimForm(0x01C254));
        followerIdleSandbox06Package.DataInputVersion = 10;
        var locationInput0 = new PackageDataLocation();
        var locationTargetRadius = new LocationTargetRadius();
        var locationTarget = new LocationTarget
        {
            Link = new FormLink<IPlacedGetter>(PlayerReferenceForm)
        };
        locationTargetRadius.Target = locationTarget;
        locationTargetRadius.Radius = 1024;
        locationInput0.Location = locationTargetRadius;
        followerIdleSandbox06Package.Data.Add((sbyte)(0), locationInput0);
        var boolInput14 = new PackageDataBool();
        followerIdleSandbox06Package.Data.Add((sbyte)(14), boolInput14);
        var boolInput1 = new PackageDataBool
        {
            Data = true
        };
        followerIdleSandbox06Package.Data.Add((sbyte)(1), boolInput1);
        var boolInput3 = new PackageDataBool();
        followerIdleSandbox06Package.Data.Add((sbyte)(3), boolInput3);
        var boolInput4 = new PackageDataBool
        {
            Data = true
        };
        followerIdleSandbox06Package.Data.Add((sbyte)(4), boolInput4);
        var boolInput5 = new PackageDataBool
        {
            Data = true
        };
        followerIdleSandbox06Package.Data.Add((sbyte)(5), boolInput5);
        var boolInput6 = new PackageDataBool
        {
            Data = true
        };
        followerIdleSandbox06Package.Data.Add((sbyte)(6), boolInput6);
        var boolInput31 = new PackageDataBool
        {
            Data = true
        };
        followerIdleSandbox06Package.Data.Add((sbyte)(31), boolInput31);
        var boolInput7 = new PackageDataBool();
        followerIdleSandbox06Package.Data.Add((sbyte)(7), boolInput7);
        var boolInput25 = new PackageDataBool();
        followerIdleSandbox06Package.Data.Add((sbyte)(25), boolInput25);
        var boolInput27 = new PackageDataBool();
        followerIdleSandbox06Package.Data.Add((sbyte)(27), boolInput27);
        var floatInput29 = new PackageDataFloat
        {
            Data = 50.0f
        };
        followerIdleSandbox06Package.Data.Add((sbyte)(29), floatInput29);
        followerIdleSandbox06Package.XnamMarker = new MemorySlice<byte>(Convert.FromHexString("20"));
        var onBeginEventEvent = new PackageEvent();
        var eventTopic = new TopicReference();
        onBeginEventEvent.Topics.Add(eventTopic);
        followerIdleSandbox06Package.OnBegin = onBeginEventEvent;
        var onEndEventEvent = new PackageEvent();
        var eventTopic2 = new TopicReference();
        onEndEventEvent.Topics.Add(eventTopic2);
        followerIdleSandbox06Package.OnEnd = onEndEventEvent;
        var onChangeEventEvent = new PackageEvent();
        var eventTopic3 = new TopicReference();
        onChangeEventEvent.Topics.Add(eventTopic3);
        followerIdleSandbox06Package.OnChange = onChangeEventEvent;
        return followerIdleSandbox06Package;
    }

    private static Package CreateFollowerIdleSandbox07Package()
    {
        var followerIdleSandbox07Package = new Package(OwnForm(0x000003), SkyrimRelease.SkyrimSE)
        {
            EditorID = "YLIWF_FollowerIdleSandbox07",
            Type = Package.Types.Package,
            PreferredSpeed = Package.Speed.Run,
            InterruptFlags = Package.InterruptFlag.HellosToPlayer | Package.InterruptFlag.RandomConversations | Package.InterruptFlag.ObserveCombatBehavior | Package.InterruptFlag.GreetCorpseBehavior | Package.InterruptFlag.ReactionToPlayerActions | Package.InterruptFlag.FriendlyFireComments | Package.InterruptFlag.AggroRadiusBehavior | Package.InterruptFlag.AllowIdleChatter | Package.InterruptFlag.WorldInteractions,
            ScheduleMonth = (sbyte)(-1),
            ScheduleDayOfWeek = Package.DayOfWeek.Any,
            ScheduleHour = (sbyte)(-1),
            ScheduleMinute = (sbyte)(-1),
        }.WithUnusedScheduleBytes();
        var globalValueCondition = new ConditionFloat();
        var globalValueData = new GetGlobalValueConditionData();
        globalValueData.Global.Link.SetTo(OwnForm(0x000805));
        globalValueCondition.Data = globalValueData;
        globalValueCondition.ComparisonValue = 1.0f;
        followerIdleSandbox07Package.Conditions.Add(globalValueCondition);
        var actorValueCondition = new ConditionFloat();
        var actorValueData = new GetActorValueConditionData
        {
            ActorValue = ActorValue.WaitingForPlayer
        };
        actorValueCondition.Data = actorValueData;
        followerIdleSandbox07Package.Conditions.Add(actorValueCondition);
        var locationHasKeywordCondition = new ConditionFloat
        {
            Flags = Condition.Flag.OR
        };
        var locationHasKeywordData = new LocationHasKeywordConditionData();
        locationHasKeywordData.Keyword.Link.SetTo(SkyrimForm(0x039793));
        locationHasKeywordCondition.Data = locationHasKeywordData;
        locationHasKeywordCondition.ComparisonValue = 1.0f;
        followerIdleSandbox07Package.Conditions.Add(locationHasKeywordCondition);
        var locationHasKeywordCondition2 = new ConditionFloat();
        var locationHasKeywordData2 = new LocationHasKeywordConditionData();
        locationHasKeywordData2.Keyword.Link.SetTo(SkyrimForm(0x0130DC));
        locationHasKeywordCondition2.Data = locationHasKeywordData2;
        locationHasKeywordCondition2.ComparisonValue = 1.0f;
        followerIdleSandbox07Package.Conditions.Add(locationHasKeywordCondition2);
        var distanceCondition = new ConditionFloat
        {
            CompareOperator = CompareOperator.LessThanOrEqualTo
        };
        var distanceData = new GetDistanceConditionData();
        distanceData.Target.Link.SetTo(PlayerReferenceForm);
        distanceCondition.Data = distanceData;
        distanceCondition.ComparisonValue = 1024.0f;
        followerIdleSandbox07Package.Conditions.Add(distanceCondition);
        var weaponMagicOutCondition = new ConditionFloat
        {
            CompareOperator = CompareOperator.NotEqualTo
        };
        var weaponMagicOutData = new IsWeaponMagicOutConditionData
        {
            RunOnType = Condition.RunOnType.Reference,
            Reference = new FormLink<ISkyrimMajorRecordGetter>(PlayerReferenceForm)
        };
        weaponMagicOutCondition.Data = weaponMagicOutData;
        weaponMagicOutCondition.ComparisonValue = 1.0f;
        followerIdleSandbox07Package.Conditions.Add(weaponMagicOutCondition);
        var inCombatCondition = new ConditionFloat
        {
            CompareOperator = CompareOperator.NotEqualTo
        };
        var inCombatData = new IsInCombatConditionData
        {
            RunOnType = Condition.RunOnType.Reference,
            Reference = new FormLink<ISkyrimMajorRecordGetter>(PlayerReferenceForm)
        };
        inCombatCondition.Data = inCombatData;
        inCombatCondition.ComparisonValue = 1.0f;
        followerIdleSandbox07Package.Conditions.Add(inCombatCondition);
        var overEncumberedCondition = new ConditionFloat
        {
            CompareOperator = CompareOperator.NotEqualTo
        };
        var overEncumberedData = new IsOverEncumberedConditionData
        {
            RunOnType = Condition.RunOnType.Reference,
            Reference = new FormLink<ISkyrimMajorRecordGetter>(PlayerReferenceForm)
        };
        overEncumberedCondition.Data = overEncumberedData;
        overEncumberedCondition.ComparisonValue = 1.0f;
        followerIdleSandbox07Package.Conditions.Add(overEncumberedCondition);
        var swimmingCondition = new ConditionFloat
        {
            CompareOperator = CompareOperator.NotEqualTo
        };
        var swimmingData = new IsSwimmingConditionData
        {
            RunOnType = Condition.RunOnType.Reference,
            Reference = new FormLink<ISkyrimMajorRecordGetter>(PlayerReferenceForm)
        };
        swimmingCondition.Data = swimmingData;
        swimmingCondition.ComparisonValue = 1.0f;
        followerIdleSandbox07Package.Conditions.Add(swimmingCondition);
        var trespassingCondition = new ConditionFloat
        {
            CompareOperator = CompareOperator.NotEqualTo
        };
        var trespassingData = new IsTrespassingConditionData
        {
            RunOnType = Condition.RunOnType.Reference,
            Reference = new FormLink<ISkyrimMajorRecordGetter>(PlayerReferenceForm)
        };
        trespassingCondition.Data = trespassingData;
        trespassingCondition.ComparisonValue = 1.0f;
        followerIdleSandbox07Package.Conditions.Add(trespassingCondition);
        var weaponOutCondition = new ConditionFloat
        {
            CompareOperator = CompareOperator.NotEqualTo
        };
        var weaponOutData = new IsWeaponOutConditionData
        {
            RunOnType = Condition.RunOnType.Reference,
            Reference = new FormLink<ISkyrimMajorRecordGetter>(PlayerReferenceForm)
        };
        weaponOutCondition.Data = weaponOutData;
        weaponOutCondition.ComparisonValue = 1.0f;
        followerIdleSandbox07Package.Conditions.Add(weaponOutCondition);
        followerIdleSandbox07Package.PackageTemplate = new FormLink<IPackageGetter>(SkyrimForm(0x01C254));
        followerIdleSandbox07Package.DataInputVersion = 10;
        var locationInput0 = new PackageDataLocation();
        var locationTargetRadius = new LocationTargetRadius();
        var locationTarget = new LocationTarget
        {
            Link = new FormLink<IPlacedGetter>(PlayerReferenceForm)
        };
        locationTargetRadius.Target = locationTarget;
        locationTargetRadius.Radius = 1024;
        locationInput0.Location = locationTargetRadius;
        followerIdleSandbox07Package.Data.Add((sbyte)(0), locationInput0);
        var boolInput14 = new PackageDataBool();
        followerIdleSandbox07Package.Data.Add((sbyte)(14), boolInput14);
        var boolInput1 = new PackageDataBool
        {
            Data = true
        };
        followerIdleSandbox07Package.Data.Add((sbyte)(1), boolInput1);
        var boolInput3 = new PackageDataBool();
        followerIdleSandbox07Package.Data.Add((sbyte)(3), boolInput3);
        var boolInput4 = new PackageDataBool
        {
            Data = true
        };
        followerIdleSandbox07Package.Data.Add((sbyte)(4), boolInput4);
        var boolInput5 = new PackageDataBool
        {
            Data = true
        };
        followerIdleSandbox07Package.Data.Add((sbyte)(5), boolInput5);
        var boolInput6 = new PackageDataBool
        {
            Data = true
        };
        followerIdleSandbox07Package.Data.Add((sbyte)(6), boolInput6);
        var boolInput31 = new PackageDataBool
        {
            Data = true
        };
        followerIdleSandbox07Package.Data.Add((sbyte)(31), boolInput31);
        var boolInput7 = new PackageDataBool();
        followerIdleSandbox07Package.Data.Add((sbyte)(7), boolInput7);
        var boolInput25 = new PackageDataBool();
        followerIdleSandbox07Package.Data.Add((sbyte)(25), boolInput25);
        var boolInput27 = new PackageDataBool();
        followerIdleSandbox07Package.Data.Add((sbyte)(27), boolInput27);
        var floatInput29 = new PackageDataFloat
        {
            Data = 50.0f
        };
        followerIdleSandbox07Package.Data.Add((sbyte)(29), floatInput29);
        followerIdleSandbox07Package.XnamMarker = new MemorySlice<byte>(Convert.FromHexString("20"));
        var onBeginEventEvent = new PackageEvent();
        var eventTopic = new TopicReference();
        onBeginEventEvent.Topics.Add(eventTopic);
        followerIdleSandbox07Package.OnBegin = onBeginEventEvent;
        var onEndEventEvent = new PackageEvent();
        var eventTopic2 = new TopicReference();
        onEndEventEvent.Topics.Add(eventTopic2);
        followerIdleSandbox07Package.OnEnd = onEndEventEvent;
        var onChangeEventEvent = new PackageEvent();
        var eventTopic3 = new TopicReference();
        onChangeEventEvent.Topics.Add(eventTopic3);
        followerIdleSandbox07Package.OnChange = onChangeEventEvent;
        return followerIdleSandbox07Package;
    }

    private static Package CreateFollowerSayDismissPackage()
    {
        var followerSayDismissPackage = new Package(OwnForm(0x000806), SkyrimRelease.SkyrimSE)
        {
            EditorID = "YLIWF_FollowerSayDismissPackage",
            Type = Package.Types.Package,
            PreferredSpeed = Package.Speed.Run,
            // PKDT: undocumented byte; preserve the original value. See layout references above.
            Unknown = (byte)(114),
            InterruptFlags = Package.InterruptFlag.HellosToPlayer | Package.InterruptFlag.RandomConversations | Package.InterruptFlag.ObserveCombatBehavior | Package.InterruptFlag.GreetCorpseBehavior | Package.InterruptFlag.ReactionToPlayerActions | Package.InterruptFlag.FriendlyFireComments | Package.InterruptFlag.AggroRadiusBehavior | Package.InterruptFlag.AllowIdleChatter | Package.InterruptFlag.WorldInteractions | (Package.InterruptFlag)0x400 | (Package.InterruptFlag)0x800 | (Package.InterruptFlag)0x1000 | (Package.InterruptFlag)0x2000 | (Package.InterruptFlag)0x4000 | (Package.InterruptFlag)0x8000,
            ScheduleMonth = (sbyte)(-1),
            ScheduleDayOfWeek = Package.DayOfWeek.Any,
            ScheduleHour = (sbyte)(-1),
            ScheduleMinute = (sbyte)(-1),
        }.WithUnusedScheduleBytes();
        var vmQuestVariableCondition = new ConditionFloat();
        var vmQuestVariableData = new GetVMQuestVariableConditionData();
        vmQuestVariableData.Quest.Link.SetTo(DialogueFollowerQuestForm);
        vmQuestVariableData.VariableName = "::iFollowerDismiss_var";
        vmQuestVariableCondition.Data = vmQuestVariableData;
        vmQuestVariableCondition.ComparisonValue = 1.0f;
        followerSayDismissPackage.Conditions.Add(vmQuestVariableCondition);
        var inFactionCondition = new ConditionFloat();
        var inFactionData = new GetInFactionConditionData();
        inFactionData.Faction.Link.SetTo(SkyrimForm(0x05C84C));
        inFactionCondition.Data = inFactionData;
        inFactionCondition.ComparisonValue = 1.0f;
        followerSayDismissPackage.Conditions.Add(inFactionCondition);
        followerSayDismissPackage.PackageTemplate = new FormLink<IPackageGetter>(SkyrimForm(0x01CCB6));
        followerSayDismissPackage.DataInputVersion = 5;
        var topicInput0 = new PackageDataTopic();
        var eventTopic = new TopicReference
        {
            Reference = new FormLink<IDialogTopicGetter>(SkyrimForm(0x05C80C))
        };
        topicInput0.Topics.Add(eventTopic);
        followerSayDismissPackage.Data.Add((sbyte)(0), topicInput0);
        var targetInput1 = new PackageDataTarget
        {
            Type = PackageDataTarget.Types.SingleRef
        };
        var packageTargetSpecificReference = new PackageTargetSpecificReference
        {
            Reference = new FormLink<IPlacedGetter>(PlayerReferenceForm)
        };
        targetInput1.Target = packageTargetSpecificReference;
        followerSayDismissPackage.Data.Add((sbyte)(1), targetInput1);
        var boolInput2 = new PackageDataBool
        {
            Data = true
        };
        followerSayDismissPackage.Data.Add((sbyte)(2), boolInput2);
        var locationInput3 = new PackageDataLocation();
        var locationTargetRadius = new LocationTargetRadius();
        var locationFallback = new LocationFallback
        {
            Type = LocationTargetRadius.LocationType.NearPackageStart
        };
        locationTargetRadius.Target = locationFallback;
        locationTargetRadius.Radius = 32;
        locationInput3.Location = locationTargetRadius;
        followerSayDismissPackage.Data.Add((sbyte)(3), locationInput3);
        var boolInput4 = new PackageDataBool
        {
            Data = true
        };
        followerSayDismissPackage.Data.Add((sbyte)(4), boolInput4);
        var boolInput5 = new PackageDataBool();
        followerSayDismissPackage.Data.Add((sbyte)(5), boolInput5);
        followerSayDismissPackage.XnamMarker = new MemorySlice<byte>(Convert.FromHexString("06"));
        var onBeginEventEvent = new PackageEvent();
        var eventTopic2 = new TopicReference();
        onBeginEventEvent.Topics.Add(eventTopic2);
        followerSayDismissPackage.OnBegin = onBeginEventEvent;
        var onEndEventEvent = new PackageEvent();
        var eventTopic3 = new TopicReference();
        onEndEventEvent.Topics.Add(eventTopic3);
        followerSayDismissPackage.OnEnd = onEndEventEvent;
        var onChangeEventEvent = new PackageEvent();
        var eventTopic4 = new TopicReference();
        onChangeEventEvent.Topics.Add(eventTopic4);
        followerSayDismissPackage.OnChange = onChangeEventEvent;
        return followerSayDismissPackage;
    }

    private static Package CreateHomeSandbox00Package()
    {
        var homeSandbox00Package = new Package(OwnForm(0x000970), SkyrimRelease.SkyrimSE)
        {
            EditorID = "YLIWF_HomeSandbox00",
            Type = Package.Types.Package,
            PreferredSpeed = Package.Speed.Run,
            InterruptFlags = Package.InterruptFlag.HellosToPlayer | Package.InterruptFlag.RandomConversations | Package.InterruptFlag.ObserveCombatBehavior | Package.InterruptFlag.GreetCorpseBehavior | Package.InterruptFlag.ReactionToPlayerActions | Package.InterruptFlag.FriendlyFireComments | Package.InterruptFlag.AggroRadiusBehavior | Package.InterruptFlag.AllowIdleChatter | Package.InterruptFlag.WorldInteractions,
            ScheduleMonth = (sbyte)(-1),
            ScheduleDayOfWeek = Package.DayOfWeek.Any,
            ScheduleHour = (sbyte)(-1),
            ScheduleMinute = (sbyte)(-1),
        }.WithUnusedScheduleBytes();
        var playerTeammateCondition = new ConditionFloat();
        var playerTeammateData = new GetPlayerTeammateConditionData();
        playerTeammateCondition.Data = playerTeammateData;
        homeSandbox00Package.Conditions.Add(playerTeammateCondition);
        var globalValueCondition = new ConditionFloat();
        var globalValueData = new GetGlobalValueConditionData();
        globalValueData.Global.Link.SetTo(OwnForm(0x000986));
        globalValueCondition.Data = globalValueData;
        globalValueCondition.ComparisonValue = 1.0f;
        homeSandbox00Package.Conditions.Add(globalValueCondition);
        homeSandbox00Package.PackageTemplate = new FormLink<IPackageGetter>(SkyrimForm(0x01C254));
        homeSandbox00Package.DataInputVersion = 10;
        var locationInput0 = new PackageDataLocation();
        var locationTargetRadius = new LocationTargetRadius();
        var locationTarget = new LocationTarget
        {
            Link = new FormLink<IPlacedGetter>(OwnForm(0x000951))
        };
        locationTargetRadius.Target = locationTarget;
        locationTargetRadius.Radius = 4096;
        locationInput0.Location = locationTargetRadius;
        homeSandbox00Package.Data.Add((sbyte)(0), locationInput0);
        var boolInput14 = new PackageDataBool();
        homeSandbox00Package.Data.Add((sbyte)(14), boolInput14);
        var boolInput1 = new PackageDataBool
        {
            Data = true
        };
        homeSandbox00Package.Data.Add((sbyte)(1), boolInput1);
        var boolInput3 = new PackageDataBool();
        homeSandbox00Package.Data.Add((sbyte)(3), boolInput3);
        var boolInput4 = new PackageDataBool
        {
            Data = true
        };
        homeSandbox00Package.Data.Add((sbyte)(4), boolInput4);
        var boolInput5 = new PackageDataBool
        {
            Data = true
        };
        homeSandbox00Package.Data.Add((sbyte)(5), boolInput5);
        var boolInput6 = new PackageDataBool
        {
            Data = true
        };
        homeSandbox00Package.Data.Add((sbyte)(6), boolInput6);
        var boolInput31 = new PackageDataBool
        {
            Data = true
        };
        homeSandbox00Package.Data.Add((sbyte)(31), boolInput31);
        var boolInput7 = new PackageDataBool();
        homeSandbox00Package.Data.Add((sbyte)(7), boolInput7);
        var boolInput25 = new PackageDataBool();
        homeSandbox00Package.Data.Add((sbyte)(25), boolInput25);
        var boolInput27 = new PackageDataBool();
        homeSandbox00Package.Data.Add((sbyte)(27), boolInput27);
        var floatInput29 = new PackageDataFloat
        {
            Data = 50.0f
        };
        homeSandbox00Package.Data.Add((sbyte)(29), floatInput29);
        homeSandbox00Package.XnamMarker = new MemorySlice<byte>(Convert.FromHexString("20"));
        var onBeginEventEvent = new PackageEvent();
        var eventTopic = new TopicReference();
        onBeginEventEvent.Topics.Add(eventTopic);
        homeSandbox00Package.OnBegin = onBeginEventEvent;
        var onEndEventEvent = new PackageEvent();
        var eventTopic2 = new TopicReference();
        onEndEventEvent.Topics.Add(eventTopic2);
        homeSandbox00Package.OnEnd = onEndEventEvent;
        var onChangeEventEvent = new PackageEvent();
        var eventTopic3 = new TopicReference();
        onChangeEventEvent.Topics.Add(eventTopic3);
        homeSandbox00Package.OnChange = onChangeEventEvent;
        return homeSandbox00Package;
    }

    private static Package CreateHomeSandbox01Package()
    {
        var homeSandbox01Package = new Package(OwnForm(0x000971), SkyrimRelease.SkyrimSE)
        {
            EditorID = "YLIWF_HomeSandbox01",
            Type = Package.Types.Package,
            PreferredSpeed = Package.Speed.Run,
            InterruptFlags = Package.InterruptFlag.HellosToPlayer | Package.InterruptFlag.RandomConversations | Package.InterruptFlag.ObserveCombatBehavior | Package.InterruptFlag.GreetCorpseBehavior | Package.InterruptFlag.ReactionToPlayerActions | Package.InterruptFlag.FriendlyFireComments | Package.InterruptFlag.AggroRadiusBehavior | Package.InterruptFlag.AllowIdleChatter | Package.InterruptFlag.WorldInteractions,
            ScheduleMonth = (sbyte)(-1),
            ScheduleDayOfWeek = Package.DayOfWeek.Any,
            ScheduleHour = (sbyte)(-1),
            ScheduleMinute = (sbyte)(-1),
        }.WithUnusedScheduleBytes();
        var playerTeammateCondition = new ConditionFloat();
        var playerTeammateData = new GetPlayerTeammateConditionData();
        playerTeammateCondition.Data = playerTeammateData;
        homeSandbox01Package.Conditions.Add(playerTeammateCondition);
        var globalValueCondition = new ConditionFloat();
        var globalValueData = new GetGlobalValueConditionData();
        globalValueData.Global.Link.SetTo(OwnForm(0x000986));
        globalValueCondition.Data = globalValueData;
        globalValueCondition.ComparisonValue = 1.0f;
        homeSandbox01Package.Conditions.Add(globalValueCondition);
        homeSandbox01Package.PackageTemplate = new FormLink<IPackageGetter>(SkyrimForm(0x01C254));
        homeSandbox01Package.DataInputVersion = 10;
        var locationInput0 = new PackageDataLocation();
        var locationTargetRadius = new LocationTargetRadius();
        var locationTarget = new LocationTarget
        {
            Link = new FormLink<IPlacedGetter>(OwnForm(0x000952))
        };
        locationTargetRadius.Target = locationTarget;
        locationTargetRadius.Radius = 4096;
        locationInput0.Location = locationTargetRadius;
        homeSandbox01Package.Data.Add((sbyte)(0), locationInput0);
        var boolInput14 = new PackageDataBool();
        homeSandbox01Package.Data.Add((sbyte)(14), boolInput14);
        var boolInput1 = new PackageDataBool
        {
            Data = true
        };
        homeSandbox01Package.Data.Add((sbyte)(1), boolInput1);
        var boolInput3 = new PackageDataBool();
        homeSandbox01Package.Data.Add((sbyte)(3), boolInput3);
        var boolInput4 = new PackageDataBool
        {
            Data = true
        };
        homeSandbox01Package.Data.Add((sbyte)(4), boolInput4);
        var boolInput5 = new PackageDataBool
        {
            Data = true
        };
        homeSandbox01Package.Data.Add((sbyte)(5), boolInput5);
        var boolInput6 = new PackageDataBool
        {
            Data = true
        };
        homeSandbox01Package.Data.Add((sbyte)(6), boolInput6);
        var boolInput31 = new PackageDataBool
        {
            Data = true
        };
        homeSandbox01Package.Data.Add((sbyte)(31), boolInput31);
        var boolInput7 = new PackageDataBool();
        homeSandbox01Package.Data.Add((sbyte)(7), boolInput7);
        var boolInput25 = new PackageDataBool();
        homeSandbox01Package.Data.Add((sbyte)(25), boolInput25);
        var boolInput27 = new PackageDataBool();
        homeSandbox01Package.Data.Add((sbyte)(27), boolInput27);
        var floatInput29 = new PackageDataFloat
        {
            Data = 50.0f
        };
        homeSandbox01Package.Data.Add((sbyte)(29), floatInput29);
        homeSandbox01Package.XnamMarker = new MemorySlice<byte>(Convert.FromHexString("20"));
        var onBeginEventEvent = new PackageEvent();
        var eventTopic = new TopicReference();
        onBeginEventEvent.Topics.Add(eventTopic);
        homeSandbox01Package.OnBegin = onBeginEventEvent;
        var onEndEventEvent = new PackageEvent();
        var eventTopic2 = new TopicReference();
        onEndEventEvent.Topics.Add(eventTopic2);
        homeSandbox01Package.OnEnd = onEndEventEvent;
        var onChangeEventEvent = new PackageEvent();
        var eventTopic3 = new TopicReference();
        onChangeEventEvent.Topics.Add(eventTopic3);
        homeSandbox01Package.OnChange = onChangeEventEvent;
        return homeSandbox01Package;
    }

    private static Package CreateHomeSandbox02Package()
    {
        var homeSandbox02Package = new Package(OwnForm(0x000972), SkyrimRelease.SkyrimSE)
        {
            EditorID = "YLIWF_HomeSandbox02",
            Type = Package.Types.Package,
            PreferredSpeed = Package.Speed.Run,
            InterruptFlags = Package.InterruptFlag.HellosToPlayer | Package.InterruptFlag.RandomConversations | Package.InterruptFlag.ObserveCombatBehavior | Package.InterruptFlag.GreetCorpseBehavior | Package.InterruptFlag.ReactionToPlayerActions | Package.InterruptFlag.FriendlyFireComments | Package.InterruptFlag.AggroRadiusBehavior | Package.InterruptFlag.AllowIdleChatter | Package.InterruptFlag.WorldInteractions,
            ScheduleMonth = (sbyte)(-1),
            ScheduleDayOfWeek = Package.DayOfWeek.Any,
            ScheduleHour = (sbyte)(-1),
            ScheduleMinute = (sbyte)(-1),
        }.WithUnusedScheduleBytes();
        var playerTeammateCondition = new ConditionFloat();
        var playerTeammateData = new GetPlayerTeammateConditionData();
        playerTeammateCondition.Data = playerTeammateData;
        homeSandbox02Package.Conditions.Add(playerTeammateCondition);
        var globalValueCondition = new ConditionFloat();
        var globalValueData = new GetGlobalValueConditionData();
        globalValueData.Global.Link.SetTo(OwnForm(0x000986));
        globalValueCondition.Data = globalValueData;
        globalValueCondition.ComparisonValue = 1.0f;
        homeSandbox02Package.Conditions.Add(globalValueCondition);
        homeSandbox02Package.PackageTemplate = new FormLink<IPackageGetter>(SkyrimForm(0x01C254));
        homeSandbox02Package.DataInputVersion = 10;
        var locationInput0 = new PackageDataLocation();
        var locationTargetRadius = new LocationTargetRadius();
        var locationTarget = new LocationTarget
        {
            Link = new FormLink<IPlacedGetter>(OwnForm(0x000953))
        };
        locationTargetRadius.Target = locationTarget;
        locationTargetRadius.Radius = 4096;
        locationInput0.Location = locationTargetRadius;
        homeSandbox02Package.Data.Add((sbyte)(0), locationInput0);
        var boolInput14 = new PackageDataBool();
        homeSandbox02Package.Data.Add((sbyte)(14), boolInput14);
        var boolInput1 = new PackageDataBool
        {
            Data = true
        };
        homeSandbox02Package.Data.Add((sbyte)(1), boolInput1);
        var boolInput3 = new PackageDataBool();
        homeSandbox02Package.Data.Add((sbyte)(3), boolInput3);
        var boolInput4 = new PackageDataBool
        {
            Data = true
        };
        homeSandbox02Package.Data.Add((sbyte)(4), boolInput4);
        var boolInput5 = new PackageDataBool
        {
            Data = true
        };
        homeSandbox02Package.Data.Add((sbyte)(5), boolInput5);
        var boolInput6 = new PackageDataBool
        {
            Data = true
        };
        homeSandbox02Package.Data.Add((sbyte)(6), boolInput6);
        var boolInput31 = new PackageDataBool
        {
            Data = true
        };
        homeSandbox02Package.Data.Add((sbyte)(31), boolInput31);
        var boolInput7 = new PackageDataBool();
        homeSandbox02Package.Data.Add((sbyte)(7), boolInput7);
        var boolInput25 = new PackageDataBool();
        homeSandbox02Package.Data.Add((sbyte)(25), boolInput25);
        var boolInput27 = new PackageDataBool();
        homeSandbox02Package.Data.Add((sbyte)(27), boolInput27);
        var floatInput29 = new PackageDataFloat
        {
            Data = 50.0f
        };
        homeSandbox02Package.Data.Add((sbyte)(29), floatInput29);
        homeSandbox02Package.XnamMarker = new MemorySlice<byte>(Convert.FromHexString("20"));
        var onBeginEventEvent = new PackageEvent();
        var eventTopic = new TopicReference();
        onBeginEventEvent.Topics.Add(eventTopic);
        homeSandbox02Package.OnBegin = onBeginEventEvent;
        var onEndEventEvent = new PackageEvent();
        var eventTopic2 = new TopicReference();
        onEndEventEvent.Topics.Add(eventTopic2);
        homeSandbox02Package.OnEnd = onEndEventEvent;
        var onChangeEventEvent = new PackageEvent();
        var eventTopic3 = new TopicReference();
        onChangeEventEvent.Topics.Add(eventTopic3);
        homeSandbox02Package.OnChange = onChangeEventEvent;
        return homeSandbox02Package;
    }

    private static Package CreateHomeSandbox03Package()
    {
        var homeSandbox03Package = new Package(OwnForm(0x000973), SkyrimRelease.SkyrimSE)
        {
            EditorID = "YLIWF_HomeSandbox03",
            Type = Package.Types.Package,
            PreferredSpeed = Package.Speed.Run,
            InterruptFlags = Package.InterruptFlag.HellosToPlayer | Package.InterruptFlag.RandomConversations | Package.InterruptFlag.ObserveCombatBehavior | Package.InterruptFlag.GreetCorpseBehavior | Package.InterruptFlag.ReactionToPlayerActions | Package.InterruptFlag.FriendlyFireComments | Package.InterruptFlag.AggroRadiusBehavior | Package.InterruptFlag.AllowIdleChatter | Package.InterruptFlag.WorldInteractions,
            ScheduleMonth = (sbyte)(-1),
            ScheduleDayOfWeek = Package.DayOfWeek.Any,
            ScheduleHour = (sbyte)(-1),
            ScheduleMinute = (sbyte)(-1),
        }.WithUnusedScheduleBytes();
        var playerTeammateCondition = new ConditionFloat();
        var playerTeammateData = new GetPlayerTeammateConditionData();
        playerTeammateCondition.Data = playerTeammateData;
        homeSandbox03Package.Conditions.Add(playerTeammateCondition);
        var globalValueCondition = new ConditionFloat();
        var globalValueData = new GetGlobalValueConditionData();
        globalValueData.Global.Link.SetTo(OwnForm(0x000986));
        globalValueCondition.Data = globalValueData;
        globalValueCondition.ComparisonValue = 1.0f;
        homeSandbox03Package.Conditions.Add(globalValueCondition);
        homeSandbox03Package.PackageTemplate = new FormLink<IPackageGetter>(SkyrimForm(0x01C254));
        homeSandbox03Package.DataInputVersion = 10;
        var locationInput0 = new PackageDataLocation();
        var locationTargetRadius = new LocationTargetRadius();
        var locationTarget = new LocationTarget
        {
            Link = new FormLink<IPlacedGetter>(OwnForm(0x000954))
        };
        locationTargetRadius.Target = locationTarget;
        locationTargetRadius.Radius = 4096;
        locationInput0.Location = locationTargetRadius;
        homeSandbox03Package.Data.Add((sbyte)(0), locationInput0);
        var boolInput14 = new PackageDataBool();
        homeSandbox03Package.Data.Add((sbyte)(14), boolInput14);
        var boolInput1 = new PackageDataBool
        {
            Data = true
        };
        homeSandbox03Package.Data.Add((sbyte)(1), boolInput1);
        var boolInput3 = new PackageDataBool();
        homeSandbox03Package.Data.Add((sbyte)(3), boolInput3);
        var boolInput4 = new PackageDataBool
        {
            Data = true
        };
        homeSandbox03Package.Data.Add((sbyte)(4), boolInput4);
        var boolInput5 = new PackageDataBool
        {
            Data = true
        };
        homeSandbox03Package.Data.Add((sbyte)(5), boolInput5);
        var boolInput6 = new PackageDataBool
        {
            Data = true
        };
        homeSandbox03Package.Data.Add((sbyte)(6), boolInput6);
        var boolInput31 = new PackageDataBool
        {
            Data = true
        };
        homeSandbox03Package.Data.Add((sbyte)(31), boolInput31);
        var boolInput7 = new PackageDataBool();
        homeSandbox03Package.Data.Add((sbyte)(7), boolInput7);
        var boolInput25 = new PackageDataBool();
        homeSandbox03Package.Data.Add((sbyte)(25), boolInput25);
        var boolInput27 = new PackageDataBool();
        homeSandbox03Package.Data.Add((sbyte)(27), boolInput27);
        var floatInput29 = new PackageDataFloat
        {
            Data = 50.0f
        };
        homeSandbox03Package.Data.Add((sbyte)(29), floatInput29);
        homeSandbox03Package.XnamMarker = new MemorySlice<byte>(Convert.FromHexString("20"));
        var onBeginEventEvent = new PackageEvent();
        var eventTopic = new TopicReference();
        onBeginEventEvent.Topics.Add(eventTopic);
        homeSandbox03Package.OnBegin = onBeginEventEvent;
        var onEndEventEvent = new PackageEvent();
        var eventTopic2 = new TopicReference();
        onEndEventEvent.Topics.Add(eventTopic2);
        homeSandbox03Package.OnEnd = onEndEventEvent;
        var onChangeEventEvent = new PackageEvent();
        var eventTopic3 = new TopicReference();
        onChangeEventEvent.Topics.Add(eventTopic3);
        homeSandbox03Package.OnChange = onChangeEventEvent;
        return homeSandbox03Package;
    }

    private static Package CreateHomeSandbox04Package()
    {
        var homeSandbox04Package = new Package(OwnForm(0x000974), SkyrimRelease.SkyrimSE)
        {
            EditorID = "YLIWF_HomeSandbox04",
            Type = Package.Types.Package,
            PreferredSpeed = Package.Speed.Run,
            InterruptFlags = Package.InterruptFlag.HellosToPlayer | Package.InterruptFlag.RandomConversations | Package.InterruptFlag.ObserveCombatBehavior | Package.InterruptFlag.GreetCorpseBehavior | Package.InterruptFlag.ReactionToPlayerActions | Package.InterruptFlag.FriendlyFireComments | Package.InterruptFlag.AggroRadiusBehavior | Package.InterruptFlag.AllowIdleChatter | Package.InterruptFlag.WorldInteractions,
            ScheduleMonth = (sbyte)(-1),
            ScheduleDayOfWeek = Package.DayOfWeek.Any,
            ScheduleHour = (sbyte)(-1),
            ScheduleMinute = (sbyte)(-1),
        }.WithUnusedScheduleBytes();
        var playerTeammateCondition = new ConditionFloat();
        var playerTeammateData = new GetPlayerTeammateConditionData();
        playerTeammateCondition.Data = playerTeammateData;
        homeSandbox04Package.Conditions.Add(playerTeammateCondition);
        var globalValueCondition = new ConditionFloat();
        var globalValueData = new GetGlobalValueConditionData();
        globalValueData.Global.Link.SetTo(OwnForm(0x000986));
        globalValueCondition.Data = globalValueData;
        globalValueCondition.ComparisonValue = 1.0f;
        homeSandbox04Package.Conditions.Add(globalValueCondition);
        homeSandbox04Package.PackageTemplate = new FormLink<IPackageGetter>(SkyrimForm(0x01C254));
        homeSandbox04Package.DataInputVersion = 10;
        var locationInput0 = new PackageDataLocation();
        var locationTargetRadius = new LocationTargetRadius();
        var locationTarget = new LocationTarget
        {
            Link = new FormLink<IPlacedGetter>(OwnForm(0x000955))
        };
        locationTargetRadius.Target = locationTarget;
        locationTargetRadius.Radius = 4096;
        locationInput0.Location = locationTargetRadius;
        homeSandbox04Package.Data.Add((sbyte)(0), locationInput0);
        var boolInput14 = new PackageDataBool();
        homeSandbox04Package.Data.Add((sbyte)(14), boolInput14);
        var boolInput1 = new PackageDataBool
        {
            Data = true
        };
        homeSandbox04Package.Data.Add((sbyte)(1), boolInput1);
        var boolInput3 = new PackageDataBool();
        homeSandbox04Package.Data.Add((sbyte)(3), boolInput3);
        var boolInput4 = new PackageDataBool
        {
            Data = true
        };
        homeSandbox04Package.Data.Add((sbyte)(4), boolInput4);
        var boolInput5 = new PackageDataBool
        {
            Data = true
        };
        homeSandbox04Package.Data.Add((sbyte)(5), boolInput5);
        var boolInput6 = new PackageDataBool
        {
            Data = true
        };
        homeSandbox04Package.Data.Add((sbyte)(6), boolInput6);
        var boolInput31 = new PackageDataBool
        {
            Data = true
        };
        homeSandbox04Package.Data.Add((sbyte)(31), boolInput31);
        var boolInput7 = new PackageDataBool();
        homeSandbox04Package.Data.Add((sbyte)(7), boolInput7);
        var boolInput25 = new PackageDataBool();
        homeSandbox04Package.Data.Add((sbyte)(25), boolInput25);
        var boolInput27 = new PackageDataBool();
        homeSandbox04Package.Data.Add((sbyte)(27), boolInput27);
        var floatInput29 = new PackageDataFloat
        {
            Data = 50.0f
        };
        homeSandbox04Package.Data.Add((sbyte)(29), floatInput29);
        homeSandbox04Package.XnamMarker = new MemorySlice<byte>(Convert.FromHexString("20"));
        var onBeginEventEvent = new PackageEvent();
        var eventTopic = new TopicReference();
        onBeginEventEvent.Topics.Add(eventTopic);
        homeSandbox04Package.OnBegin = onBeginEventEvent;
        var onEndEventEvent = new PackageEvent();
        var eventTopic2 = new TopicReference();
        onEndEventEvent.Topics.Add(eventTopic2);
        homeSandbox04Package.OnEnd = onEndEventEvent;
        var onChangeEventEvent = new PackageEvent();
        var eventTopic3 = new TopicReference();
        onChangeEventEvent.Topics.Add(eventTopic3);
        homeSandbox04Package.OnChange = onChangeEventEvent;
        return homeSandbox04Package;
    }

    private static Package CreateHomeSandbox05Package()
    {
        var homeSandbox05Package = new Package(OwnForm(0x000975), SkyrimRelease.SkyrimSE)
        {
            EditorID = "YLIWF_HomeSandbox05",
            Type = Package.Types.Package,
            PreferredSpeed = Package.Speed.Run,
            InterruptFlags = Package.InterruptFlag.HellosToPlayer | Package.InterruptFlag.RandomConversations | Package.InterruptFlag.ObserveCombatBehavior | Package.InterruptFlag.GreetCorpseBehavior | Package.InterruptFlag.ReactionToPlayerActions | Package.InterruptFlag.FriendlyFireComments | Package.InterruptFlag.AggroRadiusBehavior | Package.InterruptFlag.AllowIdleChatter | Package.InterruptFlag.WorldInteractions,
            ScheduleMonth = (sbyte)(-1),
            ScheduleDayOfWeek = Package.DayOfWeek.Any,
            ScheduleHour = (sbyte)(-1),
            ScheduleMinute = (sbyte)(-1),
        }.WithUnusedScheduleBytes();
        var playerTeammateCondition = new ConditionFloat();
        var playerTeammateData = new GetPlayerTeammateConditionData();
        playerTeammateCondition.Data = playerTeammateData;
        homeSandbox05Package.Conditions.Add(playerTeammateCondition);
        var globalValueCondition = new ConditionFloat();
        var globalValueData = new GetGlobalValueConditionData();
        globalValueData.Global.Link.SetTo(OwnForm(0x000986));
        globalValueCondition.Data = globalValueData;
        globalValueCondition.ComparisonValue = 1.0f;
        homeSandbox05Package.Conditions.Add(globalValueCondition);
        homeSandbox05Package.PackageTemplate = new FormLink<IPackageGetter>(SkyrimForm(0x01C254));
        homeSandbox05Package.DataInputVersion = 10;
        var locationInput0 = new PackageDataLocation();
        var locationTargetRadius = new LocationTargetRadius();
        var locationTarget = new LocationTarget
        {
            Link = new FormLink<IPlacedGetter>(OwnForm(0x000956))
        };
        locationTargetRadius.Target = locationTarget;
        locationTargetRadius.Radius = 4096;
        locationInput0.Location = locationTargetRadius;
        homeSandbox05Package.Data.Add((sbyte)(0), locationInput0);
        var boolInput14 = new PackageDataBool();
        homeSandbox05Package.Data.Add((sbyte)(14), boolInput14);
        var boolInput1 = new PackageDataBool
        {
            Data = true
        };
        homeSandbox05Package.Data.Add((sbyte)(1), boolInput1);
        var boolInput3 = new PackageDataBool();
        homeSandbox05Package.Data.Add((sbyte)(3), boolInput3);
        var boolInput4 = new PackageDataBool
        {
            Data = true
        };
        homeSandbox05Package.Data.Add((sbyte)(4), boolInput4);
        var boolInput5 = new PackageDataBool
        {
            Data = true
        };
        homeSandbox05Package.Data.Add((sbyte)(5), boolInput5);
        var boolInput6 = new PackageDataBool
        {
            Data = true
        };
        homeSandbox05Package.Data.Add((sbyte)(6), boolInput6);
        var boolInput31 = new PackageDataBool
        {
            Data = true
        };
        homeSandbox05Package.Data.Add((sbyte)(31), boolInput31);
        var boolInput7 = new PackageDataBool();
        homeSandbox05Package.Data.Add((sbyte)(7), boolInput7);
        var boolInput25 = new PackageDataBool();
        homeSandbox05Package.Data.Add((sbyte)(25), boolInput25);
        var boolInput27 = new PackageDataBool();
        homeSandbox05Package.Data.Add((sbyte)(27), boolInput27);
        var floatInput29 = new PackageDataFloat
        {
            Data = 50.0f
        };
        homeSandbox05Package.Data.Add((sbyte)(29), floatInput29);
        homeSandbox05Package.XnamMarker = new MemorySlice<byte>(Convert.FromHexString("20"));
        var onBeginEventEvent = new PackageEvent();
        var eventTopic = new TopicReference();
        onBeginEventEvent.Topics.Add(eventTopic);
        homeSandbox05Package.OnBegin = onBeginEventEvent;
        var onEndEventEvent = new PackageEvent();
        var eventTopic2 = new TopicReference();
        onEndEventEvent.Topics.Add(eventTopic2);
        homeSandbox05Package.OnEnd = onEndEventEvent;
        var onChangeEventEvent = new PackageEvent();
        var eventTopic3 = new TopicReference();
        onChangeEventEvent.Topics.Add(eventTopic3);
        homeSandbox05Package.OnChange = onChangeEventEvent;
        return homeSandbox05Package;
    }

    private static Package CreateHomeSandbox06Package()
    {
        var homeSandbox06Package = new Package(OwnForm(0x000976), SkyrimRelease.SkyrimSE)
        {
            EditorID = "YLIWF_HomeSandbox06",
            Type = Package.Types.Package,
            PreferredSpeed = Package.Speed.Run,
            InterruptFlags = Package.InterruptFlag.HellosToPlayer | Package.InterruptFlag.RandomConversations | Package.InterruptFlag.ObserveCombatBehavior | Package.InterruptFlag.GreetCorpseBehavior | Package.InterruptFlag.ReactionToPlayerActions | Package.InterruptFlag.FriendlyFireComments | Package.InterruptFlag.AggroRadiusBehavior | Package.InterruptFlag.AllowIdleChatter | Package.InterruptFlag.WorldInteractions,
            ScheduleMonth = (sbyte)(-1),
            ScheduleDayOfWeek = Package.DayOfWeek.Any,
            ScheduleHour = (sbyte)(-1),
            ScheduleMinute = (sbyte)(-1),
        }.WithUnusedScheduleBytes();
        var playerTeammateCondition = new ConditionFloat();
        var playerTeammateData = new GetPlayerTeammateConditionData();
        playerTeammateCondition.Data = playerTeammateData;
        homeSandbox06Package.Conditions.Add(playerTeammateCondition);
        var globalValueCondition = new ConditionFloat();
        var globalValueData = new GetGlobalValueConditionData();
        globalValueData.Global.Link.SetTo(OwnForm(0x000986));
        globalValueCondition.Data = globalValueData;
        globalValueCondition.ComparisonValue = 1.0f;
        homeSandbox06Package.Conditions.Add(globalValueCondition);
        homeSandbox06Package.PackageTemplate = new FormLink<IPackageGetter>(SkyrimForm(0x01C254));
        homeSandbox06Package.DataInputVersion = 10;
        var locationInput0 = new PackageDataLocation();
        var locationTargetRadius = new LocationTargetRadius();
        var locationTarget = new LocationTarget
        {
            Link = new FormLink<IPlacedGetter>(OwnForm(0x000957))
        };
        locationTargetRadius.Target = locationTarget;
        locationTargetRadius.Radius = 4096;
        locationInput0.Location = locationTargetRadius;
        homeSandbox06Package.Data.Add((sbyte)(0), locationInput0);
        var boolInput14 = new PackageDataBool();
        homeSandbox06Package.Data.Add((sbyte)(14), boolInput14);
        var boolInput1 = new PackageDataBool
        {
            Data = true
        };
        homeSandbox06Package.Data.Add((sbyte)(1), boolInput1);
        var boolInput3 = new PackageDataBool();
        homeSandbox06Package.Data.Add((sbyte)(3), boolInput3);
        var boolInput4 = new PackageDataBool
        {
            Data = true
        };
        homeSandbox06Package.Data.Add((sbyte)(4), boolInput4);
        var boolInput5 = new PackageDataBool
        {
            Data = true
        };
        homeSandbox06Package.Data.Add((sbyte)(5), boolInput5);
        var boolInput6 = new PackageDataBool
        {
            Data = true
        };
        homeSandbox06Package.Data.Add((sbyte)(6), boolInput6);
        var boolInput31 = new PackageDataBool
        {
            Data = true
        };
        homeSandbox06Package.Data.Add((sbyte)(31), boolInput31);
        var boolInput7 = new PackageDataBool();
        homeSandbox06Package.Data.Add((sbyte)(7), boolInput7);
        var boolInput25 = new PackageDataBool();
        homeSandbox06Package.Data.Add((sbyte)(25), boolInput25);
        var boolInput27 = new PackageDataBool();
        homeSandbox06Package.Data.Add((sbyte)(27), boolInput27);
        var floatInput29 = new PackageDataFloat
        {
            Data = 50.0f
        };
        homeSandbox06Package.Data.Add((sbyte)(29), floatInput29);
        homeSandbox06Package.XnamMarker = new MemorySlice<byte>(Convert.FromHexString("20"));
        var onBeginEventEvent = new PackageEvent();
        var eventTopic = new TopicReference();
        onBeginEventEvent.Topics.Add(eventTopic);
        homeSandbox06Package.OnBegin = onBeginEventEvent;
        var onEndEventEvent = new PackageEvent();
        var eventTopic2 = new TopicReference();
        onEndEventEvent.Topics.Add(eventTopic2);
        homeSandbox06Package.OnEnd = onEndEventEvent;
        var onChangeEventEvent = new PackageEvent();
        var eventTopic3 = new TopicReference();
        onChangeEventEvent.Topics.Add(eventTopic3);
        homeSandbox06Package.OnChange = onChangeEventEvent;
        return homeSandbox06Package;
    }

    private static Package CreateHomeSandbox07Package()
    {
        var homeSandbox07Package = new Package(OwnForm(0x000977), SkyrimRelease.SkyrimSE)
        {
            EditorID = "YLIWF_HomeSandbox07",
            Type = Package.Types.Package,
            PreferredSpeed = Package.Speed.Run,
            InterruptFlags = Package.InterruptFlag.HellosToPlayer | Package.InterruptFlag.RandomConversations | Package.InterruptFlag.ObserveCombatBehavior | Package.InterruptFlag.GreetCorpseBehavior | Package.InterruptFlag.ReactionToPlayerActions | Package.InterruptFlag.FriendlyFireComments | Package.InterruptFlag.AggroRadiusBehavior | Package.InterruptFlag.AllowIdleChatter | Package.InterruptFlag.WorldInteractions,
            ScheduleMonth = (sbyte)(-1),
            ScheduleDayOfWeek = Package.DayOfWeek.Any,
            ScheduleHour = (sbyte)(-1),
            ScheduleMinute = (sbyte)(-1),
        }.WithUnusedScheduleBytes();
        var playerTeammateCondition = new ConditionFloat();
        var playerTeammateData = new GetPlayerTeammateConditionData();
        playerTeammateCondition.Data = playerTeammateData;
        homeSandbox07Package.Conditions.Add(playerTeammateCondition);
        var globalValueCondition = new ConditionFloat();
        var globalValueData = new GetGlobalValueConditionData();
        globalValueData.Global.Link.SetTo(OwnForm(0x000986));
        globalValueCondition.Data = globalValueData;
        globalValueCondition.ComparisonValue = 1.0f;
        homeSandbox07Package.Conditions.Add(globalValueCondition);
        homeSandbox07Package.PackageTemplate = new FormLink<IPackageGetter>(SkyrimForm(0x01C254));
        homeSandbox07Package.DataInputVersion = 10;
        var locationInput0 = new PackageDataLocation();
        var locationTargetRadius = new LocationTargetRadius();
        var locationTarget = new LocationTarget
        {
            Link = new FormLink<IPlacedGetter>(OwnForm(0x000958))
        };
        locationTargetRadius.Target = locationTarget;
        locationTargetRadius.Radius = 4096;
        locationInput0.Location = locationTargetRadius;
        homeSandbox07Package.Data.Add((sbyte)(0), locationInput0);
        var boolInput14 = new PackageDataBool();
        homeSandbox07Package.Data.Add((sbyte)(14), boolInput14);
        var boolInput1 = new PackageDataBool
        {
            Data = true
        };
        homeSandbox07Package.Data.Add((sbyte)(1), boolInput1);
        var boolInput3 = new PackageDataBool();
        homeSandbox07Package.Data.Add((sbyte)(3), boolInput3);
        var boolInput4 = new PackageDataBool
        {
            Data = true
        };
        homeSandbox07Package.Data.Add((sbyte)(4), boolInput4);
        var boolInput5 = new PackageDataBool
        {
            Data = true
        };
        homeSandbox07Package.Data.Add((sbyte)(5), boolInput5);
        var boolInput6 = new PackageDataBool
        {
            Data = true
        };
        homeSandbox07Package.Data.Add((sbyte)(6), boolInput6);
        var boolInput31 = new PackageDataBool
        {
            Data = true
        };
        homeSandbox07Package.Data.Add((sbyte)(31), boolInput31);
        var boolInput7 = new PackageDataBool();
        homeSandbox07Package.Data.Add((sbyte)(7), boolInput7);
        var boolInput25 = new PackageDataBool();
        homeSandbox07Package.Data.Add((sbyte)(25), boolInput25);
        var boolInput27 = new PackageDataBool();
        homeSandbox07Package.Data.Add((sbyte)(27), boolInput27);
        var floatInput29 = new PackageDataFloat
        {
            Data = 50.0f
        };
        homeSandbox07Package.Data.Add((sbyte)(29), floatInput29);
        homeSandbox07Package.XnamMarker = new MemorySlice<byte>(Convert.FromHexString("20"));
        var onBeginEventEvent = new PackageEvent();
        var eventTopic = new TopicReference();
        onBeginEventEvent.Topics.Add(eventTopic);
        homeSandbox07Package.OnBegin = onBeginEventEvent;
        var onEndEventEvent = new PackageEvent();
        var eventTopic2 = new TopicReference();
        onEndEventEvent.Topics.Add(eventTopic2);
        homeSandbox07Package.OnEnd = onEndEventEvent;
        var onChangeEventEvent = new PackageEvent();
        var eventTopic3 = new TopicReference();
        onChangeEventEvent.Topics.Add(eventTopic3);
        homeSandbox07Package.OnChange = onChangeEventEvent;
        return homeSandbox07Package;
    }
}
