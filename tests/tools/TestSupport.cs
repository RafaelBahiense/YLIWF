using System.Buffers.Binary;
using System.IO.Compression;
using System.Text;

internal static class TestSupport
{
    public static void Check(bool condition, string description)
    {
        if (!condition)
        {
            throw new InvalidOperationException(description);
        }
    }
    public static void Fails(Action action)
    {
        try
        {
            action();
        }
        catch (InvalidDataException) { return; }
        catch (IOException) { return; }
        catch (OverflowException) { return; }
        throw new InvalidOperationException("Expected invalid input to fail");
    }

    private static readonly string[] ProjectNotices = ["LICENSE", "NOTICE", "CREDITS.md"];

    public static void CheckInstallNotices(ZipArchive archive, IReadOnlyDictionary<string, string> dependencyNotices)
    {
        var prefix = ModInfo.Identity.DocumentationDirectory + "/";
        var expected = ProjectNotices.Concat(dependencyNotices.Keys)
            .Select(name => prefix + name).ToHashSet(StringComparer.Ordinal);
        var actual = archive.Entries.Where(entry => entry.FullName.StartsWith("docs/", StringComparison.Ordinal))
            .Select(entry => entry.FullName).ToHashSet(StringComparer.Ordinal);
        Check(actual.SetEquals(expected), "Install ZIP must contain only project and dependency notices under docs");
        Check(!archive.Entries.Any(entry => entry.FullName.EndsWith(".ini", StringComparison.OrdinalIgnoreCase)),
            "Install ZIP must not overwrite user configuration");
        using var reader = new StreamReader(archive.GetEntry(prefix + "NOTICE")!.Open());
        Check(reader.ReadToEnd().Contains("THIS MOD IS NOT MADE, GUARANTEED OR SUPPORTED BY ZENIMAX OR ITS AFFILIATES.", StringComparison.Ordinal),
            "Install ZIP omitted Bethesda disclaimer");
    }
    public static byte[] DllFixture()
    {
        // Export-table fixture only. It is never published as an installable plugin.
        var bytes = new byte[1024];
        void U32(int offset, uint value) => BinaryPrimitives.WriteUInt32LittleEndian(bytes.AsSpan(offset), value);
        void U16(int offset, ushort value) => BinaryPrimitives.WriteUInt16LittleEndian(bytes.AsSpan(offset), value);
        bytes[0] = (byte)'M';
        bytes[1] = (byte)'Z';
        U32(60, 128);
        U32(128, 0x4550);
        U16(132, 0x8664);
        U16(134, 1);
        U16(148, 240);
        U16(150, 0x2000);
        U16(152, 0x20b);
        U32(264, 0x1000);
        U32(268, 40);
        U32(400, 512);
        U32(404, 0x1000);
        U32(408, 512);
        U32(412, 512);
        U32(536, 2);
        U32(544, 0x1040);
        U32(576, 0x1060);
        U32(580, 0x1080);
        Encoding.ASCII.GetBytes("SKSEPlugin_Load\0").CopyTo(bytes, 608);
        Encoding.ASCII.GetBytes("SKSEPlugin_Version\0").CopyTo(bytes, 640);
        return bytes;
    }
    public static void Scripts(string source, string output)
    {
        Directory.CreateDirectory(output);
        foreach (var file in Directory.GetFiles(source, "*.psc"))
        {
            File.WriteAllBytes(Path.Combine(output, Path.GetFileNameWithoutExtension(file) + ".pex"), PexFixture(Path.GetFileName(file)));
        }
    }
    public static byte[] PexFixture(string filename)
    {
        using var stream = new MemoryStream();
        stream.Write([0xfa, 0x57, 0xc0, 0xde, 3, 2, 0, 1]);
        stream.Write(Enumerable.Repeat((byte)123, 8).ToArray());
        Span<byte> length = stackalloc byte[2];
        foreach (var text in new[] { "C:\\Users\\PrivateUser\\repo\\" + filename, "PrivateUser", "PrivateComputer" })
        {
            var bytes = Encoding.UTF8.GetBytes(text);
            BinaryPrimitives.WriteUInt16BigEndian(length, checked((ushort)bytes.Length));
            stream.Write(length);
            stream.Write(bytes);
        }
        stream.Write([0, 0, 1]); // Empty string table; debug section present.
        stream.Write(Enumerable.Repeat((byte)124, 8).ToArray());
        stream.Write([0, 0, 0, 0, 0, 0]); // Empty debug, flag and object tables.
        return stream.ToArray();
    }
}
