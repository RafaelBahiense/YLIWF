using Mutagen.Bethesda.Plugins;
using Mutagen.Bethesda.Skyrim;
using Noggog;

internal static partial class FollowerPlugin
{
    private const uint HealerTravelStart = 0x0009D0;
    private const uint HealerAlternateTravelStart = 0x0009E0;
    private const uint HealerCastStart = 0x0009F0;
    private const uint HealerTargetStart = 0x000A00;
    private const int HealerSlots = 8;
    private static readonly FormKey HealerActivityForm = OwnForm(0x000A08);
    private static readonly FormKey HealerCombatOverridesForm = OwnForm(0x000A09);

    private static Package HealerPackage(uint id, string name, bool travel = false) => new Package(OwnForm(id), SkyrimRelease.SkyrimSE)
    {
        EditorID = name,
        Type = Package.Types.Package,
        Unknown = 3, // PKDT interruptOverrideType: Combat, as in vanilla combat overrides.
        PreferredSpeed = Package.Speed.Run,
        Flags = Package.Flag.IgnoreCombat | Package.Flag.PreferredSpeed | Package.Flag.AllowSwimming,
        ScheduleMonth = -1,
        ScheduleDayOfWeek = Package.DayOfWeek.Any,
        ScheduleHour = -1,
        ScheduleMinute = -1,
        ProcedureTree = travel ? CreateFormationProcedureTree() : [],
        Conditions =
        [
            new ConditionFloat
            {
                Data = new GetFactionRankConditionData { Faction = { Link = { FormKey = OwnForm(0x000996) } } },
                ComparisonValue = 1 // Healer role's saved ABI value.
            },
            new ConditionFloat
            {
                Data = new GetFactionRankConditionData { Faction = { Link = { FormKey = CurrentFollowerFactionForm } } },
                CompareOperator = CompareOperator.GreaterThanOrEqualTo,
                ComparisonValue = 0
            },
            new ConditionFloat { Data = new GetPlayerTeammateConditionData(), ComparisonValue = 1 },
            new ConditionFloat { Data = new GetActorValueConditionData { ActorValue = ActorValue.WaitingForPlayer }, ComparisonValue = 0 }
        ],
        OnBegin = new PackageEvent { Topics = { new TopicReference() } },
        OnEnd = new PackageEvent { Topics = { new TopicReference() } },
        OnChange = new PackageEvent { Topics = { new TopicReference() } }
    }.WithUnusedScheduleBytes();

    private static void AddHealerPackages(SkyrimMod mod)
    {
        mod.Factions.Add(new Faction(HealerActivityForm, SkyrimRelease.SkyrimSE)
        {
            EditorID = "YLIWF_HealerActivity",
            Flags = Faction.FactionFlag.HiddenFromPC
        });
        var overrides = new FormList(HealerCombatOverridesForm, SkyrimRelease.SkyrimSE)
        {
            EditorID = "YLIWF_HealerCombatOverrides"
        };
        for (var slot = 0; slot < HealerSlots; ++slot)
        {
            foreach (var alternate in new[] { false, true })
            {
                var package = HealerPackage((alternate ? HealerAlternateTravelStart : HealerTravelStart) + (uint)slot,
                    "YLIWF_HealerTravel" + slot + (alternate ? "Alternate" : ""), travel: true);
                package.DataInputVersion = 3;
                package.XnamMarker = new MemorySlice<byte>([7]);
                package.Data.Add(0, new PackageDataLocation
                {
                    Location = new LocationTargetRadius
                    {
                        Target = new LocationTarget { Link = new FormLink<IPlacedGetter>(OwnForm(HealerTargetStart + (uint)slot)) },
                        Radius = 64
                    }
                });
                package.Data.Add(2, new PackageDataBool());
                package.Data.Add(4, new PackageDataBool());
                package.Data.Add(5, new PackageDataFloat { Data = 1 });
                package.Data.Add(6, new PackageDataBool { Data = true });
                AddHealerActivityCondition(package, slot + (alternate ? HealerSlots : 0));
                mod.Packages.Add(package);
                overrides.Items.Add(new FormLink<ISkyrimMajorRecordGetter>(package.FormKey));
            }

            // Skyrim.esm:UseMagic supplies animated, single-shot casting. Native
            // code fills this slot's spell and reference inputs while inactive.
            var cast = HealerPackage(HealerCastStart + (uint)slot, "YLIWF_HealerCast" + slot);
            cast.PackageTemplate = new FormLink<IPackageGetter>(SkyrimForm(0x0504F5));
            cast.DataInputVersion = 1;
            cast.XnamMarker = new MemorySlice<byte>([13]);
            cast.Data.Add(2, new PackageDataLocation
            {
                Location = new LocationTargetRadius { Target = new LocationFallback { Type = LocationTargetRadius.LocationType.NearSelf } }
            });
            cast.Data.Add(3, new PackageDataTarget
            {
                Type = PackageDataTarget.Types.Target,
                Target = new PackageTargetObjectID { Reference = new FormLink<IObjectIdGetter>(SkyrimForm(0x012FD2)) }
            });
            cast.Data.Add(4, new PackageDataTarget
            {
                Type = PackageDataTarget.Types.SingleRef,
                Target = new PackageTargetSpecificReference { Reference = new FormLink<IPlacedGetter>(PlayerReferenceForm) }
            });
            cast.Data.Add(5, new PackageDataBool());
            cast.Data.Add(6, new PackageDataFloat { Data = 1 });
            cast.Data.Add(7, new PackageDataFloat { Data = 1.2f });
            cast.Data.Add(8, new PackageDataFloat { Data = 1 });
            cast.Data.Add(9, new PackageDataFloat { Data = 1 });
            cast.Data.Add(10, new PackageDataInt { Data = 1 });
            cast.Data.Add(11, new PackageDataInt { Data = 1 });
            cast.Data.Add(12, new PackageDataBool());
            AddHealerActivityCondition(cast, slot + HealerSlots * 2);
            mod.Packages.Add(cast);
            overrides.Items.Add(new FormLink<ISkyrimMajorRecordGetter>(cast.FormKey));
        }
        foreach (var package in StandDownPackages)
        {
            overrides.Items.Add(new FormLink<ISkyrimMajorRecordGetter>(package));
        }
        // Preserve PlayerFollowerCombatOverridePackageList's ordinary fallback.
        overrides.Items.Add(new FormLink<ISkyrimMajorRecordGetter>(SkyrimForm(0x109124)));
        overrides.Items.Add(new FormLink<ISkyrimMajorRecordGetter>(SkyrimForm(0x05C851)));
        mod.FormLists.Add(overrides);
    }

    private static void AddHealerActivityCondition(Package package, int rank) => package.Conditions.Add(new ConditionFloat
    {
        Data = new GetFactionRankConditionData { Faction = { Link = { FormKey = HealerActivityForm } } },
        ComparisonValue = rank
    });

    private static void AddHealerAliasPackages(Quest quest)
    {
        foreach (var alias in quest.Aliases.Where(alias => alias.ID != 1))
        {
            alias.CombatOverridePackageList = new FormLinkNullable<IFormListGetter>(HealerCombatOverridesForm);
            var packages = new List<FormKey>();
            for (var slot = 0; slot < HealerSlots; ++slot)
            {
                foreach (var start in new[] { HealerTravelStart, HealerAlternateTravelStart, HealerCastStart })
                {
                    packages.Add(OwnForm(start + (uint)slot));
                }
            }
            // The same gated actions must be selectable during recovery outside combat.
            foreach (var package in StandDownPackages.Reverse())
            {
                alias.PackageData.Insert(0, new FormLink<IPackageGetter>(package));
            }
            for (var index = packages.Count - 1; index >= 0; --index)
            {
                alias.PackageData.Insert(0, new FormLink<IPackageGetter>(packages[index]));
            }
        }
    }

    private static void AddHealerTargets(Cell cell)
    {
        for (var slot = 0; slot < HealerSlots; ++slot)
        {
            cell.Persistent.Add(new PlacedObject(OwnForm(HealerTargetStart + (uint)slot), SkyrimRelease.SkyrimSE)
            {
                EditorID = "YLIWF_HealerTarget" + slot,
                MajorRecordFlagsRaw = 0x400,
                Base = new FormLinkNullable<IPlaceableObjectGetter>(SkyrimForm(0x00003B)),
                Placement = new Placement()
            });
        }
    }
}
