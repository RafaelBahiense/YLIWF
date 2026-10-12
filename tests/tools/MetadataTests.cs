using System.Text;
using System.Text.Json.Nodes;
using ModTools.Packaging;
using ModTools.Papyrus;
using static ModTools.Tests.TestSupport;

namespace ModTools.Tests;

public sealed class MetadataTests : TestWorkspace
{
    [Theory]
    [InlineData("0.0.0")]
    [InlineData("1.2.3")]
    [InlineData("1.2.3.4")]
    [InlineData("255.255.4095.15")]
    public void ValidVersions(string version) => Assert.Equal(version, ReleaseVersion.Parse(version, "test"));

    [Theory]
    [InlineData("")]
    [InlineData("1.2")]
    [InlineData("01.2.3")]
    [InlineData("1.2.3.4.5")]
    [InlineData("1.2.3-beta")]
    [InlineData(" 1.2.3")]
    [InlineData("256.0.0")]
    [InlineData("1.256.0")]
    [InlineData("1.2.4096")]
    [InlineData("1.2.3.16")]
    [InlineData("99999999999999999999.0.0")]
    public void InvalidVersions(string version) => Assert.Throws<InvalidDataException>(() => ReleaseVersion.Parse(version, "test"));

    [Fact]
    public void ReleaseTimestampPreservesZipPrecision()
    {
        var previousEpoch = Environment.GetEnvironmentVariable("SOURCE_DATE_EPOCH");
        try
        {
            Environment.SetEnvironmentVariable("SOURCE_DATE_EPOCH", "1791462459");
            var expectedDate = DateTimeOffset.FromUnixTimeSeconds(1791462458);
            Assert.True(ReleaseTimestamp.Read(root) == expectedDate, "Explicit release date was not rounded to ZIP precision");
            Environment.SetEnvironmentVariable("SOURCE_DATE_EPOCH", "invalid");
            Fails(() => ReleaseTimestamp.Read(root));
            Environment.SetEnvironmentVariable("SOURCE_DATE_EPOCH", "0");
            Fails(() => ReleaseTimestamp.Read(root));
            Environment.SetEnvironmentVariable("SOURCE_DATE_EPOCH", null);
            var extractedSource = Path.Combine(temporary, "extracted-source");
            var extractedManifest = Path.Combine(extractedSource, Artifacts.Manifest);
            Directory.CreateDirectory(Path.GetDirectoryName(extractedManifest)!);
            File.WriteAllText(extractedManifest, "{\"archive_timestamp\":1791462458}");
            Assert.True(ReleaseTimestamp.Read(extractedSource) == expectedDate, "Extracted sources lost the original release date");
            Fails(() => ReleaseTimestamp.Read(temporary));
        }
        finally
        {
            Environment.SetEnvironmentVariable("SOURCE_DATE_EPOCH", previousEpoch);
        }
    }

    [Fact]
    public void PexSanitation()
    {
        var privatePex = PexFixture("Test.psc");
        var cleanPex = PexMetadata.Sanitize(privatePex);
        Assert.False(Encoding.UTF8.GetString(cleanPex).Contains("Private", StringComparison.Ordinal), "PEX retains machine identity");
        Assert.True(cleanPex.AsSpan(8, 8).SequenceEqual(new byte[8]), "PEX retains compile time");
        Assert.True(cleanPex.AsSpan(33, 8).SequenceEqual(new byte[8]), "PEX retains debug modification time");
        Assert.True(cleanPex.AsSpan(41).SequenceEqual(privatePex.AsSpan(privatePex.Length - 6)), "PEX metadata sanitation changed debug tables or bytecode");
        Assert.True(PexMetadata.Sanitize(cleanPex).SequenceEqual(cleanPex), "PEX sanitation is not idempotent");
        Fails(() => PexMetadata.Sanitize(privatePex[..20]));
    }

    [Fact]
    public void Identity()
    {
        var identityJson = File.ReadAllText(Path.Combine(root, "mod.json"));
        Assert.True(ModInfo.Identity == ModIdentity.Parse(identityJson), "Embedded identity is stale; rebuild ModTools after editing mod.json");
        var branding = JsonNode.Parse(identityJson)!.AsObject();
        branding["displayName"] = "Followers \"Together\"";
        branding["shortName"] = "FT";
        var renamed = ModIdentity.Parse(branding.ToJsonString());
        Assert.True(renamed.DisplayName == "Followers \"Together\"" && renamed.ShortName == "FT", "Branding did not update");
        Assert.True(renamed.PluginFile == ModInfo.PluginFile && renamed.DllFile == ModInfo.Identity.DllFile &&
            renamed.IniFile == ModInfo.Identity.IniFile && renamed.NativeScript == ModInfo.NativeScript,
            "Display rename changed compatibility identities");
        foreach (var (field, invalidName) in new[] { ("binaryName", "../outside"), ("scriptPrefix", "bad-prefix"), ("pluginFile", "../Other.esp"), ("displayName", "") })
        {
            var invalidIdentity = JsonNode.Parse(identityJson)!.AsObject();
            invalidIdentity[field] = invalidName;
            Fails(() => ModIdentity.Parse(invalidIdentity.ToJsonString()));
        }
    }
}
