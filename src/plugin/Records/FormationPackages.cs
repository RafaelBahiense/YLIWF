using Mutagen.Bethesda.Plugins;
using Mutagen.Bethesda.Skyrim;
using Noggog;

internal static partial class FollowerPlugin
{
    private const uint FormationPackageStart = 0x0009A0;
    private const uint FormationAlternatePackageStart = 0x0009C0;
    private const uint FormationTargetStart = 0x0009B0;
    private const int FormationSlots = 8;

    private static FormKey FormationPackageForm(int slot, bool alternate) =>
        OwnForm((alternate ? FormationAlternatePackageStart : FormationPackageStart) + (uint)slot);

    private static void AddFormationPackages(SkyrimMod mod)
    {
        for (var slot = 0; slot < FormationSlots; ++slot)
        {
            mod.Packages.Add(CreateFormationTravelPackage(slot, alternate: false));
            mod.Packages.Add(CreateFormationTravelPackage(slot, alternate: true));
        }
    }

    private static Package CreateFormationTravelPackage(int slot, bool alternate)
    {
        // Travel to the marker, then hold there. A bare Travel procedure's
        // SuccessCompletesPackage flag ends temporary adapter movement on arrival.
        var package = new Package(FormationPackageForm(slot, alternate), SkyrimRelease.SkyrimSE)
        {
            EditorID = "YLIWF_FormationTravel" + slot + (alternate ? "Alternate" : ""),
            Type = Package.Types.Package,
            PreferredSpeed = Package.Speed.Run,
            // PKDT requires this flag to honor the PreferredSpeed value.
            Flags = Package.Flag.PreferredSpeed | Package.Flag.AllowSwimming | Package.Flag.NoCombatAlert,
            InterruptFlags = Package.InterruptFlag.HellosToPlayer | Package.InterruptFlag.FriendlyFireComments,
            ScheduleMonth = -1,
            ScheduleDayOfWeek = Package.DayOfWeek.Any,
            ScheduleHour = -1,
            ScheduleMinute = -1,
            DataInputVersion = 3,
            ProcedureTree = CreateFormationProcedureTree(),
            XnamMarker = new MemorySlice<byte>([7])
        }.WithUnusedScheduleBytes();
        package.Conditions.Add(new ConditionFloat
        {
            Data = new GetFactionRankConditionData { Faction = { Link = { FormKey = OwnForm(0x000995) } } },
            ComparisonValue = FormationSlots * (alternate ? 3 : 2) + slot
        });
        package.Conditions.Add(new ConditionFloat
        {
            Data = new GetPlayerTeammateConditionData(),
            ComparisonValue = 1
        });
        package.Conditions.Add(new ConditionFloat
        {
            Data = new GetActorValueConditionData { ActorValue = ActorValue.WaitingForPlayer },
            ComparisonValue = 0
        });
        package.Data.Add(0, new PackageDataLocation
        {
            Location = new LocationTargetRadius
            {
                Target = new LocationTarget { Link = new FormLink<IPlacedGetter>(OwnForm(FormationTargetStart + (uint)slot)) },
                Radius = 64
            }
        });
        package.Data.Add(2, new PackageDataBool());
        package.Data.Add(4, new PackageDataBool());
        package.Data.Add(5, new PackageDataFloat { Data = 1 });
        package.Data.Add(6, new PackageDataBool { Data = true });
        package.OnBegin = new PackageEvent { Topics = { new TopicReference() } };
        package.OnEnd = new PackageEvent { Topics = { new TopicReference() } };
        package.OnChange = new PackageEvent { Topics = { new TopicReference() } };
        return package;
    }

    private static ExtendedList<PackageBranch> CreateFormationProcedureTree() =>
    [
        new PackageBranch
        {
            BranchType = "Sequence",
            Root = new PackageRoot { BranchCount = 2, Flags = PackageRoot.Flag.RepeatWhenComplete }
        },
        new PackageBranch
        {
            BranchType = "Procedure",
            ProcedureType = "Travel",
            Flags = 0,
            DataInputIndices = [0, 2, 4]
        },
        new PackageBranch
        {
            BranchType = "Procedure",
            ProcedureType = "Wait",
            Flags = 0,
            DataInputIndices = [5, 6]
        }
    ];

    private static void AddFormationTargets(Cell cell)
    {
        for (var slot = 0; slot < FormationSlots; ++slot)
        {
            cell.Persistent.Add(new PlacedObject(OwnForm(FormationTargetStart + (uint)slot), SkyrimRelease.SkyrimSE)
            {
                EditorID = "YLIWF_FormationTarget" + slot,
                MajorRecordFlagsRaw = 0x400, // REFR persistent flag, as for the home markers.
                Base = new FormLinkNullable<IPlaceableObjectGetter>(SkyrimForm(0x00003B)),
                Placement = new Placement()
            });
        }
    }
}
