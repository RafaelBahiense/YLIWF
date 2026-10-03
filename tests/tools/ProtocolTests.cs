using System.Text.RegularExpressions;

internal static partial class ProtocolTests
{
    internal static readonly string[] sourceArray = new[] { "Sync", "Repair", "Follow", "Wait", "Dismiss", "ClearSlot", "Adopt", "Release", "PromotePrimary" };

    public static void Run(string root)
    {
        var native = File.ReadAllText(Path.Combine(root, "include/ControllerRules.h"));
        var script = File.ReadAllText(Path.Combine(root, "src/papyrus/core/DialogueFollowerScript.psc"));
        void Require(bool condition, string message)
        {
            if (!condition)
            {
                throw new InvalidDataException("Controller ABI: " + message);
            }
        }
        Dictionary<string, int> CheckEnum(string name, string[] names, int first)
        {
            var body = Regex.Match(native, @"enum class " + name + @"\s*:\s*std::int32_t\s*\{([^}]+)\}").Groups[1].Value;
            var entries = body.Split(',', StringSplitOptions.TrimEntries | StringSplitOptions.RemoveEmptyEntries);
            Require(entries.Length == names.Length, name + " entries changed; review saved-format compatibility");
            var values = new Dictionary<string, int>();
            for (var i = 0; i < names.Length; ++i)
            {
                Require(Regex.IsMatch(entries[i], @"^" + names[i] + @"\s*=\s*" + (first + i) + @"$"), names[i] + " saved number changed");
                values.Add(names[i], first + i);
            }
            return values;
        }
        var operations = CheckEnum("Operation", ["Sync", "Recruit", "Follow", "Wait", "Dismiss", "Death", "Timeout", "Unload", "ReservedPrepare", "ReservedCleanup", "ReservedPromote", "ReservedPop", "AnimalRecruit", "AnimalWait", "AnimalFollow", "AnimalDismiss", "Repair", "ClearSlot", "Adopt", "Release", "ReservedAddExtra", "ReservedClearDead", "HomeAssign", "HomeRemove", "DialogueFollow", "DialogueWait", "DialogueDismiss", "PromotePrimary"], 0);
        var effects = CheckEnum("Effect", ["CancelTimer", "Clear", "Assign", "Teammate", "Prepare", "Release", "Protection", "Waiting", "Evaluate", "ReservedUpdate", "Relationship", "StopCombat", "Cleanup", "Message", "Hireling", "DismissLine", "EndDismissLine", "StartTimer", "HideObjective", "Speaker", "ClearSpeaker", "AnimalPrepare", "AnimalCount", "HomeClear", "HomeMove", "HomeAssign", "HomeFaction", "HomeEvaluate", "SwapPrimary"], 1);
        string FunctionBody(string text, string name) => Regex.Match(text,
            @"(?m)^(?:\w+(?:\[\])? )?Function " + name + @"\([^\r\n]*\)(?: Global)?\r?\n([\s\S]*?)^EndFunction").Groups[1].Value;
        var functions = new[] { "SetFollower", "SetAnimal", "FollowerWait", "AnimalWait", "FollowerFollow", "AnimalFollow", "DismissFollower", "DismissAnimal" };
        Require(FacadeMethodsRegex().Matches(script).Count == functions.Length, "vanilla facade must contain exactly eight methods");
        Require(script.Contains("extends Quest Conditional", StringComparison.Ordinal), "vanilla facade still inherits internal storage");
        foreach (var function in functions)
        {
            var body = FunctionBody(script, function).Trim();
            Require(body.StartsWith("YLIWF_SKSE." + function + "(", StringComparison.Ordinal) && !body.Contains('\n'), function + " is not a single native forwarding call");
        }
        var api = File.ReadAllText(Path.Combine(root, "src/papyrus/core/YLIWF_SKSE.psc"));
        var declarations = NativeDeclarationRegex().Matches(api).Select(match => match.Groups[1].Value).ToArray();
        var declared = declarations.ToHashSet(StringComparer.OrdinalIgnoreCase);
        Require(declared.Count == declarations.Length, "duplicate native declarations");
        var callers = Directory.EnumerateFiles(Path.Combine(root, "src/papyrus"), "*.psc", SearchOption.AllDirectories)
            .SelectMany(path => NativeCallRegex().Matches(File.ReadAllText(path)))
            .Select(match => match.Groups[1].Value).ToHashSet(StringComparer.OrdinalIgnoreCase);
        Require(callers.IsSubsetOf(declared), "a Papyrus caller references a removed native export");
        callers.Add("DebugInt"); // Keep the typed integer logger with the other diagnostic helpers.
        Require(declared.SetEquals(callers), "unused native export added without a caller");
        var registrations = Directory.EnumerateFiles(Path.Combine(root, "src/native"), "*.cpp")
            .SelectMany(path => NativeRegistrationRegex().Matches(File.ReadAllText(path)))
            .Select(match => match.Groups[1].Value).ToArray();
        Require(registrations.Length == declared.Count && declared.SetEquals(registrations), "native declarations and VM registrations differ");
        Require(!File.Exists(Path.Combine(root, "src/papyrus/core/YLIWF_State.psc")), "native state still has a Papyrus container");
        Require(!script.Contains("Controller", StringComparison.Ordinal) && !script.Contains("NativeWait", StringComparison.Ordinal), "execution fields leaked into the facade");
        var executor = File.ReadAllText(Path.Combine(root, "src/native/ControllerExecutor.cpp"));
        var controller = File.ReadAllText(Path.Combine(root, "src/native/Controller.cpp"));
        foreach (var effect in effects.Keys)
        {
            Require((executor + controller).Contains("case Effect::" + effect + ":", StringComparison.Ordinal), effect + " has no native execution path");
        }
        Require(!PapyrusControlFlowRegex().IsMatch(script) && !script.Contains("Utility.Wait", StringComparison.Ordinal), "controller policy/executor moved back into Papyrus");
        Require(!controller.Contains("DispatchMethodCall(object, \"DrainQueue\"", StringComparison.Ordinal), "native queue still starts a Papyrus worker");
        var bridge = File.ReadAllText(Path.Combine(root, "src/papyrus/core/YLIWF_Engine.psc"));
        Require(string.IsNullOrEmpty(FunctionBody(bridge, "CancelTimer")), "native timers still have a Papyrus cleanup adapter");
        Require(!native.Contains("plan.Add(Effect::ReservedUpdate", StringComparison.Ordinal), "planner emits a retired count-update effect");
        foreach (Match call in EngineAdapterRegex().Matches(executor))
        {
            var body = FunctionBody(bridge, call.Groups[1].Value);
            Require(body.Contains("SetNativeEffectCurrent", StringComparison.Ordinal) && body.Contains("SetNativeEffectReturned", StringComparison.Ordinal), "engine adapter lacks saved completion/stale-call guards");
        }
        foreach (var removed in new[] { "FollowerAliasScript", "YLIWF_FollowerAliasScript", "YLIWF_Log" })
            Require(!File.Exists(Path.Combine(root, "src/papyrus/core", removed + ".psc")), "redundant script remains: " + removed);
        var events = File.ReadAllText(Path.Combine(root, "src/native/plugin.cpp"));
        foreach (var name in new[] { "Activated", "Died", "CombatChanged", "Unloaded" })
            Require(events.Contains("controller::" + name + "(", StringComparison.Ordinal), "native event forwarding missing: " + name);
        var ui = File.ReadAllText(Path.Combine(root, "src/native/Debug.cpp"));
        Require(ui.Contains("controller::SubmitDebug(", StringComparison.Ordinal) && !ui.Contains("DispatchMethodCall(object, \"DebugCommand\"", StringComparison.Ordinal), "UI still depends on a Papyrus controller method");
        var home = File.ReadAllText(Path.Combine(root, "src/papyrus/core/YLIWF_HomeTopicScript.psc"));
        Require(FunctionBody(home, "Fragment_0").Trim() == "YLIWF_SKSE.SetHome(akSpeakerRef as Actor, HomeAction, HomeQuest, HomeFaction)", "home fragment contains controller policy");
        var debug = DebugOperationsRegex().Match(executor).Groups[1].Value;
        var debugOperations = OperationNameRegex().Matches(debug).Select(match => match.Groups[1].Value);
        Require(debugOperations.SequenceEqual(sourceArray), "debug commands send different operations");
    }

    [GeneratedRegex(@"(?m)^(?:\w+(?:\[\])? )?Function (\w+)\([^\r\n]*\) Global Native\r?$")]
    private static partial Regex NativeDeclarationRegex();

    [GeneratedRegex(@"YLIWF_SKSE\.(\w+)\(")]
    private static partial Regex NativeCallRegex();

    [GeneratedRegex(@"(?:RegisterFunction\(|RegisterLatent<[^>]+>\(vm,\s*)""(\w+)""")]
    private static partial Regex NativeRegistrationRegex();
    [GeneratedRegex(@"(?m)^Function \w+\(")]
    private static partial Regex FacadeMethodsRegex();

    [GeneratedRegex(@"(?m)^\s*(?:While|If)\b")]
    private static partial Regex PapyrusControlFlowRegex();

    [GeneratedRegex(@"Bridge\(object, Phase::(?:Engine|Counts), ""(\w+)""")]
    private static partial Regex EngineAdapterRegex();

    [GeneratedRegex(@"constexpr std::array operations\{([^}]+)\}")]
    private static partial Regex DebugOperationsRegex();

    [GeneratedRegex(@"Operation::(\w+)")]
    private static partial Regex OperationNameRegex();



}
