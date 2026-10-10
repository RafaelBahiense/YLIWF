using Mutagen.Bethesda.Skyrim;
using Mutagen.Bethesda.Strings;

internal static partial class FollowerPlugin
{
    private static void AddPerks(SkyrimMod mod)
    {
        mod.Perks.Add(CreateCompanionProtectionPerk());
    }

    private static Perk CreateCompanionProtectionPerk()
    {
        var companionProtectionPerk = new Perk(OwnForm(0x000802), SkyrimRelease.SkyrimSE)
        {
            EditorID = "YLIWF_CompanionsSafePerk",
            Description = new TranslatedString(Language.English, ""),
            NumRanks = (byte)(1),
            Playable = true
        };
        var spellMagnitudeEffect = new PerkEntryPointModifyValue();
        var spellMagnitudeSpellConditions = new PerkCondition
        {
            RunOnTabIndex = (byte)(1)
        };
        var hasKeywordCondition = new ConditionFloat
        {
            Flags = Condition.Flag.OR
        };
        var hasKeywordData = new HasKeywordConditionData();
        hasKeywordData.Keyword.Link.SetTo(SkyrimForm(0x046B99));
        hasKeywordCondition.Data = hasKeywordData;
        hasKeywordCondition.ComparisonValue = 1.0f;
        spellMagnitudeSpellConditions.Conditions.Add(hasKeywordCondition);
        var epMagicSpellHasSkillCondition = new ConditionFloat
        {
            Flags = Condition.Flag.OR
        };
        var epMagicSpellHasSkillData = new EPMagic_SpellHasSkillConditionData
        {
            ActorValue = ActorValue.Destruction
        };
        epMagicSpellHasSkillCondition.Data = epMagicSpellHasSkillData;
        epMagicSpellHasSkillCondition.ComparisonValue = 1.0f;
        spellMagnitudeSpellConditions.Conditions.Add(epMagicSpellHasSkillCondition);
        spellMagnitudeEffect.Conditions.Add(spellMagnitudeSpellConditions);
        var spellMagnitudeTargetConditions = new PerkCondition
        {
            RunOnTabIndex = (byte)(2)
        };
        var combatTargetCondition = new ConditionFloat();
        var combatTargetData = new IsCombatTargetConditionData();
        combatTargetData.TargetNpc.Link.SetTo(PlayerReferenceForm);
        combatTargetCondition.Data = combatTargetData;
        spellMagnitudeTargetConditions.Conditions.Add(combatTargetCondition);
        var inCombatCondition = new ConditionFloat();
        var inCombatData = new IsInCombatConditionData();
        inCombatCondition.Data = inCombatData;
        inCombatCondition.ComparisonValue = 1.0f;
        spellMagnitudeTargetConditions.Conditions.Add(inCombatCondition);
        var inFactionCondition = new ConditionFloat
        {
            Flags = Condition.Flag.OR
        };
        var inFactionData = new GetInFactionConditionData();
        inFactionData.Faction.Link.SetTo(CurrentFollowerFactionForm);
        inFactionCondition.Data = inFactionData;
        inFactionCondition.ComparisonValue = 1.0f;
        spellMagnitudeTargetConditions.Conditions.Add(inFactionCondition);
        var playerTeammateCondition = new ConditionFloat
        {
            Flags = Condition.Flag.OR
        };
        var playerTeammateData = new GetPlayerTeammateConditionData();
        playerTeammateCondition.Data = playerTeammateData;
        playerTeammateCondition.ComparisonValue = 1.0f;
        spellMagnitudeTargetConditions.Conditions.Add(playerTeammateCondition);
        spellMagnitudeEffect.Conditions.Add(spellMagnitudeTargetConditions);
        spellMagnitudeEffect.EntryPoint = APerkEntryPointEffect.EntryType.ModSpellMagnitude;
        spellMagnitudeEffect.PerkConditionTabCount = (byte)(3);
        spellMagnitudeEffect.Modification = PerkEntryPointModifyValue.ModificationType.Multiply;
        spellMagnitudeEffect.Value = 0.0f;
        companionProtectionPerk.Effects.Add(spellMagnitudeEffect);
        var attackDamageEffect = new PerkEntryPointModifyValue();
        var attackDamageTargetConditions = new PerkCondition
        {
            RunOnTabIndex = (byte)(2)
        };
        var combatTargetCondition2 = new ConditionFloat();
        var combatTargetData2 = new IsCombatTargetConditionData();
        combatTargetData2.TargetNpc.Link.SetTo(PlayerReferenceForm);
        combatTargetCondition2.Data = combatTargetData2;
        attackDamageTargetConditions.Conditions.Add(combatTargetCondition2);
        var inCombatCondition2 = new ConditionFloat();
        var inCombatData2 = new IsInCombatConditionData();
        inCombatCondition2.Data = inCombatData2;
        inCombatCondition2.ComparisonValue = 1.0f;
        attackDamageTargetConditions.Conditions.Add(inCombatCondition2);
        var inFactionCondition2 = new ConditionFloat
        {
            Flags = Condition.Flag.OR
        };
        var inFactionData2 = new GetInFactionConditionData();
        inFactionData2.Faction.Link.SetTo(CurrentFollowerFactionForm);
        inFactionCondition2.Data = inFactionData2;
        inFactionCondition2.ComparisonValue = 1.0f;
        attackDamageTargetConditions.Conditions.Add(inFactionCondition2);
        var playerTeammateCondition2 = new ConditionFloat
        {
            Flags = Condition.Flag.OR
        };
        var playerTeammateData2 = new GetPlayerTeammateConditionData();
        playerTeammateCondition2.Data = playerTeammateData2;
        playerTeammateCondition2.ComparisonValue = 1.0f;
        attackDamageTargetConditions.Conditions.Add(playerTeammateCondition2);
        attackDamageEffect.Conditions.Add(attackDamageTargetConditions);
        attackDamageEffect.EntryPoint = APerkEntryPointEffect.EntryType.ModAttackDamage;
        attackDamageEffect.PerkConditionTabCount = (byte)(3);
        attackDamageEffect.Modification = PerkEntryPointModifyValue.ModificationType.Multiply;
        attackDamageEffect.Value = 0.0f;
        companionProtectionPerk.Effects.Add(attackDamageEffect);
        return companionProtectionPerk;
    }
}
