using System.Diagnostics;

namespace ModTools.Building;

public static class ProcessRunner
{
    public static void Run(string executable, IEnumerable<string> arguments, string root)
    {
        var start = new ProcessStartInfo(executable) { WorkingDirectory = root, UseShellExecute = false };
        foreach (var argument in arguments)
        {
            start.ArgumentList.Add(argument);
        }

        Console.WriteLine($"Running: {executable} {string.Join(' ', start.ArgumentList)}");
        using var process = Process.Start(start) ?? throw new IOException($"Could not start {executable}");
        process.WaitForExit();
        if (process.ExitCode != 0)
        {
            throw new IOException($"{executable} failed (exit {process.ExitCode})");
        }
    }
}
