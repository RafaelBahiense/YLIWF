using System.Buffers.Binary;
using System.Drawing;
using ModTools.Building;
using Mutagen.Bethesda.Plugins;
using Mutagen.Bethesda.Plugins.Records;
using Mutagen.Bethesda.Skyrim;
using Noggog;

namespace Yliwf.Addons.CombatTests;

public sealed class CombatArena : IAddonPluginGenerator
{
    public const string Plugin = "YouLeadIWillFollow.CombatArena.esp";
    public const string CellEditorId = "YLIWFCombatArena";
    public string AddonName => "CombatTests";
    SkyrimMod IAddonPluginGenerator.Create() => Create();
    public const float Boundary = 1952;
    public const float ObstacleLeft = 480;
    public const float ObstacleRight = 608;
    public const float ObstacleEnd = 608;
    public const uint PartitionBase = 0x09A0EB; // WRIntCastleWallTallFreeStr01
    public const float PartitionScale = 1.25f;
    public const float PartitionX = 544;
    private const uint PathingCell = 2783551548;
    private static readonly ModKey OwnMod = ModKey.FromFileName(Plugin);
    private static readonly ModKey Skyrim = ModKey.FromFileName("Skyrim.esm");

    public static readonly (uint Id, string Name, P3Float Position, float Heading)[] Markers =
    [
        (0x810, "Observer", new(-1280, -1280, 16), 0),
        (0x811, "Defender", new(-1024, -256, 16), 0),
        (0x812, "MeleeEnemy", new(-1024, 256, 16), MathF.PI),
        (0x813, "Healer", new(-512, -1024, 16), 0),
        (0x814, "Ally", new(-512, 256, 16), MathF.PI),
        (0x815, "Pursuer", new(512, -1024, 16), MathF.PI / 2),
        (0x816, "ObstructedHealer", new(256, 0, 16), MathF.PI / 2),
        (0x817, "ObstructedAlly", new(1024, 0, 16), -MathF.PI / 2)
    ];

    private static FormKey Own(uint id) => new(OwnMod, id);
    private static FormKey Vanilla(uint id) => new(Skyrim, id);

    public static SkyrimMod Create()
    {
        var mod = new SkyrimMod(OwnMod, SkyrimRelease.SkyrimSE);
        mod.ModHeader.Author = ModInfo.Identity.Author;
        mod.ModHeader.Description = "Isolated combat testing arena. No startup scripts or encounters.";
        mod.ModHeader.Stats.NextFormID = 0xA00;
        mod.ModHeader.MasterReferences.Add(new MasterReference { Master = Skyrim });
        var cell = new Cell(Own(0x800), SkyrimRelease.SkyrimSE)
        {
            EditorID = CellEditorId,
            Name = "YLIWF Combat Arena",
            Flags = Cell.Flag.IsInteriorCell | Cell.Flag.CantTravelFromHere,
            WaterHeight = -2147483600f,
            Lighting = Lighting()
        };
        var subBlock = new CellSubBlock { BlockNumber = 4, GroupType = GroupTypeEnum.InteriorCellSubBlock };
        subBlock.Cells.Add(cell);
        var block = new CellBlock { BlockNumber = 8, GroupType = GroupTypeEnum.InteriorCellBlock };
        block.SubBlocks.Add(subBlock);
        mod.Cells.Add(block);

        AddRoom(cell);
        var entry = Place(0x803, 0x32, new(-1280, -1280, 16), 0, 1);
        entry.EditorID = "YLIWFCombatArenaEntry";
        entry.MajorRecordFlagsRaw = 0x400;
        cell.Persistent.Add(entry);
        foreach (var (Id, Name, Position, Heading) in Markers)
        {
            var placed = Place(Id, 0x34, Position, Heading, 1);
            placed.EditorID = "YLIWFCombatArena" + Name;
            placed.MajorRecordFlagsRaw = 0x400;
            cell.Persistent.Add(placed);
        }

        var navmesh = new NavigationMesh(Own(0x801), SkyrimRelease.SkyrimSE)
        {
            EditorID = "YLIWFCombatArenaNavmesh",
            Data = Navigation()
        };
        cell.NavigationMeshes.Add(navmesh);
        var map = new NavigationMeshInfoMap(Own(0x802), SkyrimRelease.SkyrimSE)
        {
            NavMeshVersion = 12,
            PreferredPathing = new PreferredPathing()
        };
        map.MapInfos.Add(new NavigationMapInfo
        {
            NavigationMesh = new FormLink<INavigationMeshGetter>(navmesh.FormKey),
            Point = new P3Float(0, 0, 0),
            Unknown2 = unchecked((int)PathingCell),
            Parent = new NavigationMapInfoCellParent { ParentCell = new FormLink<ICellGetter>(cell.FormKey) }
        });
        mod.NavigationMeshInfoMaps.Add(map);
        return mod;
    }

    private static CellLighting Lighting()
    {
        var ambient = Color.FromArgb(128, 128, 128);
        return new CellLighting
        {
            AmbientColor = ambient,
            DirectionalColor = Color.FromArgb(80, 80, 80),
            FogNearColor = Color.Black,
            FogFarColor = Color.Black,
            FogNear = 20000,
            FogFar = 40000,
            FogMax = 0,
            FogPower = 1,
            AmbientColors = new AmbientColors
            {
                DirectionalXPlus = ambient,
                DirectionalXMinus = ambient,
                DirectionalYPlus = ambient,
                DirectionalYMinus = ambient,
                DirectionalZPlus = ambient,
                DirectionalZMinus = ambient,
                Specular = Color.Black,
                Scale = 1
            }
        };
    }

    private static PlacedObject Place(uint id, uint baseId, P3Float position, float rotation, float scale) =>
        new(Own(id), SkyrimRelease.SkyrimSE)
        {
            Base = new FormLinkNullable<IPlaceableObjectGetter>(Vanilla(baseId)),
            Placement = new Placement { Position = position, Rotation = new P3Float(0, 0, rotation) },
            Scale = scale
        };

    private static void AddRoom(Cell cell)
    {
        // Vanilla Nordic small-room pieces at scale 2: 512-unit tiles and a
        // 640-unit ceiling. Keep full floor tiles beneath the freestanding divider.
        uint id = 0x900;
        for (var y = 0; y < 8; y++)
        {
            for (var x = 0; x < 8; x++)
            {
                uint model = 0x13F81; // NorRmSmMid01
                float rotation = 0;
                if (x is 0 or 7 && y is 0 or 7)
                {
                    model = 0x13F7C; // NorRmSmCorIn01
                    rotation = (x, y) switch
                    {
                        (0, 0) => 3 * MathF.PI / 2,
                        (7, 0) => MathF.PI,
                        (7, 7) => MathF.PI / 2,
                        _ => 0
                    };
                }
                else if (x is 0 or 7)
                {
                    model = 0x13F86; // NorRmSmWallSide01
                    rotation = x == 7 ? MathF.PI : 0;
                }
                else if (y is 0 or 7)
                {
                    model = 0x13F82; // NorRmSmWallFront01
                    rotation = y == 0 ? MathF.PI : 0;
                }
                cell.Temporary.Add(Place(id++, model, new P3Float(-1792 + x * 512, -1792 + y * 512, 0), rotation, 2));
            }
        }
        // This closed, two-sided piece has no attached room floor. Its vanilla
        // bounds are X +/-26, Y -109..107, Z -34..510. Scaled sections overlap
        // at their ends and reach the ceiling without exposing a room-kit back face.
        for (var section = 0; section < 4; section++)
        {
            var divider = Place((uint)(0x940 + section), PartitionBase,
                new P3Float(PartitionX, -384 + section * 256, 4), 0, PartitionScale);
            divider.EditorID = "YLIWFCombatArenaPartition" + section;
            cell.Temporary.Add(divider);
        }
    }

    private static float[] Axis(params float[] extra) =>
        [.. Enumerable.Range(0, 16).Select(i => -Boundary + i * 256).Append(Boundary).Concat(extra).Distinct().Order()];

    private static NavigationMeshData Navigation()
    {
        var xs = Axis(ObstacleLeft, ObstacleRight);
        var ys = Axis(-ObstacleEnd, ObstacleEnd);
        var data = new NavigationMeshData
        {
            NavmeshVersion = 12,
            CrcHash = PathingCell,
            Parent = new CellNavmeshParent { Parent = new FormLink<ICellGetter>(Own(0x800)) },
            NavmeshGridDivisor = 1,
            MaxDistanceX = Boundary * 2,
            MaxDistanceY = Boundary * 2,
            Min = new P3Float(-Boundary, -Boundary, 0),
            Max = new P3Float(Boundary, Boundary, 0)
        };
        for (var y = 0; y < ys.Length; y++)
        {
            for (var x = 0; x < xs.Length; x++)
            {
                data.Vertices.Add(new P3Float(xs[x], ys[y], 0));
            }
        }
        for (var y = 0; y < ys.Length - 1; y++)
        {
            for (var x = 0; x < xs.Length - 1; x++)
            {
                var centerX = (xs[x] + xs[x + 1]) / 2;
                var centerY = (ys[y] + ys[y + 1]) / 2;
                if (centerX > ObstacleLeft && centerX < ObstacleRight && Math.Abs(centerY) < ObstacleEnd)
                {
                    continue;
                }
                var a = checked((short)(y * xs.Length + x));
                var b = checked((short)(a + 1));
                var c = checked((short)(a + xs.Length));
                var d = checked((short)(c + 1));
                data.Triangles.Add(Triangle(a, b, d));
                data.Triangles.Add(Triangle(a, d, c));
            }
        }
        ConnectEdges(data.Triangles);

        // NVNM's one-cell spatial grid lists every triangle (count, then int16 IDs).
        var grid = new byte[4 + data.Triangles.Count * 2];
        BinaryPrimitives.WriteUInt32LittleEndian(grid, checked((uint)data.Triangles.Count));
        for (var i = 0; i < data.Triangles.Count; i++)
        {
            BinaryPrimitives.WriteInt16LittleEndian(grid.AsSpan(4 + i * 2), checked((short)i));
        }
        data.NavmeshGrid = grid;
        return data;
    }

    private static NavmeshTriangle Triangle(short a, short b, short c) => new()
    {
        Vertices = new P3Int16(a, b, c),
        EdgeLink_0_1 = -1,
        EdgeLink_1_2 = -1,
        EdgeLink_2_0 = -1,
        Flags = NavmeshTriangle.Flag.Found
    };

    private static void ConnectEdges(ExtendedList<NavmeshTriangle> triangles)
    {
        var edges = new Dictionary<(short, short), (short Triangle, int Edge)>();
        for (var i = 0; i < triangles.Count; i++)
        {
            var v = triangles[i].Vertices;
            var vertices = new[] { v.X, v.Y, v.Z };
            for (var edge = 0; edge < 3; edge++)
            {
                var a = vertices[edge];
                var b = vertices[(edge + 1) % 3];
                if (edges.Remove((b, a), out var neighbor))
                {
                    SetEdge(triangles[i], edge, neighbor.Triangle);
                    SetEdge(triangles[neighbor.Triangle], neighbor.Edge, checked((short)i));
                }
                else
                {
                    edges.Add((a, b), (checked((short)i), edge));
                }
            }
        }
    }

    private static void SetEdge(NavmeshTriangle triangle, int edge, short neighbor)
    {
        switch (edge)
        {
            case 0:
                triangle.EdgeLink_0_1 = neighbor;
                break;
            case 1:
                triangle.EdgeLink_1_2 = neighbor;
                break;
            case 2:
                triangle.EdgeLink_2_0 = neighbor;
                break;
            default:
                throw new ArgumentOutOfRangeException(nameof(edge));
        }
    }

}
