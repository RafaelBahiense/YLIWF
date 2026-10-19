using Mutagen.Bethesda.Plugins;
using Mutagen.Bethesda.Skyrim;
using Noggog;

internal static partial class FollowerPlugin
{
    private static readonly FormKey StandDownGlobalForm = OwnForm(0x000A10);
    private static readonly FormKey StandDownPackageForm = OwnForm(0x000A11);
    private static readonly FormKey StandDownMovementForm = OwnForm(0x000A15);
    private const int StandDownSlots = 16;

    private static IEnumerable<FormKey> StandDownPackages =>
        Enumerable.Range(0, StandDownSlots * 2).Select(slot => OwnForm(0xA20 + (uint)slot))
            .Concat(Enumerable.Range(0, 4).Select(index => OwnForm(0xA11 + (uint)index)));

    private static void AddCombatOrders(SkyrimMod mod)
    {
        mod.Globals.Add(new GlobalShort(StandDownGlobalForm, SkyrimRelease.SkyrimSE)
        {
            EditorID = "YLIWF_StandDown",
            Data = 0
        });
        mod.Factions.Add(new Faction(StandDownMovementForm, SkyrimRelease.SkyrimSE)
        {
            EditorID = "YLIWF_StandDownMovement",
            Flags = Faction.FactionFlag.HiddenFromPC
        });
        mod.Factions.Add(new Faction(OwnForm(0x000A16), SkyrimRelease.SkyrimSE)
        {
            EditorID = "YLIWF_StandDownCombatPrevention",
            Flags = Faction.FactionFlag.HiddenFromPC
        });
        // Waiting remains an explicit follower order, independent of combat policy.
        mod.Packages.Add(new Package(StandDownPackageForm, SkyrimRelease.SkyrimSE)
        {
            EditorID = "YLIWF_StandDownPackage",
            Type = Package.Types.Package,
            Unknown = 3, // PKDT interruptOverrideType: Combat; same as healer overrides.
            Flags = Package.Flag.IgnoreCombat | Package.Flag.NoCombatAlert,
            ScheduleMonth = -1,
            ScheduleDayOfWeek = Package.DayOfWeek.Any,
            ScheduleHour = -1,
            ScheduleMinute = -1,
            DataInputVersion = 3,
            XnamMarker = new MemorySlice<byte>([2]),
            ProcedureTree =
            [
                new PackageBranch
                {
                    BranchType = "Sequence",
                    Root = new PackageRoot { BranchCount = 1, Flags = PackageRoot.Flag.RepeatWhenComplete }
                },
                new PackageBranch { BranchType = "Procedure", ProcedureType = "Wait", DataInputIndices = [0, 1] }
            ],
            Data = { [0] = new PackageDataFloat { Data = 1 }, [1] = new PackageDataBool { Data = true } },
            Conditions =
            [
                new ConditionFloat
                {
                    Data = new GetGlobalValueConditionData { Global = { Link = { FormKey = StandDownGlobalForm } } },
                    ComparisonValue = 1
                },
                new ConditionFloat
                {
                    Data = new GetActorValueConditionData { ActorValue = ActorValue.WaitingForPlayer },
                    CompareOperator = CompareOperator.GreaterThan,
                    ComparisonValue = 0
                }
            ]
        }.WithUnusedScheduleBytes());

        string[] names = ["Close", "Normal", "Far"];
        float[] minimum = [128, 256, 512], maximum = [192, 384, 768];
        for (short preset = 0; preset < names.Length; ++preset)
        {
            var follow = CreateFollowDistancePackage(0xA12 + (uint)preset, "Passive" + names[preset], preset,
                minimum[preset], maximum[preset]);
            follow.Unknown = 3;
            follow.Flags |= Package.Flag.IgnoreCombat;
            follow.Conditions.Add(StandDownEnabled());
            follow.Conditions.Add(new ConditionFloat
            {
                Data = new GetActorValueConditionData { ActorValue = ActorValue.WaitingForPlayer },
                ComparisonValue = 0
            });
            follow.Conditions.Add(new ConditionFloat
            {
                Data = new GetFactionRankConditionData { Faction = { Link = { FormKey = StandDownMovementForm } } },
                CompareOperator = CompareOperator.LessThan,
                ComparisonValue = 0
            });
            mod.Packages.Add(follow);
        }
        for (var slot = 0; slot < StandDownSlots * 2; ++slot)
        {
            var avoidance = CreateFormationTravelPackage(slot % FormationSlots, alternate: false, id: 0xA20 + (uint)slot);
            avoidance.EditorID = "YLIWF_StandDownAvoid" + slot;
            avoidance.Unknown = 3;
            avoidance.Flags |= Package.Flag.IgnoreCombat;
            avoidance.Conditions.Clear();
            avoidance.Conditions.Add(StandDownEnabled());
            avoidance.Conditions.Add(new ConditionFloat
            {
                Data = new GetFactionRankConditionData { Faction = { Link = { FormKey = StandDownMovementForm } } },
                ComparisonValue = slot
            });
            ((PackageDataLocation)avoidance.Data[0]).Location = new LocationTargetRadius
            {
                Target = new LocationTarget { Link = new FormLink<IPlacedGetter>(OwnForm(0xA40 + (uint)(slot % StandDownSlots))) },
                Radius = 64
            };
            mod.Packages.Add(avoidance);
        }
    }

    private static ConditionFloat StandDownEnabled() => new()
    {
        Data = new GetGlobalValueConditionData { Global = { Link = { FormKey = StandDownGlobalForm } } },
        ComparisonValue = 1
    };

    private static void AddStandDownTargets(Cell cell)
    {
        for (var slot = 0; slot < StandDownSlots; ++slot)
        {
            cell.Persistent.Add(new PlacedObject(OwnForm(0xA40 + (uint)slot), SkyrimRelease.SkyrimSE)
            {
                EditorID = "YLIWF_StandDownTarget" + slot,
                MajorRecordFlagsRaw = 0x400,
                Base = new FormLinkNullable<IPlaceableObjectGetter>(SkyrimForm(0x00003B)),
                Placement = new Placement()
            });
        }
    }
}
