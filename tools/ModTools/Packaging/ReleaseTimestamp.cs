using System.Diagnostics;
using System.Globalization;
using System.Text.Json;

namespace ModTools.Packaging;

public static class ReleaseTimestamp
{
    public static DateTimeOffset Read(string root)
    {
        var epoch = Environment.GetEnvironmentVariable("SOURCE_DATE_EPOCH");
        if (epoch != null)
        {
            return FromEpoch(epoch);
        }
        if (Directory.Exists(Path.Combine(root, ".git")) || File.Exists(Path.Combine(root, ".git")))
        {
            var info = new ProcessStartInfo("git")
            {
                WorkingDirectory = root, UseShellExecute = false,
                RedirectStandardOutput = true, RedirectStandardError = true
            };
            foreach (var argument in new[] { "show", "-s", "--format=%ct", "HEAD" })
            {
                info.ArgumentList.Add(argument);
            }
            using var process = Process.Start(info) ?? throw new IOException("Could not read release commit timestamp");
            var output = process.StandardOutput.ReadToEnd();
            process.StandardError.ReadToEnd();
            process.WaitForExit();
            if (process.ExitCode != 0)
            {
                throw new InvalidDataException("Could not read HEAD timestamp; set SOURCE_DATE_EPOCH explicitly");
            }
            return FromEpoch(output.Trim());
        }
        // Corresponding-source ZIPs retain the release date for builds without Git.
        var manifestPath = Path.Combine(root, Artifacts.Manifest);
        if (File.Exists(manifestPath))
        {
            using var manifest = JsonDocument.Parse(File.ReadAllText(manifestPath));
            if (manifest.RootElement.TryGetProperty(ManifestFields.ArchiveTimestamp, out var timestamp))
            {
                return FromEpoch(timestamp.GetRawText());
            }
        }
        throw new InvalidDataException("No release commit timestamp; set SOURCE_DATE_EPOCH for builds without Git");
    }
    private static DateTimeOffset FromEpoch(string epoch)
    {
        if (!long.TryParse(epoch, NumberStyles.None, CultureInfo.InvariantCulture, out var seconds) || seconds < 315532800 || seconds > 4354819199)
        {
            throw new InvalidDataException("Release timestamp must be Unix seconds within ZIP's 1980–2107 date range");
        }
        // ZIP timestamps have two-second precision and carry no timezone.
        return DateTimeOffset.FromUnixTimeSeconds(seconds - seconds % 2);
    }
}
