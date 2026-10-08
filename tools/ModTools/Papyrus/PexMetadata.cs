using System.Buffers.Binary;
using System.Text;

namespace ModTools.Papyrus;

public static class PexMetadata
{
    // Skyrim 3.2 uses big-endian integers. Only metadata is rewritten; the
    // string table, bytecode, and debug line mappings keep their original bytes.
    // Layout: github.com/Orvid/Caprica/blob/master/Caprica/pex/PexFile.cpp
    public static void Sanitize(string path) => File.WriteAllBytes(path, Sanitize(File.ReadAllBytes(path)));

    public static byte[] Sanitize(byte[] data)
    {
        if (data.Length < 16 || !data.AsSpan(0, 8).SequenceEqual(new byte[] { 0xfa, 0x57, 0xc0, 0xde, 3, 2, 0, 1 }))
        {
            throw new InvalidDataException("Expected a Skyrim 3.2 PEX header");
        }
        var offset = 16;
        string ReadString()
        {
            Require(2);
            var length = BinaryPrimitives.ReadUInt16BigEndian(data.AsSpan(offset, 2));
            offset += 2;
            Require(length);
            var text = Encoding.UTF8.GetString(data, offset, length);
            offset += length;
            return text;
        }
        void Require(int length)
        {
            if (length > data.Length - offset)
            {
                throw new InvalidDataException("Truncated PEX metadata");
            }
        }
        var source = ReadString().Replace('\\', '/').Split('/')[^1];
        if (string.IsNullOrWhiteSpace(source) || !source.EndsWith(".psc", StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidDataException("Invalid PEX source filename");
        }
        ReadString(); // Compiler username.
        ReadString(); // Compiler computer name.
        var bodyOffset = offset;
        Require(2);
        var strings = BinaryPrimitives.ReadUInt16BigEndian(data.AsSpan(offset, 2));
        offset += 2;
        for (var i = 0; i < strings; ++i)
        {
            ReadString();
        }
        Require(1);
        var debug = data[offset++];
        if (debug > 1)
        {
            throw new InvalidDataException("Invalid PEX debug flag");
        }
        var body = data.AsSpan(bodyOffset).ToArray();
        if (debug == 1)
        {
            Require(8);
            body.AsSpan(offset - bodyOffset, 8).Clear(); // Source modification time.
        }
        using var output = new MemoryStream();
        output.Write(data.AsSpan(0, 8));
        output.Write(new byte[8]); // Compilation time.
        var filename = Encoding.UTF8.GetBytes(source);
        Span<byte> lengthBytes = stackalloc byte[2];
        BinaryPrimitives.WriteUInt16BigEndian(lengthBytes, checked((ushort)filename.Length));
        output.Write(lengthBytes);
        output.Write(filename);
        output.Write(new byte[4]); // Empty username and computer-name strings.
        output.Write(body);
        return output.ToArray();
    }
}
