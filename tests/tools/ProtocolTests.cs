using System.Text.RegularExpressions;

internal static partial class ProtocolTests
{
    // Order is part of the saved controller format.
    private static readonly string[] ExpectedOperations =
    [
        "Sync",
        "Recruit",
        "Follow",
        "Wait",
        "Dismiss",
        "Death",
        "Timeout",
        "Unload",
        "ReservedPrepare",
        "ReservedCleanup",
        "ReservedPromote",
        "ReservedPop",
        "AnimalRecruit",
        "AnimalWait",
        "AnimalFollow",
        "AnimalDismiss",
        "Repair",
        "ClearSlot",
        "Adopt",
        "Release",
        "ReservedAddExtra",
        "ReservedClearDead",
        "HomeAssign",
        "HomeRemove",
        "DialogueFollow",
        "DialogueWait",
        "DialogueDismiss",
        "PromotePrimary"
    ];

    private static readonly string[] ExpectedEffects =
    [
        "CancelTimer",
        "Clear",
        "Assign",
        "Teammate",
        "Prepare",
        "Release",
        "Protection",
        "Waiting",
        "Evaluate",
        "ReservedUpdate",
        "Relationship",
        "StopCombat",
        "Cleanup",
        "Message",
        "Hireling",
        "DismissLine",
        "EndDismissLine",
        "StartTimer",
        "HideObjective",
        "Speaker",
        "ClearSpeaker",
        "AnimalPrepare",
        "AnimalCount",
        "HomeClear",
        "HomeMove",
        "HomeAssign",
        "HomeFaction",
        "HomeEvaluate",
        "SwapPrimary"
    ];

    private static readonly string[] ExpectedDebugOperations =
    [
        "Sync", "Repair", "Follow", "Wait", "Dismiss", "ClearSlot", "Adopt", "Release", "PromotePrimary"
    ];

    public static void Run(string root)
    {
        var rules = ReadSource(root, "include/ControllerRules.h");
        var facade = ReadSource(root, "src/papyrus/core/DialogueFollowerScript.psc");
        var executor = ReadSource(root, "src/native/ControllerExecutor.cpp");
        var controller = ReadSource(root, "src/native/Controller.cpp");

        CheckSavedEnumNumbers(rules);
        CheckVanillaFacade(facade);
        CheckNativeExports(root);
        CheckNativeExecution(rules, executor, controller);
        CheckEngineAdapters(executor, ReadSource(root, "src/papyrus/core/YLIWF_Engine.psc"));
        CheckRemovedScripts(root);
        CheckEventForwarding(ReadSource(root, "src/native/plugin.cpp"));
        CheckDebugCommands(executor, ReadSource(root, "src/native/Debug.cpp"));
        CheckHomeForwarding(ReadSource(root, "src/papyrus/core/YLIWF_HomeTopicScript.psc"));
    }

    private static void CheckSavedEnumNumbers(string rules)
    {
        CheckEnum(rules, "Operation", ExpectedOperations, first: 0);
        CheckEnum(rules, "Effect", ExpectedEffects, first: 1);
    }

    private static void CheckEnum(string rules, string name, string[] expectedNames, int first)
    {
        var body = Regex.Match(rules, @"enum class " + name + @"\s*:\s*std::int32_t\s*\{([^}]+)\}")
            .Groups[1].Value;
        var entries = body.Split(',', StringSplitOptions.TrimEntries | StringSplitOptions.RemoveEmptyEntries);
        Require(entries.Length == expectedNames.Length, name + " entries changed; review saved-format compatibility");

        for (var index = 0; index < expectedNames.Length; ++index)
        {
            var expected = @"^" + expectedNames[index] + @"\s*=\s*" + (first + index) + @"$";
            Require(Regex.IsMatch(entries[index], expected), expectedNames[index] + " saved number changed");
        }
    }

    private static void CheckVanillaFacade(string script)
    {
        string[] methods =
        [
            "SetFollower", "SetAnimal", "FollowerWait", "AnimalWait",
            "FollowerFollow", "AnimalFollow", "DismissFollower", "DismissAnimal"
        ];
        Require(FacadeMethodsRegex().Count(script) == methods.Length,
            "vanilla facade must contain exactly eight methods");
        Require(script.Contains("extends Quest Conditional", StringComparison.Ordinal),
            "vanilla facade still inherits internal storage");

        foreach (var method in methods)
        {
            var body = FunctionBody(script, method).Trim();
            Require(body.StartsWith("YLIWF_SKSE." + method + "(", StringComparison.Ordinal) && !body.Contains('\n'),
                method + " is not a single native forwarding call");
        }

        Require(!script.Contains("Controller", StringComparison.Ordinal) &&
            !script.Contains("NativeWait", StringComparison.Ordinal),
            "execution fields leaked into the facade");
        Require(!PapyrusControlFlowRegex().IsMatch(script) && !script.Contains("Utility.Wait", StringComparison.Ordinal),
            "controller policy/executor moved back into Papyrus");
    }

    private static void CheckNativeExports(string root)
    {
        var api = ReadSource(root, "src/papyrus/core/YLIWF_SKSE.psc");
        var declarations = NativeDeclarationRegex().Matches(api).Select(match => match.Groups[1].Value).ToArray();
        var declared = declarations.ToHashSet(StringComparer.OrdinalIgnoreCase);
        Require(declared.Count == declarations.Length, "duplicate native declarations");

        var callers = Directory.EnumerateFiles(Path.Combine(root, "src/papyrus"), "*.psc", SearchOption.AllDirectories)
            .SelectMany(path => NativeCallRegex().Matches(File.ReadAllText(path)))
            .Select(match => match.Groups[1].Value).ToHashSet(StringComparer.OrdinalIgnoreCase);
        Require(callers.IsSubsetOf(declared), "a Papyrus caller references a removed native export");

        callers.Add("DebugInt"); // Retain the typed integer diagnostic helper.
        Require(declared.SetEquals(callers), "unused native export added without a caller");

        var registrations = Directory.EnumerateFiles(Path.Combine(root, "src/native"), "*.cpp")
            .SelectMany(path => NativeRegistrationRegex().Matches(File.ReadAllText(path)))
            .Select(match => match.Groups[1].Value).ToArray();
        Require(registrations.Length == declared.Count && declared.SetEquals(registrations),
            "native declarations and VM registrations differ");
    }

    private static void CheckNativeExecution(string rules, string executor, string controller)
    {
        foreach (var effect in ExpectedEffects)
        {
            Require((executor + controller).Contains("case Effect::" + effect + ":", StringComparison.Ordinal),
                effect + " has no native execution path");
        }
        Require(!controller.Contains("DispatchMethodCall(object, \"DrainQueue\"", StringComparison.Ordinal),
            "native queue still starts a Papyrus worker");
        Require(!rules.Contains("plan.Add(Effect::ReservedUpdate", StringComparison.Ordinal),
            "planner emits a retired count-update effect");
    }

    private static void CheckEngineAdapters(string executor, string bridge)
    {
        Require(!FunctionBody(bridge, "Counts").Contains("vanilla.SetValue", StringComparison.Ordinal),
            "delayed Papyrus counts can overwrite the native recruitment dialogue gate");
        Require(string.IsNullOrEmpty(FunctionBody(bridge, "CancelTimer")),
            "native timers still have a Papyrus cleanup adapter");

        foreach (Match call in EngineAdapterRegex().Matches(executor))
        {
            var body = FunctionBody(bridge, call.Groups[1].Value);
            Require(body.Contains("SetNativeEffectCurrent", StringComparison.Ordinal) &&
                body.Contains("SetNativeEffectReturned", StringComparison.Ordinal),
                "engine adapter lacks saved completion/stale-call guards");
        }
    }

    private static void CheckRemovedScripts(string root)
    {
        Require(!File.Exists(Path.Combine(root, "src/papyrus/core/YLIWF_State.psc")),
            "native state still has a Papyrus container");
        foreach (var removed in new[] { "FollowerAliasScript", "YLIWF_FollowerAliasScript", "YLIWF_Log" })
        {
            Require(!File.Exists(Path.Combine(root, "src/papyrus/core", removed + ".psc")),
                "redundant script remains: " + removed);
        }
    }

    private static void CheckEventForwarding(string events)
    {
        foreach (var name in new[] { "Activated", "Died", "CombatChanged", "Unloaded" })
        {
            Require(events.Contains("controller::" + name + "(", StringComparison.Ordinal),
                "native event forwarding missing: " + name);
        }
    }

    private static void CheckDebugCommands(string executor, string ui)
    {
        Require(ui.Contains("controller::SubmitDebug(", StringComparison.Ordinal) &&
            !ui.Contains("DispatchMethodCall(object, \"DebugCommand\"", StringComparison.Ordinal),
            "UI still depends on a Papyrus controller method");

        var debugTable = DebugOperationsRegex().Match(executor).Groups[1].Value;
        var operations = OperationNameRegex().Matches(debugTable).Select(match => match.Groups[1].Value);
        Require(operations.SequenceEqual(ExpectedDebugOperations), "debug commands send different operations");
    }

    private static void CheckHomeForwarding(string home)
    {
        Require(FunctionBody(home, "Fragment_0").Trim() ==
            "YLIWF_SKSE.SetHome(akSpeakerRef as Actor, HomeAction, HomeQuest, HomeFaction)",
            "home fragment contains controller policy");
    }

    private static string ReadSource(string root, string path) => File.ReadAllText(Path.Combine(root, path));

    private static string FunctionBody(string text, string name) => Regex.Match(text,
        @"(?m)^(?:\w+(?:\[\])? )?Function " + name + @"\([^\r\n]*\)(?: Global)?\r?\n([\s\S]*?)^EndFunction")
        .Groups[1].Value;

    private static void Require(bool condition, string message)
    {
        if (!condition)
        {
            throw new InvalidDataException("Controller ABI: " + message);
        }
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
