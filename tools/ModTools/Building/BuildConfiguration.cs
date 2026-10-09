namespace ModTools.Building;

public enum BuildConfiguration
{
    Release,
    Debug,
    RelWithDebInfo
}

internal static class BuildConfigurations
{
    internal static string CMakeName(this BuildConfiguration configuration) => configuration switch
    {
        BuildConfiguration.Release => "Release",
        BuildConfiguration.Debug => "Debug",
        BuildConfiguration.RelWithDebInfo => "RelWithDebInfo",
        _ => throw new ArgumentOutOfRangeException(nameof(configuration))
    };

    internal static string Preset(this BuildConfiguration configuration) => configuration.CMakeName().ToLowerInvariant();

    internal static BuildConfiguration Parse(string value)
    {
        foreach (var configuration in Enum.GetValues<BuildConfiguration>())
        {
            if (configuration.CMakeName() == value)
            {
                return configuration;
            }
        }
        throw new ArgumentException($"Unknown configuration: {value}");
    }
}
