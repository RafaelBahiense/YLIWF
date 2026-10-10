using Mutagen.Bethesda.Plugins;
using Mutagen.Bethesda.Skyrim;
using Noggog;

internal static partial class FollowerPlugin
{
    private static readonly FormKey FollowDistanceGlobalForm = OwnForm(0x000993);
    private static readonly FormKey FollowDistanceFactionForm = OwnForm(0x000994);

    // PlayerFollowerPackage (Skyrim.esm:05C84B), using the FollowPlayer template
    // (0D530D). Public inputs 1 and 2 are the minimum and maximum follow radii.
    // Own records keep these presets scoped to the human follower aliases.
    private static Package CreateFollowDistancePackage(uint id, string name, short preset, float minimum, float maximum)
    {
        var package = new Package(OwnForm(id), SkyrimRelease.SkyrimSE)
        {
            EditorID = "YLIWF_Follow" + name,
            Type = Package.Types.Package,
            PreferredSpeed = Package.Speed.Run,
            Flags = Package.Flag.AllowSwimming | Package.Flag.NoCombatAlert,
            Unknown = byte.MaxValue, // Preserve PlayerFollowerPackage's PKDT byte.
            InterruptFlags = (Package.InterruptFlag)64736,
            ScheduleMonth = -1,
            ScheduleDayOfWeek = Package.DayOfWeek.Any,
            ScheduleHour = -1,
            ScheduleMinute = -1,
            PackageTemplate = new FormLink<IPackageGetter>(SkyrimForm(0x0D530D)),
            DataInputVersion = 10,
            XnamMarker = new MemorySlice<byte>([0x27])
        }.WithUnusedScheduleBytes();
        // Two rank bands distinguish individual and party-applied choices.
        package.Conditions.Add(new ConditionFloat
        {
            Flags = Condition.Flag.OR,
            Data = new GetFactionRankConditionData { Faction = { Link = { FormKey = FollowDistanceFactionForm } } },
            ComparisonValue = preset
        });
        package.Conditions.Add(new ConditionFloat
        {
            Data = new GetFactionRankConditionData { Faction = { Link = { FormKey = FollowDistanceFactionForm } } },
            ComparisonValue = preset + 3
        });
        static PackageDataTarget PlayerTarget() => new()
        {
            Type = PackageDataTarget.Types.SingleRef,
            Target = new PackageTargetSpecificReference { Reference = new FormLink<IPlacedGetter>(PlayerReferenceForm) }
        };
        package.Data.Add(0, PlayerTarget());
        package.Data.Add(1, new PackageDataFloat { Data = minimum });
        package.Data.Add(2, new PackageDataFloat { Data = maximum });
        package.Data.Add(6, new PackageDataBool());
        package.Data.Add(9, new PackageDataLocation
        {
            Location = new LocationTargetRadius
            {
                Target = new LocationFallback { Type = LocationTargetRadius.LocationType.NearSelf }
            }
        });
        package.Data.Add(11, new PackageDataBool());
        package.Data.Add(26, PlayerTarget());
        package.Data.Add(28, new PackageDataInt());
        package.OnBegin = new PackageEvent { Topics = { new TopicReference() } };
        package.OnEnd = new PackageEvent { Topics = { new TopicReference() } };
        package.OnChange = new PackageEvent { Topics = { new TopicReference() } };
        return package;
    }

    private static void AddFollowDistancePackages(QuestAlias alias)
    {
        foreach (var id in new uint[] { 0x000990, 0x000991, 0x000992 })
        {
            alias.PackageData.Add(new FormLink<IPackageGetter>(OwnForm(id)));
        }
    }
}
