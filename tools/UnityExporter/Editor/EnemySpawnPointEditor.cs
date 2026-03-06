using UnityEngine;
using UnityEditor;

[CustomEditor(typeof(EnemySpawnPoint))]
public class EnemySpawnPointEditor : Editor
{
    // 폴드아웃 상태 저장
    bool m_foldStats     = true;
    bool m_foldAttack    = true;
    bool m_foldIndicator = false;
    bool m_foldVisual    = false;
    bool m_foldAnim      = false;

    // 색상 팔레트
    static readonly Color ColorHeader    = new Color(0.25f, 0.45f, 0.65f);
    static readonly Color ColorWarning   = new Color(0.9f,  0.7f,  0.1f);
    static readonly Color ColorError     = new Color(0.9f,  0.3f,  0.3f);
    static readonly Color ColorOk        = new Color(0.3f,  0.75f, 0.4f);

    public override void OnInspectorGUI()
    {
        var sp = (EnemySpawnPoint)target;
        serializedObject.Update();

        // ─────────────────────────────────────────
        //  헤더
        // ─────────────────────────────────────────
        DrawColoredBox(ColorHeader, () =>
        {
            EditorGUILayout.LabelField("Enemy Spawn Point", EditorStyles.whiteLargeLabel);
        });
        EditorGUILayout.Space(4);

        // ─────────────────────────────────────────
        //  기본 정보
        // ─────────────────────────────────────────
        DrawColoredBox(new Color(0.2f, 0.2f, 0.2f), () =>
        {
            EditorGUILayout.LabelField("기본 정보", EditorStyles.boldLabel);
            EditorGUILayout.Space(2);

            EditorGUILayout.PropertyField(serializedObject.FindProperty("presetName"),
                new GUIContent("프리셋 이름", "C++ EnemyPreset.name 과 동일해야 함"));
            EditorGUILayout.PropertyField(serializedObject.FindProperty("count"),
                new GUIContent("스폰 수"));
            EditorGUILayout.PropertyField(serializedObject.FindProperty("activationDelay"),
                new GUIContent("스폰 딜레이 (초)"));

            // 프리셋 이름 미입력 경고
            if (string.IsNullOrWhiteSpace(sp.presetName))
            {
                DrawHelpBox("프리셋 이름을 입력하세요.", ColorError);
            }
        });
        EditorGUILayout.Space(4);

        // ─────────────────────────────────────────
        //  전투 스탯
        // ─────────────────────────────────────────
        m_foldStats = DrawFoldout(m_foldStats, "전투 스탯", ColorHeader, () =>
        {
            DrawStatRow("최대 체력",        "maxHP",          1f,   10000f, $"{sp.maxHP:F0}");
            DrawStatRow("이동 속도",        "moveSpeed",      0.1f, 30f,    $"{sp.moveSpeed:F1}");
            DrawStatRow("공격 범위",        "attackRange",    0.5f, 20f,    $"{sp.attackRange:F1}m");
            DrawStatRow("공격 쿨다운",      "attackCooldown", 0.1f, 10f,    $"{sp.attackCooldown:F1}s");
            DrawStatRow("감지 거리",        "detectionRange", 1f,   100f,   $"{sp.detectionRange:F1}m");

            // 공격 범위 > 감지 거리 경고
            if (sp.attackRange > sp.detectionRange)
                DrawHelpBox("공격 범위가 감지 거리보다 큽니다.", ColorWarning);
        });
        EditorGUILayout.Space(4);

        // ─────────────────────────────────────────
        //  공격 타입
        // ─────────────────────────────────────────
        m_foldAttack = DrawFoldout(m_foldAttack, "공격 타입", ColorHeader, () =>
        {
            EditorGUILayout.PropertyField(serializedObject.FindProperty("attackType"),
                new GUIContent("공격 타입"));

            // 공격 타입 설명
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

            bool isRush = sp.indicatorType == IndicatorType.RushCircle
                       || sp.indicatorType == IndicatorType.RushCone;
            bool hasHit = sp.indicatorType == IndicatorType.Circle
                       || sp.indicatorType == IndicatorType.RushCircle
                       || sp.indicatorType == IndicatorType.RushCone;
            bool hasCone = sp.indicatorType == IndicatorType.RushCone;

            if (sp.indicatorType == IndicatorType.None)
            {
                EditorGUILayout.HelpBox("인디케이터 없음 — 공격 예고 표시 안 함", MessageType.None);
                return;
            }

            EditorGUILayout.Space(4);

            if (isRush)
                DrawStatRow("돌진 거리", "rushDistance", 0f, 30f, $"{sp.rushDistance:F1}m");

            if (hasHit)
                DrawStatRow("피격 반경", "hitRadius", 0f, 20f, $"{sp.hitRadius:F1}m");

            if (hasCone)
                DrawStatRow("콘 각도", "coneAngle", 0f, 360f, $"{sp.coneAngle:F0}°");

            // 돌진 거리 미설정 경고
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
            {
                EditorGUILayout.HelpBox("애니메이션 경로가 비어있으면 클립 이름이 무시됩니다.", MessageType.Warning);
            }

            using var disabled = new EditorGUI.DisabledScope(!hasAnim);
            EditorGUILayout.PropertyField(serializedObject.FindProperty("clipIdle"),   new GUIContent("대기"));
            EditorGUILayout.PropertyField(serializedObject.FindProperty("clipChase"),  new GUIContent("추격"));
            EditorGUILayout.PropertyField(serializedObject.FindProperty("clipAttack"), new GUIContent("공격"));
            EditorGUILayout.PropertyField(serializedObject.FindProperty("clipStagger"),new GUIContent("경직"));
            EditorGUILayout.PropertyField(serializedObject.FindProperty("clipDeath"),  new GUIContent("사망"));
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
                $"attackType: {sp.attackType}  indicator: {sp.indicatorType}";

            EditorGUILayout.LabelField(preview, EditorStyles.helpBox);
        });

        serializedObject.ApplyModifiedProperties();
    }

    // ─────────────────────────────────────────
    //  유틸리티
    // ─────────────────────────────────────────

    static void DrawColoredBox(Color bgColor, System.Action content)
    {
        var style = new GUIStyle(GUI.skin.box)
        {
            padding = new RectOffset(8, 8, 6, 6)
        };
        var oldColor = GUI.backgroundColor;
        GUI.backgroundColor = bgColor;
        EditorGUILayout.BeginVertical(style);
        GUI.backgroundColor = oldColor;
        content?.Invoke();
        EditorGUILayout.EndVertical();
    }

    static bool DrawFoldout(bool expanded, string label, Color color, System.Action content)
    {
        var style = new GUIStyle(EditorStyles.foldoutHeader)
        {
            fontStyle = FontStyle.Bold
        };
        var oldColor = GUI.backgroundColor;
        GUI.backgroundColor = color * 0.8f;
        expanded = EditorGUILayout.BeginFoldoutHeaderGroup(expanded, label, style);
        GUI.backgroundColor = oldColor;

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
        var oldColor = GUI.backgroundColor;
        GUI.backgroundColor = color;
        EditorGUILayout.LabelField(msg, style);
        GUI.backgroundColor = oldColor;
    }
}
