using Mutagen.Bethesda.Plugins;
using Mutagen.Bethesda.Skyrim;

internal static partial class FollowerPlugin
{
    private static void AddCells(SkyrimMod mod)
    {
        mod.Cells.Unknown = 16843009;
        var interiorCellBlock = new CellBlock
        {
            BlockNumber = 4,
            GroupType = GroupTypeEnum.InteriorCellBlock,
            Unknown = 16843009
        };
        var interiorCellSubBlock = new CellSubBlock
        {
            BlockNumber = 8,
            GroupType = GroupTypeEnum.InteriorCellSubBlock,
            Unknown = 16843009
        };
        var homeMarkerCell = new Cell(OwnForm(0x000950), SkyrimRelease.SkyrimSE)
        {
            IsCompressed = true,
            MajorRecordFlagsRaw = 262144,
            EditorID = "YLIWF_HomeMarkerCell",
            SkyrimMajorRecordFlags = SkyrimMajorRecord.SkyrimMajorRecordFlag.Compressed,
            Flags = Cell.Flag.IsInteriorCell
        };
        var cellLighting = new CellLighting
        {
            AmbientColor = System.Drawing.Color.FromArgb(unchecked((int)0x00000000)),
            DirectionalColor = System.Drawing.Color.FromArgb(unchecked((int)0x00000000)),
            FogNearColor = System.Drawing.Color.FromArgb(unchecked((int)0x00B0DCF7)),
            FogNear = 340.0f,
            FogFar = 14000.0f,
            FogPower = 1.0f
        };
        var ambientColors = new AmbientColors
        {
            DirectionalXPlus = System.Drawing.Color.FromArgb(unchecked((int)0x00000000)),
            DirectionalXMinus = System.Drawing.Color.FromArgb(unchecked((int)0x00000000)),
            DirectionalYPlus = System.Drawing.Color.FromArgb(unchecked((int)0x00000000)),
            DirectionalYMinus = System.Drawing.Color.FromArgb(unchecked((int)0x00000000)),
            DirectionalZPlus = System.Drawing.Color.FromArgb(unchecked((int)0x00000000)),
            DirectionalZMinus = System.Drawing.Color.FromArgb(unchecked((int)0x00000000)),
            Specular = System.Drawing.Color.FromArgb(unchecked((int)0x00B0DCF7)),
            Scale = 1.0f
        };
        cellLighting.AmbientColors = ambientColors;
        cellLighting.FogFarColor = System.Drawing.Color.FromArgb(unchecked((int)0x00B0DCF7));
        cellLighting.FogMax = 1.0f;
        cellLighting.Inherits = CellLighting.Inherit.AmbientColor | CellLighting.Inherit.DirectionalColor | CellLighting.Inherit.FogColor | CellLighting.Inherit.FogNear | CellLighting.Inherit.FogFar | CellLighting.Inherit.DirectionalRotation | CellLighting.Inherit.DirectionalFade | CellLighting.Inherit.ClipDistance | CellLighting.Inherit.FogPower | CellLighting.Inherit.FogMax | CellLighting.Inherit.LightFadeDistances;
        homeMarkerCell.Lighting = cellLighting;
        homeMarkerCell.LightingTemplate = new FormLink<ILightingTemplateGetter>(SkyrimForm(0x01952F));
        homeMarkerCell.WaterHeight = -2147483600.0f;
        var homeMarker00 = new PlacedObject(OwnForm(0x000951), SkyrimRelease.SkyrimSE)
        {
            MajorRecordFlagsRaw = 1024,
            EditorID = "YLIWF_HomeMarker00",
            SkyrimMajorRecordFlags = (SkyrimMajorRecord.SkyrimMajorRecordFlag)0x400,
            Base = new FormLinkNullable<IPlaceableObjectGetter>(SkyrimForm(0x00003B))
        };
        var placement = new Placement();
        homeMarker00.Placement = placement;
        homeMarkerCell.Persistent.Add(homeMarker00);
        var homeMarker01 = new PlacedObject(OwnForm(0x000952), SkyrimRelease.SkyrimSE)
        {
            MajorRecordFlagsRaw = 1024,
            EditorID = "YLIWF_HomeMarker01",
            SkyrimMajorRecordFlags = (SkyrimMajorRecord.SkyrimMajorRecordFlag)0x400,
            Base = new FormLinkNullable<IPlaceableObjectGetter>(SkyrimForm(0x00003B))
        };
        var placement2 = new Placement();
        homeMarker01.Placement = placement2;
        homeMarkerCell.Persistent.Add(homeMarker01);
        var homeMarker02 = new PlacedObject(OwnForm(0x000953), SkyrimRelease.SkyrimSE)
        {
            MajorRecordFlagsRaw = 1024,
            EditorID = "YLIWF_HomeMarker02",
            SkyrimMajorRecordFlags = (SkyrimMajorRecord.SkyrimMajorRecordFlag)0x400,
            Base = new FormLinkNullable<IPlaceableObjectGetter>(SkyrimForm(0x00003B))
        };
        var placement3 = new Placement();
        homeMarker02.Placement = placement3;
        homeMarkerCell.Persistent.Add(homeMarker02);
        var homeMarker03 = new PlacedObject(OwnForm(0x000954), SkyrimRelease.SkyrimSE)
        {
            MajorRecordFlagsRaw = 1024,
            EditorID = "YLIWF_HomeMarker03",
            SkyrimMajorRecordFlags = (SkyrimMajorRecord.SkyrimMajorRecordFlag)0x400,
            Base = new FormLinkNullable<IPlaceableObjectGetter>(SkyrimForm(0x00003B))
        };
        var placement4 = new Placement();
        homeMarker03.Placement = placement4;
        homeMarkerCell.Persistent.Add(homeMarker03);
        var homeMarker04 = new PlacedObject(OwnForm(0x000955), SkyrimRelease.SkyrimSE)
        {
            MajorRecordFlagsRaw = 1024,
            EditorID = "YLIWF_HomeMarker04",
            SkyrimMajorRecordFlags = (SkyrimMajorRecord.SkyrimMajorRecordFlag)0x400,
            Base = new FormLinkNullable<IPlaceableObjectGetter>(SkyrimForm(0x00003B))
        };
        var placement5 = new Placement();
        homeMarker04.Placement = placement5;
        homeMarkerCell.Persistent.Add(homeMarker04);
        var homeMarker05 = new PlacedObject(OwnForm(0x000956), SkyrimRelease.SkyrimSE)
        {
            MajorRecordFlagsRaw = 1024,
            EditorID = "YLIWF_HomeMarker05",
            SkyrimMajorRecordFlags = (SkyrimMajorRecord.SkyrimMajorRecordFlag)0x400,
            Base = new FormLinkNullable<IPlaceableObjectGetter>(SkyrimForm(0x00003B))
        };
        var placement6 = new Placement();
        homeMarker05.Placement = placement6;
        homeMarkerCell.Persistent.Add(homeMarker05);
        var homeMarker06 = new PlacedObject(OwnForm(0x000957), SkyrimRelease.SkyrimSE)
        {
            MajorRecordFlagsRaw = 1024,
            EditorID = "YLIWF_HomeMarker06",
            SkyrimMajorRecordFlags = (SkyrimMajorRecord.SkyrimMajorRecordFlag)0x400,
            Base = new FormLinkNullable<IPlaceableObjectGetter>(SkyrimForm(0x00003B))
        };
        var placement7 = new Placement();
        homeMarker06.Placement = placement7;
        homeMarkerCell.Persistent.Add(homeMarker06);
        var homeMarker07 = new PlacedObject(OwnForm(0x000958), SkyrimRelease.SkyrimSE)
        {
            MajorRecordFlagsRaw = 1024,
            EditorID = "YLIWF_HomeMarker07",
            SkyrimMajorRecordFlags = (SkyrimMajorRecord.SkyrimMajorRecordFlag)0x400,
            Base = new FormLinkNullable<IPlaceableObjectGetter>(SkyrimForm(0x00003B))
        };
        var placement8 = new Placement();
        homeMarker07.Placement = placement8;
        homeMarkerCell.Persistent.Add(homeMarker07);
        homeMarkerCell.MajorFlags = (Cell.MajorFlag)0x40000;
        interiorCellSubBlock.Cells.Add(homeMarkerCell);
        interiorCellBlock.SubBlocks.Add(interiorCellSubBlock);
        mod.Cells.Add(interiorCellBlock);
    }

}
