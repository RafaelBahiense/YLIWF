using System.Text;
using System.Text.Json.Nodes;
using ModTools.Packaging;
using ModTools.Papyrus;
using static TestSupport;

internal static class MetadataTests
{
    public static void Run(string root, string temporary)
    {
        CheckReleaseTimestamp(root, temporary);
        CheckPexSanitation();
        CheckIdentity(root);
        CheckVersions();
    }

    private static void CheckVersions()
    {
        foreach (var valid in new[] { "0.0.0", "1.2.3", "1.2.3.4", "255.255.4095.15" })
        {
            Check(ReleaseVersion.Parse(valid, "test") == valid, "Valid SKSE version was rejected");
        }
        foreach (var invalid in new[] { "", "1.2", "01.2.3", "1.2.3.4.5", "1.2.3-beta", " 1.2.3", "256.0.0", "1.256.0", "1.2.4096", "1.2.3.16", "99999999999999999999.0.0" })
        {
            Fails(() => ReleaseVersion.Parse(invalid, "test"));
        }
    }

    private static void CheckReleaseTimestamp(string root, string temporary)
    {
        var previousEpoch = Environment.GetEnvironmentVariable("SOURCE_DATE_EPOCH");
        try
        {
            Environment.SetEnvironmentVariable("SOURCE_DATE_EPOCH", "1791462459");
            var expectedDate = DateTimeOffset.FromUnixTimeSeconds(1791462458);
            Check(ReleaseTimestamp.Read(root) == expectedDate, "Explicit release date was not rounded to ZIP precision");
            Environment.SetEnvironmentVariable("SOURCE_DATE_EPOCH", "invalid");
            Fails(() => ReleaseTimestamp.Read(root));
            Environment.SetEnvironmentVariable("SOURCE_DATE_EPOCH", "0");
            Fails(() => ReleaseTimestamp.Read(root));
            Environment.SetEnvironmentVariable("SOURCE_DATE_EPOCH", null);
            var extractedSource = Path.Combine(temporary, "extracted-source");
            var extractedManifest = Path.Combine(extractedSource, Artifacts.Manifest);
            Directory.CreateDirectory(Path.GetDirectoryName(extractedManifest)!);
            File.WriteAllText(extractedManifest, "{\"archive_timestamp\":1791462458}");
            Check(ReleaseTimestamp.Read(extractedSource) == expectedDate, "Extracted sources lost the original release date");
            Fails(() => ReleaseTimestamp.Read(temporary));
        }
        finally
        {
            Environment.SetEnvironmentVariable("SOURCE_DATE_EPOCH", previousEpoch);
        }
    }

    private static void CheckPexSanitation()
    {
        var privatePex = PexFixture("Test.psc");
        var cleanPex = PexMetadata.Sanitize(privatePex);
        Check(!Encoding.UTF8.GetString(cleanPex).Contains("Private", StringComparison.Ordinal), "PEX retains machine identity");
        Check(cleanPex.AsSpan(8, 8).SequenceEqual(new byte[8]), "PEX retains compile time");
        Check(cleanPex.AsSpan(33, 8).SequenceEqual(new byte[8]), "PEX retains debug modification time");
        Check(cleanPex.AsSpan(41).SequenceEqual(privatePex.AsSpan(privatePex.Length - 6)), "PEX metadata sanitation changed debug tables or bytecode");
        Check(PexMetadata.Sanitize(cleanPex).SequenceEqual(cleanPex), "PEX sanitation is not idempotent");
        Fails(() => PexMetadata.Sanitize(privatePex[..20]));
    }

    private static void CheckIdentity(string root)
    {
        var identityJson = File.ReadAllText(Path.Combine(root, "mod.json"));
        Check(ModInfo.Identity == ModIdentity.Parse(identityJson), "Embedded identity is stale; rebuild ModTools after editing mod.json");
        var branding = JsonNode.Parse(identityJson)!.AsObject();
        branding["displayName"] = "Followers \"Together\"";
        branding["shortName"] = "FT";
        var renamed = ModIdentity.Parse(branding.ToJsonString());
        Check(renamed.DisplayName == "Followers \"Together\"" && renamed.ShortName == "FT", "Branding did not update");
        Check(renamed.PluginFile == ModInfo.PluginFile && renamed.DllFile == ModInfo.Identity.DllFile &&
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
