using System.Buffers.Binary;
using Mutagen.Bethesda.Skyrim;
using Noggog;
using Yliwf.Addons.CombatTests;

namespace ModTools.Tests;

public sealed class CombatArenaTests : TestWorkspace
{
    [Fact]
    public void StandardPluginBuildGeneratesTheManifestDeclaredArena()
    {
        ModTools.Building.PluginBuilder.Generate(root, temporary);
        var addon = ModTools.Building.Addon.Read(root).Single(addon => addon.Name == "CombatTests");
        Assert.Equal(CombatArena.Plugin, addon.PluginFile);
        var path = ModTools.Building.AddonPlugins.OutputPath(temporary, addon);
        Assert.True(File.Exists(path));
        ModTools.Packaging.BuildReceipt.Validate(root, ModTools.Packaging.BuildComponent.Plugin,
            [path], ModTools.Packaging.BuildReceipt.PluginPath(path));
        using var mod = SkyrimMod.CreateFromBinaryOverlay(path, SkyrimRelease.SkyrimSE);
        Assert.Equal(CombatArena.CellEditorId, Cell(mod).EditorID);
    }

    private static ICellGetter Cell(ISkyrimModGetter mod) => mod.Cells.Single().SubBlocks.Single().Cells.Single();

    [Fact]
    public void PowerBashIdleIsStandaloneAndRequiresPerkAndOwnedBlockState()
    {
        var path = Path.Combine(temporary, CombatArena.Plugin);
        CombatArena.Create().BeginWrite.ToPath(path).WithLoadOrderFromHeaderMasters()
            .WithNoDataFolder().NoMastersListContentCheck().Write();
        using var mod = SkyrimMod.CreateFromBinaryOverlay(path, SkyrimRelease.SkyrimSE);
        var idle = Assert.Single(mod.IdleAnimations);
        Assert.Equal(CombatArena.PowerBashIdle, idle.FormKey.ID);
        Assert.Equal("bashPowerStart", idle.AnimationEvent);
        Assert.All(idle.RelatedIdles, link => Assert.True(link.IsNull));
        Assert.Equal(2, idle.RelatedIdles.Count);
        Assert.Equal(4, idle.Conditions.Count);
        var perk = Assert.IsType<HasPerkConditionData>(idle.Conditions[0].Data);
        Assert.Equal(CombatArena.PowerBashPerk, perk.Perk.Link.FormKey.ID);
        Assert.IsType<IsBlockingConditionData>(idle.Conditions[1].Data);
        Assert.IsType<GetWantBlockingConditionData>(idle.Conditions[2].Data);
        Assert.IsType<GetAttackStateConditionData>(idle.Conditions[3].Data);
        Assert.All(idle.Conditions, condition => Assert.Equal(CompareOperator.EqualTo, condition.CompareOperator));
        Assert.DoesNotContain(idle.Conditions, condition => condition.Data is GetIsIDConditionData);
    }

    [Fact]
    public void ScenePackagesHoldSetupAndAllowCombatWithoutFollowingThePlayer()
    {
        var path = Path.Combine(temporary, CombatArena.Plugin);
        CombatArena.Create().BeginWrite.ToPath(path).WithLoadOrderFromHeaderMasters()
            .WithNoDataFolder().NoMastersListContentCheck().Write();
        using var mod = SkyrimMod.CreateFromBinaryOverlay(path, SkyrimRelease.SkyrimSE);
        var packages = mod.Packages.Where(p => p.FormKey.ID is >= 0x840 and <= 0x843).OrderBy(p => p.FormKey.ID).ToArray();
        Assert.Equal(4, packages.Length);
        for (var index = 0; index < packages.Length; ++index)
        {
            var package = packages[index];
            Assert.Equal((index & 1) == 0, package.Flags.HasFlag(Package.Flag.IgnoreCombat));
            Assert.Equal((index & 1) == 0, package.Flags.HasFlag(Package.Flag.PreferredSpeed));
            var location = Assert.IsAssignableFrom<IPackageDataLocationGetter>(package.Data[0]);
            var radius = Assert.IsAssignableFrom<ILocationTargetRadiusGetter>(location.Location);
            var target = Assert.IsAssignableFrom<ILocationTargetGetter>(radius.Target);
            Assert.Equal(index < 2 ? 0x811u : 0x813u, target.Link.FormKey.ID);
            Assert.Equal((index & 1) == 0 ? 32u : 4096u, radius.Radius);
            if ((index & 1) == 0)
            {
                Assert.True(package.PackageTemplate.IsNull);
                Assert.Equal("Travel", package.ProcedureTree[1].ProcedureType);
                Assert.Equal("Wait", package.ProcedureTree[2].ProcedureType);
                Assert.Equal(PackageRoot.Flag.RepeatWhenComplete, package.ProcedureTree[0].Root!.Flags);
            }
            else
            {
                Assert.Equal(0x503D0u, package.PackageTemplate.FormKey.ID);
                Assert.Equal("Skyrim.esm", package.PackageTemplate.FormKey.ModKey.FileName.String);
                Assert.Empty(package.ProcedureTree);
                Assert.Equal(Package.Interrupt.Combat, package.InterruptOverride);
                Assert.Equal(Package.Speed.Run, package.PreferredSpeed);
                Assert.Equal(Package.Flag.AllowSwimming, package.Flags);
                Assert.Equal(Package.InterruptFlag.FriendlyFireComments | Package.InterruptFlag.AggroRadiusBehavior, package.InterruptFlags);
                Assert.Equal(5, package.DataInputVersion);
                Assert.Equal((byte)130, package.Unknown);
            }
            if ((index & 1) != 0)
            {
                var condition = Assert.Single(package.Conditions);
                var gate = Assert.IsAssignableFrom<IGetFactionRankConditionDataGetter>(condition.Data);
                Assert.Equal(0x844u, gate.Faction.Link.FormKey.ID);
                Assert.Equal(Condition.RunOnType.Subject, gate.RunOnType);
                Assert.Equal(index < 2 ? 1 : 2, ((IConditionFloatGetter)condition).ComparisonValue);
            }
        }
    }

    [Fact]
    public void ArenaRoundTripsWithoutWorldOverridesOrStartupBehavior()
    {
        var path = Path.Combine(temporary, CombatArena.Plugin);
        CombatArena.Create().BeginWrite.ToPath(path).WithLoadOrderFromHeaderMasters()
            .WithNoDataFolder().NoMastersListContentCheck().Write();
        using var mod = SkyrimMod.CreateFromBinaryOverlay(path, SkyrimRelease.SkyrimSE);
        var cell = Cell(mod);
        Assert.Equal(CombatArena.CellEditorId, cell.EditorID);
        Assert.True(cell.Flags.HasFlag(Mutagen.Bethesda.Skyrim.Cell.Flag.IsInteriorCell));
        var item = Assert.Single(mod.ModHeader.MasterReferences);
        Assert.Equal("Skyrim.esm", item.Master.FileName.String);
        Assert.All(mod.EnumerateMajorRecords(), r => Assert.Equal(mod.ModKey, r.FormKey.ModKey));
        Assert.Empty(mod.Quests);
        Assert.Equal(CombatArena.FixtureBaseCount + 1, mod.Npcs.Count);
        Assert.Empty(mod.Worldspaces);
        Assert.All(cell.Persistent.Concat(cell.Temporary), placed => Assert.IsType<IPlacedObjectGetter>(placed, exactMatch: false));
        Assert.Equal(68, cell.Temporary.Count);
        Assert.Equal(9, cell.Persistent.Count);
        Assert.All(cell.Persistent, p => Assert.NotEqual(0, (p.MajorRecordFlagsRaw & 0x400)));

        var nav = cell.NavigationMeshes.Single();
        Assert.Equal(cell.FormKey, Assert.IsType<ICellNavmeshParentGetter>(nav.Data!.Parent, exactMatch: false).Parent.FormKey);
        var info = mod.NavigationMeshInfoMaps.Single().MapInfos.Single();
        Assert.Equal(nav.FormKey, info.NavigationMesh.FormKey);
        Assert.Equal(cell.FormKey, Assert.IsType<INavigationMapInfoCellParentGetter>(info.Parent, exactMatch: false).ParentCell.FormKey);
        Assert.Empty(nav.Data.EdgeLinks);
        Assert.Empty(nav.Data.DoorTriangles);
        Assert.All(cell.Temporary.OfType<IPlacedObjectGetter>().Where(p => p.FormKey.ID < 0x940),
            p => Assert.Equal(2f, p.Scale));
    }

    [Fact]
    public void AllTrianglesHavePositiveAreaReciprocalEdgesAndOneConnectedComponent()
    {
        var data = Cell(CombatArena.Create()).NavigationMeshes.Single().Data!;
        for (var i = 0; i < data.Triangles.Count; i++)
        {
            var triangle = data.Triangles[i];
            var v = triangle.Vertices;
            var a = data.Vertices[v.X];
            var b = data.Vertices[v.Y];
            var c = data.Vertices[v.Z];
            Assert.True(Cross(a, b, c) > 0, $"Triangle {i} has reversed winding or zero area");
            var indices = new[] { v.X, v.Y, v.Z };
            var links = Edges(triangle);
            for (var edge = 0; edge < 3; edge++)
            {
                if (links[edge] < 0)
                {
                    continue;
                }
                Assert.InRange(links[edge], 0, data.Triangles.Count - 1);
                var other = data.Triangles[links[edge]];
                var ov = new[] { other.Vertices.X, other.Vertices.Y, other.Vertices.Z };
                var reverse = Enumerable.Range(0, 3).Single(e => ov[e] == indices[(edge + 1) % 3] && ov[(e + 1) % 3] == indices[edge]);
                Assert.Equal(i, Edges(other)[reverse]);
            }
        }
        var visited = new HashSet<int>();
        var pending = new Queue<int>();
        pending.Enqueue(0);
        while (pending.TryDequeue(out var next))
        {
            if (!visited.Add(next))
            {
                continue;
            }
            foreach (var adjacent in Edges(data.Triangles[next]).Where(e => e >= 0))
            {
                pending.Enqueue(adjacent);
            }
        }
        Assert.Equal(data.Triangles.Count, visited.Count);
    }

    [Fact]
    public void NavigationKeepsAColumnClearanceMarginFromThePhysicalPerimeter()
    {
        var data = Cell(CombatArena.Create()).NavigationMeshes.Single().Data!;
        Assert.True(1952 - CombatArena.Boundary >= 384);
        Assert.All(data.Vertices, vertex =>
        {
            Assert.InRange(vertex.X, -CombatArena.Boundary, CombatArena.Boundary);
            Assert.InRange(vertex.Y, -CombatArena.Boundary, CombatArena.Boundary);
        });
    }

    [Fact]
    public void PartitionHasNoWalkableTrianglesAndEveryStartIsOnConnectedNavigation()
    {
        var cell = Cell(CombatArena.Create());
        var data = cell.NavigationMeshes.Single().Data!;
        foreach (var triangle in data.Triangles)
        {
            var a = data.Vertices[triangle.Vertices.X];
            var b = data.Vertices[triangle.Vertices.Y];
            var c = data.Vertices[triangle.Vertices.Z];
            var minX = Math.Min(a.X, Math.Min(b.X, c.X));
            var maxX = Math.Max(a.X, Math.Max(b.X, c.X));
            var minY = Math.Min(a.Y, Math.Min(b.Y, c.Y));
            var maxY = Math.Max(a.Y, Math.Max(b.Y, c.Y));
            Assert.False(minX < CombatArena.ObstacleRight && maxX > CombatArena.ObstacleLeft &&
                         minY < CombatArena.ObstacleEnd && maxY > -CombatArena.ObstacleEnd);
        }
        foreach (var marker in cell.Persistent.OfType<IPlacedObjectGetter>())
        {
            var position = marker.Placement!.Position;
            Assert.Contains(data.Triangles, triangle =>
                Contains(data.Vertices[triangle.Vertices.X], data.Vertices[triangle.Vertices.Y], data.Vertices[triangle.Vertices.Z], position));
        }
        var references = cell.Temporary.OfType<IPlacedObjectGetter>().ToArray();
        Assert.Equal(references.Length, references.Select(r => r.Placement!.Position).Distinct().Count());
    }

    [Fact]
    public void DividerUsesClosedFreestandingSectionsOverAnUnbrokenFloor()
    {
        var cell = Cell(CombatArena.Create());
        var references = cell.Temporary.OfType<IPlacedObjectGetter>().ToArray();
        foreach (var id in new uint[] { 0x91D, 0x925 })
        {
            var floor = Assert.Single(references, placed => placed.FormKey.ID == id);
            Assert.Equal(0x13F81u, floor.Base.FormKey.ID);
        }
        var sections = references.Where(placed => placed.Base.FormKey.ID == CombatArena.PartitionBase)
            .OrderBy(placed => placed.Placement!.Position.Y).ToArray();
        Assert.Equal(4, sections.Length);
        var lastEnd = float.NegativeInfinity;
        foreach (var section in sections)
        {
            Assert.Equal(CombatArena.PartitionScale, section.Scale);
            var position = section.Placement!.Position;
            var scale = section.Scale!.Value;
            Assert.InRange(position.X - 26 * scale, CombatArena.ObstacleLeft, CombatArena.ObstacleRight);
            Assert.InRange(position.X + 26 * scale, CombatArena.ObstacleLeft, CombatArena.ObstacleRight);
            Assert.InRange(position.Y - 109 * scale, -CombatArena.ObstacleEnd, CombatArena.ObstacleEnd);
            Assert.InRange(position.Y + 107 * scale, -CombatArena.ObstacleEnd, CombatArena.ObstacleEnd);
            Assert.True(position.Z - 34 * scale < 0);
            Assert.True(position.Z + 510 * scale >= 640);
            if (!float.IsNegativeInfinity(lastEnd))
            {
                Assert.True(position.Y - 109 * scale < lastEnd);
            }
            lastEnd = position.Y + 107 * scale;
        }
    }

    [Fact]
    public void SpatialGridAndNavigationInfoSurviveSerialization()
    {
        var path = Path.Combine(temporary, CombatArena.Plugin);
        CombatArena.Create().BeginWrite.ToPath(path).WithLoadOrderFromHeaderMasters()
            .WithNoDataFolder().NoMastersListContentCheck().Write();
        using var mod = SkyrimMod.CreateFromBinaryOverlay(path, SkyrimRelease.SkyrimSE);
        var data = Cell(mod).NavigationMeshes.Single().Data!;
        Assert.Equal(1u, data.NavmeshGridDivisor);
        var grid = data.NavmeshGrid.Span;
        Assert.Equal((uint)data.Triangles.Count, BinaryPrimitives.ReadUInt32LittleEndian(grid));
        Assert.Equal(4 + data.Triangles.Count * 2, grid.Length);
        for (var i = 0; i < data.Triangles.Count; i++)
        {
            Assert.Equal(i, BinaryPrimitives.ReadInt16LittleEndian(grid.Slice(4 + i * 2, 2)));
        }
        Assert.Equal(12u, data.NavmeshVersion);
        Assert.Equal(data.CrcHash, unchecked((uint)mod.NavigationMeshInfoMaps.Single().MapInfos.Single().Unknown2));
        Assert.NotNull(mod.NavigationMeshInfoMaps.Single().PreferredPathing);
    }

    private static short[] Edges(INavmeshTriangleGetter triangle) =>
        [triangle.EdgeLink_0_1, triangle.EdgeLink_1_2, triangle.EdgeLink_2_0];

    private static float Cross(P3Float a, P3Float b, P3Float c) =>
        (b.X - a.X) * (c.Y - a.Y) - (b.Y - a.Y) * (c.X - a.X);

    private static bool Contains(P3Float a, P3Float b, P3Float c, P3Float point) =>
        Cross(a, b, point) >= 0 && Cross(b, c, point) >= 0 && Cross(c, a, point) >= 0;
}
