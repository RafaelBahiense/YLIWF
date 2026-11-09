using Mutagen.Bethesda.Skyrim;

internal static partial class FollowerPlugin
{
    private static void AddCombatStyles(SkyrimMod mod)
    {
        mod.CombatStyles.Add(DefenderStyle(0xA17, "YLIWF_ShieldDefenderStyle", guarding: false, recovering: false));
        mod.CombatStyles.Add(DefenderStyle(0xA18, "YLIWF_ShieldDefenderGuardStyle", guarding: true, recovering: false));
        mod.CombatStyles.Add(DefenderStyle(0xA19, "YLIWF_ShieldDefenderRecoveryStyle", guarding: false, recovering: true));
        mod.CombatStyles.Add(DefenderStyle(0xA1A, "YLIWF_ShieldDefenderGuardRecoveryStyle", guarding: true, recovering: true));
        mod.CombatStyles.Add(DefenderStyle(0xA1B, "YLIWF_ShieldDefenderCounterStyle", guarding: false, recovering: false, counter: true));
        mod.CombatStyles.Add(DefenderStyle(0xA1C, "YLIWF_ShieldDefenderInterceptStyle", guarding: false, recovering: false, intercept: true));
    }

    private static CombatStyle DefenderStyle(uint id, string editorId, bool guarding, bool recovering, bool counter = false, bool intercept = false)
    {
        // Combat styles bias engine decisions; they do not force individual animations.
        return new CombatStyle(OwnForm(id), SkyrimRelease.SkyrimSE)
        {
            EditorID = editorId,
            Flags = (CombatStyle.Flag)0,
            OffensiveMult = recovering ? 0 : guarding ? 0.1f : intercept ? 1.0f : counter ? 0.85f : 0.4f,
            DefensiveMult = guarding ? 1.0f : intercept ? 0.3f : 0.7f,
            GroupOffensiveMult = intercept ? 1.0f : 0.75f,
            EquipmentScoreMultMelee = 1,
            EquipmentScoreMultShout = recovering ? 0 : 0.5f,
            AvoidThreatChance = guarding || intercept ? 0 : recovering ? 0.1f : 0.05f,
            Melee = new CombatStyleMelee
            {
                AttackStaggeredMult = recovering ? 0 : 1,
                PowerAttackStaggeredMult = recovering || guarding ? 0 : counter ? 0.8f : 0.2f,
                PowerAttackBlockingMult = recovering || guarding ? 0 : 0.1f,
                BashMult = recovering ? 0 : counter ? 0.2f : guarding ? 0.65f : 0.45f,
                BashRecoilMult = recovering ? 0 : 0.2f,
                BashAttackMult = recovering ? 0 : guarding ? 0.6f : 0.3f,
                BashPowerAttackMult = recovering ? 0 : guarding ? 0.9f : 0.5f,
                SpecialAttackMult = recovering || guarding ? 0 : 0.25f
            },
            CloseRange = new CombatStyleCloseRange
            {
                CircleMult = intercept ? 0 : guarding ? 0.05f : 0.15f,
                FallbackMult = intercept ? 0 : guarding ? 0.05f : recovering ? 0.35f : 0.15f,
                FlankDistance = 1,
                StalkTime = 1
            }
        };
    }
}
