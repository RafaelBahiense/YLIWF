using Mutagen.Bethesda.Plugins;
using Mutagen.Bethesda.Skyrim;

internal static partial class FollowerPlugin
{
    private const int DefenderMovementSlots = 8;
    private static readonly FormKey DefenderMovementFaction = OwnForm(0xB28);

    private static void AddDefenderMovementPackages(SkyrimMod mod)
    {
        mod.Factions.Add(new Faction(DefenderMovementFaction, SkyrimRelease.SkyrimSE)
        {
            EditorID = "YLIWF_DefenderMovement",
            Flags = Faction.FactionFlag.HiddenFromPC
        });
        for (var index = 0; index < DefenderMovementSlots * 4; ++index)
        {
            var package = CreateFormationTravelPackage(index % DefenderMovementSlots, alternate: false, id: 0xB00 + (uint)index);
            package.EditorID = "YLIWF_DefenderMovement" + index;
            package.Unknown = 3;
            package.Flags |= Package.Flag.IgnoreCombat;
            package.PreferredSpeed = index >= DefenderMovementSlots * 2 ? Package.Speed.Walk : Package.Speed.Run;
            package.Conditions.Clear();
            package.Conditions.Add(new ConditionFloat
            {
                Data = new GetFactionRankConditionData { Faction = { Link = { FormKey = DefenderMovementFaction } } },
                ComparisonValue = index
            });
            package.Conditions.Add(new ConditionFloat
            {
                Data = new GetFactionRankConditionData { Faction = { Link = { FormKey = OwnForm(0x996) } } },
                ComparisonValue = 5
            });
            package.Conditions.Add(new ConditionFloat
            {
                Data = new GetFactionRankConditionData { Faction = { Link = { FormKey = CurrentFollowerFactionForm } } },
                CompareOperator = CompareOperator.GreaterThanOrEqualTo,
                ComparisonValue = 0
            });
            package.Conditions.Add(new ConditionFloat { Data = new GetPlayerTeammateConditionData(), ComparisonValue = 1 });
            package.Conditions.Add(new ConditionFloat { Data = new IsInCombatConditionData(), ComparisonValue = 1 });
            package.Conditions.Add(new ConditionFloat { Data = new GetActorValueConditionData { ActorValue = ActorValue.WaitingForPlayer }, ComparisonValue = 0 });
            package.Conditions.Add(new ConditionFloat
            {
                Data = new GetGlobalValueConditionData { Global = { Link = { FormKey = StandDownGlobalForm } } },
                ComparisonValue = 0
            });
            ((PackageDataLocation)package.Data[0]).Location = new LocationTargetRadius
            {
                Target = new LocationTarget { Link = new FormLink<IPlacedGetter>(OwnForm(0xB20 + (uint)(index % DefenderMovementSlots))) },
                Radius = 64
            };
            mod.Packages.Add(package);
        }
    }

    private static void AddDefenderMovementTargets(Cell cell)
    {
        for (var slot = 0; slot < DefenderMovementSlots; ++slot)
        {
            cell.Persistent.Add(new PlacedObject(OwnForm(0xB20 + (uint)slot), SkyrimRelease.SkyrimSE)
            {
                EditorID = "YLIWF_DefenderMovementTarget" + slot,
                MajorRecordFlagsRaw = 0x400,
                Base = new FormLinkNullable<IPlaceableObjectGetter>(SkyrimForm(0x3B)),
                Placement = new Placement()
            });
        }
    }
}
