using UnityEngine;
using UnityEditor;

[CustomEditor(typeof(EnemySpawnPoint))]
public class EnemySpawnPointEditor : Editor
{
    // 폴드아웃 상태 저장
    bool m_foldStats     = true;
    bool m_foldAttack    = true;
    bool m_foldIndicator = true;
    bool m_foldVisual    = true;
    bool m_foldAnim      = false;

    // 색상 팔레트
    static readonly Color ColorHeader  = new Color(0.25f, 0.45f, 0.65f);
    static readonly Color ColorWarning = new Color(0.9f,  0.7f,  0.1f);
    static readonly Color ColorError   = new Color(0.9f,  0.3f,  0.3f);

    // 이전 프레임 값 (변경 감지용)
    string    m_prevPresetName  = null;
    AttackType m_prevAttackType = (AttackType)(-1);

    // ─────────────────────────────────────────
    //  Elemental 프리셋 이름 목록 (하드코딩)
    // ─────────────────────────────────────────
    static readonly string[] k_presetNames =
    {
        "AirElemental_Bl",    "AirElemental_Cn",    "AirElemental_Gn",
        "AirElemental_Pe",    "AirElemental_Rd",    "AirElemental_Ye",
        "ChaosElemental_Bl",  "ChaosElemental_Gn",  "ChaosElemental_Or",
        "ChaosElemental_Pe",  "ChaosElemental_Rd",  "ChaosElemental_Ye",
        "EarthElemental_Bl",  "EarthElemental_Gn",  "EarthElemental_Or",
        "EarthElemental_Pe",  "EarthElemental_Ye",
        "FireGolem_Bl",       "FireGolem_Cn",       "FireGolem_Gn",
        "FireGolem_Pe",       "FireGolem_Rd",       "FireGolem_Ye",
        "HolyElemental_Bk",   "HolyElemental_Bl",   "HolyElemental_Cn",
        "HolyElemental_Gn",   "HolyElemental_Pe",   "HolyElemental_Ye",
        "IceElemental_Bl",    "IceElemental_Cn",    "IceElemental_Gn",
        "IceElemental_Pe",    "IceElemental_Rd",    "IceElemental_Ye",
        "LavaElemental_Bl",   "LavaElemental_Cn",   "LavaElemental_Gn",
        "LavaElemental_Pe",   "LavaElemental_Rd",   "LavaElemental_Ye",
        "LavaMan_Bl",         "LavaMan_Cn",         "LavaMan_Gn",
        "LavaMan_Pe",         "LavaMan_Rd",         "LavaMan_Ye",
        "MagicElemental_Bk",  "MagicElemental_Bl",  "MagicElemental_Cn",
        "MagicElemental_Gn",  "MagicElemental_Pe",  "MagicElemental_Rd",
        "MagmaElemental_Bl",  "MagmaElemental_Gn",  "MagmaElemental_Gr",
        "MagmaElemental_Pe",  "MagmaElemental_Rd",  "MagmaElemental_Ye",
        "MoltenElemental_Bl", "MoltenElemental_Gn", "MoltenElemental_Gr",
        "MoltenElemental_Or", "MoltenElemental_Pe", "MoltenElemental_Rd",
        "PrimalWaterElemental_Bl", "PrimalWaterElemental_Cn", "PrimalWaterElemental_Gn",
        "PrimalWaterElemental_Pe", "PrimalWaterElemental_Rd", "PrimalWaterElemental_Ye",
        "ShadowElemental_Bk", "ShadowElemental_Bl", "ShadowElemental_Cn",
        "ShadowElemental_Gn", "ShadowElemental_Pe", "ShadowElemental_Rd",
        "StoneElemental_Bk",  "StoneElemental_Bl",  "StoneElemental_Br",
        "StoneElemental_Gn",  "StoneElemental_Pe",  "StoneElemental_Rd",
        "StormElemental_Bl",  "StormElemental_Cn",  "StormElemental_Gn",
        "StormElemental_Pe",  "StormElemental_Rd",  "StormElemental_Ye",
        "WaterElemental_Bl",  "WaterElemental_Cn",  "WaterElemental_Gn",
        "WaterElemental_Gr",  "WaterElemental_Pe",  "WaterElemental_Ye",
    };

    // ─────────────────────────────────────────
    //  자동 설정 헬퍼
    // ─────────────────────────────────────────

    /// 프리셋 이름으로 meshPath / animationPath 자동 설정
    static void ApplyPresetPaths(SerializedObject so, string name)
    {
        string dir = $"Assets/Enemies/Elementals/{name}";
        so.FindProperty("meshPath").stringValue      = $"{dir}/{name}.bin";
        so.FindProperty("animationPath").stringValue = $"{dir}/{name}_Anim.bin";
        so.FindProperty("color").colorValue          = Color.white;
    }

    /// 공격 타입에 따라 indicatorType 및 기본값 자동 설정
    static void ApplyIndicatorDefaults(SerializedObject so, AttackType type, float attackRange)
    {
        var indicatorProp     = so.FindProperty("indicatorType");
        var rushDistanceProp  = so.FindProperty("rushDistance");
        var hitRadiusProp     = so.FindProperty("hitRadius");
        var coneAngleProp     = so.FindProperty("coneAngle");

        switch (type)
        {
            case AttackType.Melee:
                indicatorProp.enumValueIndex = (int)IndicatorType.Circle;
                hitRadiusProp.floatValue     = Mathf.Max(1f, attackRange);
                break;
            case AttackType.RushFront:
                indicatorProp.enumValueIndex = (int)IndicatorType.RushCone;
                rushDistanceProp.floatValue  = 8f;
                hitRadiusProp.floatValue     = 2f;
                coneAngleProp.floatValue     = 60f;
                break;
            case AttackType.RushAoE:
                indicatorProp.enumValueIndex = (int)IndicatorType.RushCircle;
                rushDistanceProp.floatValue  = 8f;
                hitRadiusProp.floatValue     = 3f;
                break;
            case AttackType.Ranged:
                indicatorProp.enumValueIndex = (int)IndicatorType.None;
                break;
        }
    }

    // ─────────────────────────────────────────
    //  인스펙터 GUI
    // ─────────────────────────────────────────

    public override void OnInspectorGUI()
    {
        var sp = (EnemySpawnPoint)target;
        serializedObject.Update();

        // 첫 로드 시 이전 값 초기화 (초기화 시에는 자동 설정 안 함)
        if (m_prevPresetName == null)  m_prevPresetName  = sp.presetName;
        if ((int)m_prevAttackType < 0) m_prevAttackType  = sp.attackType;

        // ─────────────────────────────────────────
        //  헤더
        // ─────────────────────────────────────────
        DrawColoredBox(ColorHeader, () =>
        {
            EditorGUILayout.BeginHorizontal();
            EditorGUILayout.LabelField("Enemy Spawn Point", EditorStyles.whiteLargeLabel);
            if (GUILayout.Button("재설정", GUILayout.Width(54), GUILayout.Height(20)))
            {
                ApplyPresetPaths(serializedObject, sp.presetName);
                ApplyIndicatorDefaults(serializedObject, sp.attackType, sp.attackRange);
                serializedObject.ApplyModifiedProperties();
                m_prevPresetName = sp.presetName;
                m_prevAttackType = sp.attackType;
            }
            EditorGUILayout.EndHorizontal();
        });
        EditorGUILayout.Space(4);

        // ─────────────────────────────────────────
        //  기본 정보
        // ─────────────────────────────────────────
        DrawColoredBox(new Color(0.2f, 0.2f, 0.2f), () =>
        {
            EditorGUILayout.LabelField("기본 정보", EditorStyles.boldLabel);
            EditorGUILayout.Space(2);

            // ── 프리셋 이름 드롭다운 ──────────────────────────
            var presetProp = serializedObject.FindProperty("presetName");
            int curIdx = System.Array.IndexOf(k_presetNames, sp.presetName);
            bool knownPreset = curIdx >= 0;
            if (!knownPreset) curIdx = 0;

            int newIdx = EditorGUILayout.Popup(
                new GUIContent("프리셋 이름", "Assets/Enemies/Elementals 하위 폴더 이름"),
                curIdx, k_presetNames);

            bool presetChanged = newIdx != curIdx || !knownPreset;
            if (presetChanged)
            {
                presetProp.stringValue = k_presetNames[newIdx];
                ApplyPresetPaths(serializedObject, k_presetNames[newIdx]);
                ApplyIndicatorDefaults(serializedObject, sp.attackType, sp.attackRange);
                serializedObject.ApplyModifiedProperties();
                m_prevPresetName = k_presetNames[newIdx];
            }

            if (!knownPreset)
                DrawHelpBox($"기존 값 \"{sp.presetName}\"이 목록에 없어 첫 항목으로 초기화됩니다.", ColorWarning);

            EditorGUILayout.PropertyField(serializedObject.FindProperty("count"),
                new GUIContent("스폰 수"));
            EditorGUILayout.PropertyField(serializedObject.FindProperty("activationDelay"),
                new GUIContent("스폰 딜레이 (초)"));
        });
        EditorGUILayout.Space(4);

        // ─────────────────────────────────────────
        //  전투 스탯
        // ─────────────────────────────────────────
        m_foldStats = DrawFoldout(m_foldStats, "전투 스탯", ColorHeader, () =>
        {
            DrawStatRow("최대 체력",   "maxHP",          1f,   10000f, $"{sp.maxHP:F0}");
            DrawStatRow("이동 속도",   "moveSpeed",      0.1f, 30f,    $"{sp.moveSpeed:F1}");
            DrawStatRow("공격 범위",   "attackRange",    0.5f, 20f,    $"{sp.attackRange:F1}m");
            DrawStatRow("공격 쿨다운", "attackCooldown", 0.1f, 10f,    $"{sp.attackCooldown:F1}s");
            DrawStatRow("감지 거리",   "detectionRange", 1f,   100f,   $"{sp.detectionRange:F1}m");

            if (sp.attackRange > sp.detectionRange)
                DrawHelpBox("공격 범위가 감지 거리보다 큽니다.", ColorWarning);
        });
        EditorGUILayout.Space(4);

        // ─────────────────────────────────────────
        //  공격 타입
        // ─────────────────────────────────────────
        m_foldAttack = DrawFoldout(m_foldAttack, "공격 타입", ColorHeader, () =>
        {
            EditorGUI.BeginChangeCheck();
            EditorGUILayout.PropertyField(serializedObject.FindProperty("attackType"),
                new GUIContent("공격 타입"));
            if (EditorGUI.EndChangeCheck())
            {
                serializedObject.ApplyModifiedProperties();
                ApplyIndicatorDefaults(serializedObject, sp.attackType, sp.attackRange);
                serializedObject.ApplyModifiedProperties();
                m_prevAttackType = sp.attackType;
            }

            string desc = sp.attackType switch
            {
                AttackType.Melee     => "근접 공격 — 적이 범위 안에 들어오면 즉시 타격",
                AttackType.RushFront => "돌진 전방 공격 — 직선으로 돌진 후 전방 콘 타격",
                AttackType.RushAoE   => "돌진 광역 공격 — 돌진 후 목표 위치 주변 원형 타격",
                AttackType.Ranged    => "원거리 공격 — 투사체 발사",
                _                    => ""
            };
            if (!string.IsNullOrEmpty(desc))
                EditorGUILayout.HelpBox(desc, MessageType.Info);
        });
        EditorGUILayout.Space(4);

        // ─────────────────────────────────────────
        //  공격 인디케이터
        // ─────────────────────────────────────────
        m_foldIndicator = DrawFoldout(m_foldIndicator, "공격 인디케이터", ColorHeader, () =>
        {
            EditorGUILayout.PropertyField(serializedObject.FindProperty("indicatorType"),
                new GUIContent("인디케이터 타입"));

            bool isRush  = sp.indicatorType == IndicatorType.RushCircle
                        || sp.indicatorType == IndicatorType.RushCone;
            bool hasHit  = sp.indicatorType == IndicatorType.Circle
                        || sp.indicatorType == IndicatorType.RushCircle
                        || sp.indicatorType == IndicatorType.RushCone;
            bool hasCone = sp.indicatorType == IndicatorType.RushCone;

            if (sp.indicatorType == IndicatorType.None)
            {
                EditorGUILayout.HelpBox("인디케이터 없음 — 공격 예고 표시 안 함", MessageType.None);
                return;
            }

            EditorGUILayout.Space(4);
            if (isRush)  DrawStatRow("돌진 거리", "rushDistance", 0f, 30f,  $"{sp.rushDistance:F1}m");
            if (hasHit)  DrawStatRow("피격 반경", "hitRadius",    0f, 20f,  $"{sp.hitRadius:F1}m");
            if (hasCone) DrawStatRow("콘 각도",   "coneAngle",    0f, 360f, $"{sp.coneAngle:F0}°");

            if (isRush && sp.rushDistance <= 0f)
                DrawHelpBox("돌진 타입에는 돌진 거리를 설정해야 합니다.", ColorWarning);
        });
        EditorGUILayout.Space(4);

        // ─────────────────────────────────────────
        //  시각적 설정
        // ─────────────────────────────────────────
        m_foldVisual = DrawFoldout(m_foldVisual, "시각적 설정", ColorHeader, () =>
        {
            EditorGUILayout.PropertyField(serializedObject.FindProperty("meshPath"),
                new GUIContent("메시 경로", "비어있으면 큐브 사용"));
            EditorGUILayout.PropertyField(serializedObject.FindProperty("animationPath"),
                new GUIContent("애니메이션 경로"));
            EditorGUILayout.PropertyField(serializedObject.FindProperty("scale"),
                new GUIContent("스케일"));
            EditorGUILayout.PropertyField(serializedObject.FindProperty("color"),
                new GUIContent("색상"));
        });
        EditorGUILayout.Space(4);

        // ─────────────────────────────────────────
        //  애니메이션 클립
        // ─────────────────────────────────────────
        m_foldAnim = DrawFoldout(m_foldAnim, "애니메이션 클립", ColorHeader, () =>
        {
            bool hasAnim = !string.IsNullOrWhiteSpace(sp.animationPath);
            if (!hasAnim)
                EditorGUILayout.HelpBox("애니메이션 경로가 비어있으면 클립 이름이 무시됩니다.", MessageType.Warning);

            using var disabled = new EditorGUI.DisabledScope(!hasAnim);
            EditorGUILayout.PropertyField(serializedObject.FindProperty("clipIdle"),    new GUIContent("대기"));
            EditorGUILayout.PropertyField(serializedObject.FindProperty("clipChase"),   new GUIContent("추격"));
            EditorGUILayout.PropertyField(serializedObject.FindProperty("clipAttack"),  new GUIContent("공격"));
            EditorGUILayout.PropertyField(serializedObject.FindProperty("clipStagger"), new GUIContent("경직"));
            EditorGUILayout.PropertyField(serializedObject.FindProperty("clipDeath"),   new GUIContent("사망"));
        });
        EditorGUILayout.Space(4);

        // ─────────────────────────────────────────
        //  JSON 미리보기
        // ─────────────────────────────────────────
        DrawColoredBox(new Color(0.15f, 0.25f, 0.15f), () =>
        {
            EditorGUILayout.LabelField("JSON 미리보기", EditorStyles.boldLabel);
            EditorGUILayout.Space(2);

            string preview =
                $"presetName: \"{sp.presetName}\"\n" +
                $"count: {sp.count},  delay: {sp.activationDelay:F1}s\n" +
                $"HP: {sp.maxHP:F0}  SPD: {sp.moveSpeed:F1}  ATK_R: {sp.attackRange:F1}\n" +
                $"ATK_CD: {sp.attackCooldown:F1}s  DET: {sp.detectionRange:F1}m\n" +
                $"attackType: {sp.attackType}  indicator: {sp.indicatorType}\n" +
                $"mesh: {sp.meshPath}";

            EditorGUILayout.LabelField(preview, EditorStyles.helpBox);
        });

        serializedObject.ApplyModifiedProperties();
    }

    // ─────────────────────────────────────────
    //  유틸리티
    // ─────────────────────────────────────────

    static void DrawColoredBox(Color bgColor, System.Action content)
    {
        var style = new GUIStyle(GUI.skin.box) { padding = new RectOffset(8, 8, 6, 6) };
        var old = GUI.backgroundColor;
        GUI.backgroundColor = bgColor;
        EditorGUILayout.BeginVertical(style);
        GUI.backgroundColor = old;
        content?.Invoke();
        EditorGUILayout.EndVertical();
    }

    static bool DrawFoldout(bool expanded, string label, Color color, System.Action content)
    {
        var style = new GUIStyle(EditorStyles.foldoutHeader) { fontStyle = FontStyle.Bold };
        var old = GUI.backgroundColor;
        GUI.backgroundColor = color * 0.8f;
        expanded = EditorGUILayout.BeginFoldoutHeaderGroup(expanded, label, style);
        GUI.backgroundColor = old;
        if (expanded)
        {
            EditorGUI.indentLevel++;
            EditorGUILayout.Space(2);
            content?.Invoke();
            EditorGUI.indentLevel--;
        }
        EditorGUILayout.EndFoldoutHeaderGroup();
        return expanded;
    }

    void DrawStatRow(string label, string propName, float min, float max, string valueDisplay)
    {
        var prop = serializedObject.FindProperty(propName);
        EditorGUILayout.BeginHorizontal();
        EditorGUILayout.LabelField(label, GUILayout.Width(90));
        EditorGUILayout.Slider(prop, min, max, GUIContent.none);
        EditorGUILayout.LabelField(valueDisplay, GUILayout.Width(60));
        EditorGUILayout.EndHorizontal();
    }

    static void DrawHelpBox(string msg, Color color)
    {
        var style = new GUIStyle(EditorStyles.helpBox);
        var old = GUI.backgroundColor;
        GUI.backgroundColor = color;
        EditorGUILayout.LabelField(msg, style);
        GUI.backgroundColor = old;
    }
}
