using UnityEngine;
using UnityEditor;

[CustomEditor(typeof(PlayerSpawnPoint))]
public class PlayerSpawnPointEditor : Editor
{
    public override void OnInspectorGUI()
    {
        var sp = (PlayerSpawnPoint)target;
        serializedObject.Update();

        // 헤더
        var headerStyle = new GUIStyle(GUI.skin.box) { padding = new RectOffset(8, 8, 6, 6) };
        var oldColor = GUI.backgroundColor;
        GUI.backgroundColor = new Color(0.2f, 0.4f, 0.8f);
        EditorGUILayout.BeginVertical(headerStyle);
        GUI.backgroundColor = oldColor;
        EditorGUILayout.LabelField("Player Spawn Point", EditorStyles.whiteLargeLabel);
        EditorGUILayout.EndVertical();

        EditorGUILayout.Space(6);

        // 위치/회전 정보 표시 (읽기 전용 - Transform에서 가져옴)
        var boxStyle = new GUIStyle(GUI.skin.box) { padding = new RectOffset(8, 8, 6, 6) };
        GUI.backgroundColor = new Color(0.2f, 0.2f, 0.2f);
        EditorGUILayout.BeginVertical(boxStyle);
        GUI.backgroundColor = oldColor;

        EditorGUILayout.LabelField("스폰 정보 (Transform 기준)", EditorStyles.boldLabel);
        EditorGUILayout.Space(2);

        using (new EditorGUI.DisabledScope(true))
        {
            EditorGUILayout.Vector3Field("위치", sp.transform.position);
            EditorGUILayout.FloatField("방향 (Y 회전)", sp.transform.eulerAngles.y);
        }

        EditorGUILayout.EndVertical();

        EditorGUILayout.Space(4);

        EditorGUILayout.PropertyField(serializedObject.FindProperty("isDefault"),
            new GUIContent("기본 스폰", "씬당 하나만 true로 설정"));

        // 중복 경고
        var allSpawns = FindObjectsOfType<PlayerSpawnPoint>();
        if (allSpawns.Length > 1)
        {
            EditorGUILayout.Space(4);
            EditorGUILayout.HelpBox(
                $"씬에 PlayerSpawnPoint가 {allSpawns.Length}개 있습니다. 하나만 배치하세요.",
                MessageType.Warning
            );
        }

        serializedObject.ApplyModifiedProperties();
    }
}
