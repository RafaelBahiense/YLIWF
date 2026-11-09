using Mutagen.Bethesda.Skyrim;

namespace ModTools.Tests;

public sealed class DefenderMovementTests : TestWorkspace
{
    [Fact]
    public void MovementHasIndependentTargetsAndLiveCombatGates()
    {
        using var loaded = SkyrimMod.CreateFromBinaryOverlay(CreatePlugin(), SkyrimRelease.SkyrimSE);
        var markers = loaded.EnumerateMajorRecords().OfType<IPlacedObjectGetter>().ToArray();
        for (var index = 0; index < 32; ++index)
        {
            var package = loaded.Packages.Single(record => record.FormKey.ID == 0xB00 + index);
            Assert.Equal(Package.Interrupt.Combat, package.InterruptOverride);
            Assert.Equal(0x503D0u, package.PackageTemplate.FormKey.ID);
            Assert.Equal("Skyrim.esm", package.PackageTemplate.FormKey.ModKey.FileName.String);
            Assert.Empty(package.ProcedureTree);
            Assert.Single(package.Data);
            Assert.Equal((byte)5, package.DataInputVersion);
            Assert.False(package.Flags.HasFlag(Package.Flag.IgnoreCombat));
            Assert.False(package.Flags.HasFlag(Package.Flag.NoCombatAlert));
            Assert.True(package.Flags.HasFlag(Package.Flag.PreferredSpeed));
            Assert.Equal(Package.InterruptFlag.FriendlyFireComments | Package.InterruptFlag.AggroRadiusBehavior, package.InterruptFlags);
            Assert.False(package.Flags.HasFlag(Package.Flag.MustComplete));
            Assert.Equal(index >= 16 ? Package.Speed.Walk : Package.Speed.Run, package.PreferredSpeed);
            Assert.Contains(package.Conditions, condition => condition.Data is IIsInCombatConditionDataGetter);
            Assert.Contains(package.Conditions.Cast<IConditionFloatGetter>(), condition =>
                condition.Data is IGetGlobalValueConditionDataGetter data && data.Global.Link.FormKey.ID == 0xA10 && condition.ComparisonValue == 0);
            Assert.Contains(package.Conditions.Cast<IConditionFloatGetter>(), condition =>
                condition.Data is IGetFactionRankConditionDataGetter data && data.Faction.Link.FormKey.ID == 0x996 && condition.ComparisonValue == 5);
            var location = (ILocationTargetRadiusGetter)((IPackageDataLocationGetter)package.Data[0]).Location;
            var target = ((ILocationTargetGetter)location.Target).Link.FormKey;
            Assert.Equal((uint)(0xB20 + index % 8), target.ID);
            Assert.NotEqual(0, markers.Single(marker => marker.FormKey == target).MajorRecordFlagsRaw & 0x400);
        }
    }
}
