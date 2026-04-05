using System.Collections.Generic;
using System.IO;
using System.Text;
using UnityEngine;
using UnityEditor;
using UnityEngine.AI;

/// <summary>
/// 씬의 맵 데이터를 JSON + OBJ 파일로 익스포트하는 에디터 툴.
/// Unity 메뉴: Tools > Map Exporter
///
/// 태그 규칙:
///   Room     - 방 경계 오브젝트
///   Obstacle - 충돌 처리할 벽/장애물 (Collider 필수)
///   MapMesh  - 렌더링용 맵 오브젝트 (MeshFilter 필수) → OBJ 파일로 추출
/// </summary>
public class MapExporter : EditorWindow
{
    // ─────────────────────────────────────────
    //  설정 필드
    // ─────────────────────────────────────────
    string m_outputPath    = "Assets/MapData/map.json";
    string m_meshSubfolder = "meshes";

    bool m_exportPlayer    = true;
    bool m_exportRooms     = true;
    bool m_exportEnemies   = true;
    bool m_exportObstacles = true;
    bool m_exportMeshes    = true;
    bool m_exportNavMesh   = true;
    bool m_prettyPrint     = true;

    // Room-per-file export
    string m_roomsOutputDir = "Assets/MapData";

    Vector2 m_scrollPos;
    string  m_lastLog = "";

    // ─────────────────────────────────────────
    //  메뉴 등록
    // ─────────────────────────────────────────
    [MenuItem("Tools/Map Exporter")]
    static void Open()
    {
        var win = GetWindow<MapExporter>("Map Exporter");
        win.minSize = new Vector2(400, 560);
        win.Show();
    }

    // ─────────────────────────────────────────
    //  GUI
    // ─────────────────────────────────────────
    void OnGUI()
    {
        m_scrollPos = EditorGUILayout.BeginScrollView(m_scrollPos);

        // ── 헤더 ──
        EditorGUILayout.LabelField("Map Exporter",
            new GUIStyle(EditorStyles.boldLabel) { fontSize = 14 });
        EditorGUILayout.LabelField("씬 데이터를 C++ 프로젝트용 JSON + OBJ로 익스포트합니다.",
            EditorStyles.miniLabel);
        EditorGUILayout.Space(8);

        // ── 출력 경로 ──
        EditorGUILayout.LabelField("출력 경로", EditorStyles.boldLabel);
        EditorGUILayout.BeginHorizontal();
        m_outputPath = EditorGUILayout.TextField(m_outputPath);
        if (GUILayout.Button("...", GUILayout.Width(30)))
        {
            string path = EditorUtility.SaveFilePanel("저장 위치 선택", "Assets/MapData", "map", "json");
            if (!string.IsNullOrEmpty(path))
                m_outputPath = "Assets" + path.Replace(Application.dataPath, "");
        }
        EditorGUILayout.EndHorizontal();
        EditorGUILayout.Space(8);

        // ── 익스포트 항목 ──
        EditorGUILayout.LabelField("익스포트 항목", EditorStyles.boldLabel);
        m_exportPlayer    = EditorGUILayout.Toggle("플레이어 스폰", m_exportPlayer);
        m_exportRooms     = EditorGUILayout.Toggle("방 구조 (Room 태그)", m_exportRooms);
        m_exportEnemies   = EditorGUILayout.Toggle("적 스폰 포인트", m_exportEnemies);
        m_exportObstacles = EditorGUILayout.Toggle("조형물 포함 (Obstacle 태그, 충돌 없음)", m_exportObstacles);

        // 메시 익스포트 + 서브폴더 설정
        m_exportMeshes = EditorGUILayout.Toggle("맵 메시 OBJ (MapMesh 태그)", m_exportMeshes);
        if (m_exportMeshes)
        {
            EditorGUI.indentLevel++;
            m_meshSubfolder = EditorGUILayout.TextField("메시 서브폴더", m_meshSubfolder);
            EditorGUILayout.HelpBox(
                "MeshFilter가 있는 MapMesh 태그 오브젝트를 OBJ 파일로 추출합니다.\n" +
                "같은 메시를 공유하는 오브젝트는 OBJ를 하나만 생성하고 참조합니다.",
                MessageType.None);
            EditorGUI.indentLevel--;
        }

        m_exportNavMesh = EditorGUILayout.Toggle("NavMesh 삼각형", m_exportNavMesh);
        EditorGUILayout.Space(4);
        m_prettyPrint   = EditorGUILayout.Toggle("들여쓰기 (Pretty Print)", m_prettyPrint);
        EditorGUILayout.Space(8);

        // ── 씬 현황 요약 ──
        DrawSceneSummary();
        EditorGUILayout.Space(8);

        // ── 단일 파일 익스포트 버튼 ──
        GUI.backgroundColor = new Color(0.3f, 0.7f, 0.4f);
        if (GUILayout.Button("Export (단일 map.json)", GUILayout.Height(32)))
            Export();
        GUI.backgroundColor = Color.white;

        EditorGUILayout.Space(6);

        // ── 룸별 분리 익스포트 ──
        EditorGUILayout.LabelField("룸별 분리 익스포트", EditorStyles.boldLabel);
        EditorGUILayout.BeginHorizontal();
        m_roomsOutputDir = EditorGUILayout.TextField("출력 폴더", m_roomsOutputDir);
        if (GUILayout.Button("...", GUILayout.Width(30)))
        {
            string path = EditorUtility.SaveFolderPanel("룸 파일 저장 위치", "Assets/MapData", "");
            if (!string.IsNullOrEmpty(path))
                m_roomsOutputDir = "Assets" + path.Replace(Application.dataPath, "");
        }
        EditorGUILayout.EndHorizontal();
        EditorGUILayout.HelpBox(
            "Room 태그 오브젝트마다 별도 JSON 파일로 추출합니다.\n" +
            "각 파일에는 해당 Room의 자식 MapMesh/Obstacle/EnemySpawnPoint가 포함됩니다.\n" +
            "rooms.json 매니페스트를 생성해 C++ 프로젝트가 자동으로 인식합니다.",
            MessageType.None);
        GUI.backgroundColor = new Color(0.3f, 0.5f, 0.9f);
        if (GUILayout.Button("Export Rooms (룸별 분리)", GUILayout.Height(36)))
            ExportRooms();
        GUI.backgroundColor = Color.white;

        // ── 결과 로그 ──
        if (!string.IsNullOrEmpty(m_lastLog))
        {
            EditorGUILayout.Space(8);
            EditorGUILayout.LabelField("결과", EditorStyles.boldLabel);
            EditorGUILayout.HelpBox(m_lastLog, MessageType.Info);
        }

        EditorGUILayout.EndScrollView();
    }

    void DrawSceneSummary()
    {
        EditorGUILayout.LabelField("씬 현황", EditorStyles.boldLabel);
        EditorGUILayout.BeginVertical(new GUIStyle(EditorStyles.helpBox));

        int roomCount     = SafeFindWithTag("Room").Length;
        int enemyCount    = FindObjectsOfType<EnemySpawnPoint>().Length;
        int playerCount   = FindObjectsOfType<PlayerSpawnPoint>().Length;
        int obstacleCount = SafeFindWithTag("Obstacle").Length;
        int meshObjCount  = SafeFindWithTag("MapMesh").Length;

        EditorGUILayout.LabelField($"PlayerSpawnPoint:   {playerCount}개");
        EditorGUILayout.LabelField($"Room 태그:          {roomCount}개" +
            (roomCount == 0 ? "  (태그 미등록)" : ""));
        EditorGUILayout.LabelField($"EnemySpawnPoint:    {enemyCount}개");
        EditorGUILayout.LabelField($"Obstacle 태그:      {obstacleCount}개" +
            (obstacleCount == 0 ? "  (태그 미등록)" : ""));
        EditorGUILayout.LabelField($"MapMesh 태그:       {meshObjCount}개" +
            (meshObjCount == 0 ? "  (태그 미등록)" : ""));

        if (m_exportMeshes && meshObjCount > 0)
        {
            int uniqueMeshCount = 0;
            var seen = new HashSet<int>();
            foreach (var go in SafeFindWithTag("MapMesh"))
            {
                var mf = go.GetComponent<MeshFilter>();
                if (mf != null && mf.sharedMesh != null && seen.Add(mf.sharedMesh.GetInstanceID()))
                    uniqueMeshCount++;
            }
            EditorGUILayout.LabelField($"  └ 고유 메시: {uniqueMeshCount}개 → OBJ {uniqueMeshCount}개");
        }

        if (m_exportNavMesh)
        {
            try
            {
                var tri = NavMesh.CalculateTriangulation();
                EditorGUILayout.LabelField($"NavMesh 정점: {tri.vertices.Length} / 삼각형: {tri.indices.Length / 3}");
            }
            catch
            {
                EditorGUILayout.LabelField("NavMesh: 베이크 없음");
            }
        }

        EditorGUILayout.Space(2);
        if (playerCount == 0)
            EditorGUILayout.HelpBox("PlayerSpawnPoint가 없습니다.", MessageType.Warning);
        if (playerCount > 1)
            EditorGUILayout.HelpBox($"PlayerSpawnPoint {playerCount}개 → 첫 번째(isDefault)만 사용됩니다.", MessageType.Warning);

        // 미등록 태그 안내
        if (roomCount == 0 || obstacleCount == 0 || meshObjCount == 0)
            EditorGUILayout.HelpBox(
                "태그 미등록 항목은 Edit > Project Settings > Tags and Layers 에서 추가하세요.\n" +
                "필요한 태그: Room, Obstacle, MapMesh",
                MessageType.Info);

        EditorGUILayout.EndVertical();
    }

    // ─────────────────────────────────────────
    //  익스포트 로직
    // ─────────────────────────────────────────
    void Export()
    {
        // 출력 경로 미리 계산 (메시 폴더가 JSON과 같은 디렉토리에 생성됨)
        string fullPath  = Path.Combine(Application.dataPath,
            m_outputPath.StartsWith("Assets/") ? m_outputPath.Substring(7) : m_outputPath);
        string outputDir = Path.GetDirectoryName(fullPath);
        string meshDir   = Path.Combine(outputDir, m_meshSubfolder);
        Directory.CreateDirectory(outputDir);

        var sb = new StringBuilder();
        sb.AppendLine("{");

        if (m_exportPlayer)    AppendPlayerSpawn(sb);
        if (m_exportRooms)     AppendRooms(sb);
        if (m_exportEnemies)   AppendEnemySpawns(sb);
        // Obstacle 태그 오브젝트는 AppendMapObjects에서 "prop":true로 처리됨
        if (m_exportMeshes)    AppendMapObjects(sb, meshDir);
        if (m_exportNavMesh)   AppendNavMesh(sb);

        // 마지막 쉼표 제거 후 닫기
        string json = sb.ToString().TrimEnd(',', '\n', '\r', ' ');
        json += "\n}";

        if (m_prettyPrint)
            json = PrettyPrint(json);

        File.WriteAllText(fullPath, json, Encoding.UTF8);
        AssetDatabase.Refresh();

        m_lastLog = $"저장 완료: {m_outputPath}\n" +
                    $"파일 크기: {new FileInfo(fullPath).Length / 1024.0f:F1} KB";
        Debug.Log($"[MapExporter] {m_lastLog}");
    }

    // ─────────────────────────────────────────
    //  섹션별 JSON 생성
    // ─────────────────────────────────────────

    void AppendPlayerSpawn(StringBuilder sb)
    {
        var spawns = FindObjectsOfType<PlayerSpawnPoint>();
        if (spawns.Length == 0) return;

        PlayerSpawnPoint sp = System.Array.Find(spawns, x => x.isDefault) ?? spawns[0];

        sb.AppendLine($"  \"playerSpawn\": {{");
        sb.AppendLine($"    \"position\": {V3(sp.transform.position)},");
        sb.AppendLine($"    \"rotationY\": {sp.transform.eulerAngles.y:F2}");
        sb.AppendLine($"  }},");
    }

    void AppendRooms(StringBuilder sb)
    {
        var roomObjects = GameObject.FindGameObjectsWithTag("Room");
        sb.AppendLine("  \"rooms\": [");

        for (int i = 0; i < roomObjects.Length; i++)
        {
            var go     = roomObjects[i];
            Bounds bounds = GetObjectBounds(go);

            sb.AppendLine($"    {{");
            sb.AppendLine($"      \"id\": {i},");
            sb.AppendLine($"      \"name\": \"{EscapeJson(go.name)}\",");
            sb.AppendLine($"      \"boundsMin\": {V3(bounds.min)},");
            sb.AppendLine($"      \"boundsMax\": {V3(bounds.max)},");
            sb.AppendLine($"      \"center\": {V3(bounds.center)}");
            sb.Append("    }");
            sb.AppendLine(i < roomObjects.Length - 1 ? "," : "");
        }

        sb.AppendLine("  ],");
    }

    void AppendEnemySpawns(StringBuilder sb)
    {
        var spawns = FindObjectsOfType<EnemySpawnPoint>();
        sb.AppendLine("  \"enemySpawns\": [");

        for (int i = 0; i < spawns.Length; i++)
        {
            var sp = spawns[i];

            sb.AppendLine($"    {{");
            sb.AppendLine($"      \"presetName\": \"{EscapeJson(sp.presetName)}\",");
            sb.AppendLine($"      \"count\": {sp.count},");
            sb.AppendLine($"      \"activationDelay\": {sp.activationDelay:F2},");
            sb.AppendLine($"      \"position\": {V3(sp.transform.position)},");
            sb.AppendLine($"      \"rotationY\": {sp.transform.eulerAngles.y:F2},");
            sb.AppendLine($"      \"stats\": {{");
            sb.AppendLine($"        \"maxHP\": {sp.maxHP:F1},");
            sb.AppendLine($"        \"moveSpeed\": {sp.moveSpeed:F2},");
            sb.AppendLine($"        \"attackRange\": {sp.attackRange:F2},");
            sb.AppendLine($"        \"attackCooldown\": {sp.attackCooldown:F2},");
            sb.AppendLine($"        \"detectionRange\": {sp.detectionRange:F2}");
            sb.AppendLine($"      }},");
            sb.AppendLine($"      \"attackType\": \"{sp.attackType}\",");
            sb.AppendLine($"      \"indicator\": {{");
            sb.AppendLine($"        \"type\": \"{sp.indicatorType}\",");
            sb.AppendLine($"        \"rushDistance\": {sp.rushDistance:F2},");
            sb.AppendLine($"        \"hitRadius\": {sp.hitRadius:F2},");
            sb.AppendLine($"        \"coneAngle\": {sp.coneAngle:F2}");
            sb.AppendLine($"      }},");
            sb.AppendLine($"      \"visual\": {{");
            sb.AppendLine($"        \"meshPath\": \"{EscapeJson(sp.meshPath)}\",");
            sb.AppendLine($"        \"animationPath\": \"{EscapeJson(sp.animationPath)}\",");
            sb.AppendLine($"        \"scale\": {V3S(sp.scale)},");
            sb.AppendLine($"        \"color\": {Color32Json(sp.color)}");
            sb.AppendLine($"      }},");
            sb.AppendLine($"      \"animClips\": {{");
            sb.AppendLine($"        \"idle\": \"{EscapeJson(sp.clipIdle)}\",");
            sb.AppendLine($"        \"chase\": \"{EscapeJson(sp.clipChase)}\",");
            sb.AppendLine($"        \"attack\": \"{EscapeJson(sp.clipAttack)}\",");
            sb.AppendLine($"        \"stagger\": \"{EscapeJson(sp.clipStagger)}\",");
            sb.AppendLine($"        \"death\": \"{EscapeJson(sp.clipDeath)}\"");
            sb.AppendLine($"      }}");
            sb.Append("    }");
            sb.AppendLine(i < spawns.Length - 1 ? "," : "");
        }

        sb.AppendLine("  ],");
    }

    void AppendObstacles(StringBuilder sb)
    {
        var obstacles = GameObject.FindGameObjectsWithTag("Obstacle");
        sb.AppendLine("  \"obstacles\": [");

        for (int i = 0; i < obstacles.Length; i++)
        {
            var go  = obstacles[i];
            var col = go.GetComponent<Collider>();
            if (col == null) continue;

            Bounds b = col.bounds;
            sb.AppendLine($"    {{");
            sb.AppendLine($"      \"name\": \"{EscapeJson(go.name)}\",");
            sb.AppendLine($"      \"center\": {V3(b.center)},");
            sb.AppendLine($"      \"size\": {V3S(b.size)}");
            sb.Append("    }");
            sb.AppendLine(i < obstacles.Length - 1 ? "," : "");
        }

        sb.AppendLine("  ],");
    }

    /// <summary>
    /// MapMesh 태그 오브젝트의 Transform + 메시 경로를 JSON에 추가하고
    /// 고유 메시를 OBJ 파일로 추출합니다.
    /// </summary>
    void AppendMapObjects(StringBuilder sb, string meshOutputDir)
    {
        // MapMesh (충돌 포함) + Obstacle (prop: 렌더만, 충돌 없음)
        var mapMeshGOs  = SafeFindWithTag("MapMesh");
        var obstacleGOs = m_exportObstacles ? SafeFindWithTag("Obstacle") : new GameObject[0];

        // 유효한 MeshFilter 오브젝트 목록 구성 (isProp 플래그 포함)
        var meshObjects = new System.Collections.Generic.List<(GameObject go, bool isProp)>();
        foreach (var go in mapMeshGOs)
        {
            var mf = go.GetComponent<MeshFilter>();
            if (mf != null && mf.sharedMesh != null) meshObjects.Add((go, false));
        }
        foreach (var go in obstacleGOs)
        {
            var mf = go.GetComponent<MeshFilter>();
            if (mf != null && mf.sharedMesh != null) meshObjects.Add((go, true));
            else if (mf == null) Debug.LogWarning($"[MapExporter] Obstacle '{go.name}': MeshFilter 없음, 건너뜀");
        }
        if (meshObjects.Count == 0) return;

        Directory.CreateDirectory(meshOutputDir);

        // 텍스처 출력 폴더 (meshes/textures/)
        string texOutputDir = Path.Combine(meshOutputDir, "textures");
        Directory.CreateDirectory(texOutputDir);

        // 고유 메시 추적: instanceID → 저장된 파일명
        var exportedMeshes = new Dictionary<int, string>();
        // 파일명 중복 방지: 이름 → 카운터
        var nameCounter = new Dictionary<string, int>();
        // 텍스처 중복 복사 방지: assetPath → 복사된 파일명
        var exportedTextures = new Dictionary<string, string>();

        sb.AppendLine("  \"mapObjects\": [");

        int written = 0;
        for (int i = 0; i < meshObjects.Count; i++)
        {
            var (go, isProp) = meshObjects[i];
            var mf = go.GetComponent<MeshFilter>();
            Mesh mesh   = mf.sharedMesh;
            int  meshID = mesh.GetInstanceID();

            // 같은 메시가 처음이면 OBJ 파일 생성
            if (!exportedMeshes.TryGetValue(meshID, out string meshFileName))
            {
                string baseName = SanitizeFileName(
                    string.IsNullOrEmpty(mesh.name) ? "mesh" : mesh.name);

                // 같은 이름의 다른 메시가 있을 경우 _1, _2 ... 접미사
                if (nameCounter.TryGetValue(baseName, out int cnt))
                {
                    nameCounter[baseName] = cnt + 1;
                    baseName = $"{baseName}_{cnt}";
                }
                else
                {
                    nameCounter[baseName] = 1;
                }

                meshFileName = baseName + ".obj";
                ExportOBJ(mesh, Path.Combine(meshOutputDir, meshFileName));
                exportedMeshes[meshID] = meshFileName;
            }

            if (written > 0) { /* 쉼표는 앞 항목 뒤에 붙임 */ }

            var rend = go.GetComponent<Renderer>();

            sb.AppendLine($"    {{");
            sb.AppendLine($"      \"name\": \"{EscapeJson(go.name)}\",");
            sb.AppendLine($"      \"meshFile\": \"{m_meshSubfolder}/{meshFileName}\",");
            sb.AppendLine($"      \"position\": {V3(go.transform.position)},");
            sb.AppendLine($"      \"rotation\": {Quat(go.transform.rotation)},");
            sb.AppendLine($"      \"scale\": {V3S(go.transform.lossyScale)},");
            AppendMaterialFields(sb, rend, isProp, texOutputDir, exportedTextures);
            bool hasNext = (i < meshObjects.Count - 1);
            sb.Append("    }");
            sb.AppendLine(hasNext ? "," : "");
            written++;
        }

        sb.AppendLine("  ],");
    }

    // ─────────────────────────────────────────
    //  룸별 분리 익스포트
    // ─────────────────────────────────────────

    /// <summary>
    /// Room 태그 오브젝트마다 별도 JSON 파일로 익스포트하고 rooms.json 매니페스트를 생성합니다.
    /// 각 방 파일은 기존 map.json과 동일한 포맷이며 MapLoader가 그대로 로드할 수 있습니다.
    /// </summary>
    void ExportRooms()
    {
        string fullOutputDir = Path.Combine(Application.dataPath,
            m_roomsOutputDir.StartsWith("Assets/") ? m_roomsOutputDir.Substring(7) : m_roomsOutputDir);
        string meshDir = Path.Combine(fullOutputDir, m_meshSubfolder);
        Directory.CreateDirectory(fullOutputDir);
        Directory.CreateDirectory(meshDir);
        Directory.CreateDirectory(Path.Combine(meshDir, "textures"));

        var roomObjects = SafeFindWithTag("Room");
        if (roomObjects.Length == 0)
        {
            m_lastLog = "Room 태그 오브젝트가 없습니다.";
            Debug.LogWarning("[MapExporter] " + m_lastLog);
            return;
        }

        // 메시/텍스처 익스포트 상태 공유 (중복 OBJ 방지)
        var exportedMeshes   = new Dictionary<int, string>();
        var nameCounter      = new Dictionary<string, int>();
        var exportedTextures = new Dictionary<string, string>();

        var roomJsonPaths = new System.Collections.Generic.List<string>();

        for (int i = 0; i < roomObjects.Length; i++)
        {
            var roomGO   = roomObjects[i];
            string safe  = SanitizeFileName(roomGO.name);
            string fname = $"room_{safe}.json";
            string fullPath = Path.Combine(fullOutputDir, fname);
            string assetPath = "Assets" + fullPath.Replace(Application.dataPath, "").Replace('\\', '/');

            ExportRoomFile(roomGO, fullPath, meshDir, exportedMeshes, nameCounter, exportedTextures);
            roomJsonPaths.Add(assetPath);
        }

        // manifest: rooms.json
        var msb = new StringBuilder();
        msb.AppendLine("{");
        msb.AppendLine("  \"rooms\": [");
        for (int i = 0; i < roomJsonPaths.Count; i++)
        {
            msb.Append($"    \"{EscapeJson(roomJsonPaths[i])}\"");
            msb.AppendLine(i < roomJsonPaths.Count - 1 ? "," : "");
        }
        msb.AppendLine("  ]");
        msb.AppendLine("}");
        File.WriteAllText(Path.Combine(fullOutputDir, "rooms.json"), msb.ToString(), Encoding.UTF8);

        AssetDatabase.Refresh();
        m_lastLog = $"룸 분리 완료: {roomObjects.Length}개 방\n폴더: {m_roomsOutputDir}";
        Debug.Log($"[MapExporter] {m_lastLog}");
    }

    /// <summary>
    /// 단일 Room 오브젝트를 standalone map.json 포맷으로 익스포트합니다.
    /// 자식 중 MapMesh 태그 → mapObjects (충돌 포함)
    /// 자식 중 Obstacle 태그 → mapObjects (prop: true, 렌더만)
    /// 자식 EnemySpawnPoint 컴포넌트 → enemySpawns
    /// </summary>
    void ExportRoomFile(
        GameObject roomGO, string roomJsonPath, string meshDir,
        Dictionary<int, string> exportedMeshes,
        Dictionary<string, int> nameCounter,
        Dictionary<string, string> exportedTextures)
    {
        string texOutputDir = Path.Combine(meshDir, "textures");
        var sb = new StringBuilder();
        sb.AppendLine("{");

        // Player spawn (자식 PlayerSpawnPoint 또는 방 중심)
        Vector3 spawnPos = GetObjectBounds(roomGO).center;
        var playerSpawnComp = roomGO.GetComponentInChildren<PlayerSpawnPoint>();
        if (playerSpawnComp != null) spawnPos = playerSpawnComp.transform.position;

        sb.AppendLine("  \"playerSpawn\": {");
        sb.AppendLine($"    \"position\": {V3(spawnPos)},");
        sb.AppendLine($"    \"rotationY\": 0");
        sb.AppendLine("  },");

        // 방 경계 (단일 Room 항목)
        Bounds bounds = GetObjectBounds(roomGO);
        sb.AppendLine("  \"rooms\": [");
        sb.AppendLine("    {");
        sb.AppendLine($"      \"id\": 0,");
        sb.AppendLine($"      \"name\": \"{EscapeJson(roomGO.name)}\",");
        sb.AppendLine($"      \"boundsMin\": {V3(bounds.min)},");
        sb.AppendLine($"      \"boundsMax\": {V3(bounds.max)},");
        sb.AppendLine($"      \"center\": {V3(bounds.center)}");
        sb.AppendLine("    }");
        sb.AppendLine("  ],");

        // 적 스폰 (자식 EnemySpawnPoint)
        var spawns = roomGO.GetComponentsInChildren<EnemySpawnPoint>();
        sb.AppendLine("  \"enemySpawns\": [");
        for (int j = 0; j < spawns.Length; j++)
        {
            WriteEnemySpawnEntry(sb, spawns[j], "    ");
            sb.AppendLine(j < spawns.Length - 1 ? "," : "");
        }
        sb.AppendLine("  ],");

        sb.AppendLine("  \"obstacles\": [],");

        // 맵 오브젝트 (자식 MapMesh + Obstacle 태그)
        var allObjects = new System.Collections.Generic.List<(GameObject go, bool isProp)>();
        foreach (Transform t in roomGO.GetComponentsInChildren<Transform>(true))
        {
            if (t == roomGO.transform) continue;
            var mf = t.GetComponent<MeshFilter>();
            if (mf == null || mf.sharedMesh == null) continue;
            if (t.CompareTag("MapMesh"))  allObjects.Add((t.gameObject, false));
            else if (t.CompareTag("Obstacle")) allObjects.Add((t.gameObject, true));
        }

        sb.AppendLine("  \"mapObjects\": [");
        for (int k = 0; k < allObjects.Count; k++)
        {
            var (go, isProp) = allObjects[k];
            WriteMapObjectEntry(sb, go, isProp, meshDir, texOutputDir,
                                exportedMeshes, nameCounter, exportedTextures);
            sb.AppendLine(k < allObjects.Count - 1 ? "," : "");
        }
        sb.AppendLine("  ]");
        sb.Append("}");

        File.WriteAllText(roomJsonPath, sb.ToString(), Encoding.UTF8);
        Debug.Log($"[MapExporter] 방 파일 저장: {Path.GetFileName(roomJsonPath)} " +
                  $"({allObjects.Count} 오브젝트, {spawns.Length} 스폰)");
    }

    void WriteEnemySpawnEntry(StringBuilder sb, EnemySpawnPoint sp, string indent)
    {
        sb.AppendLine($"{indent}{{");
        sb.AppendLine($"{indent}  \"presetName\": \"{EscapeJson(sp.presetName)}\",");
        sb.AppendLine($"{indent}  \"count\": {sp.count},");
        sb.AppendLine($"{indent}  \"activationDelay\": {sp.activationDelay:F2},");
        sb.AppendLine($"{indent}  \"position\": {V3(sp.transform.position)},");
        sb.AppendLine($"{indent}  \"rotationY\": {sp.transform.eulerAngles.y:F2},");
        sb.AppendLine($"{indent}  \"stats\": {{");
        sb.AppendLine($"{indent}    \"maxHP\": {sp.maxHP:F1},");
        sb.AppendLine($"{indent}    \"moveSpeed\": {sp.moveSpeed:F2},");
        sb.AppendLine($"{indent}    \"attackRange\": {sp.attackRange:F2},");
        sb.AppendLine($"{indent}    \"attackCooldown\": {sp.attackCooldown:F2},");
        sb.AppendLine($"{indent}    \"detectionRange\": {sp.detectionRange:F2}");
        sb.AppendLine($"{indent}  }},");
        sb.AppendLine($"{indent}  \"attackType\": \"{sp.attackType}\",");
        sb.AppendLine($"{indent}  \"indicator\": {{");
        sb.AppendLine($"{indent}    \"type\": \"{sp.indicatorType}\",");
        sb.AppendLine($"{indent}    \"rushDistance\": {sp.rushDistance:F2},");
        sb.AppendLine($"{indent}    \"hitRadius\": {sp.hitRadius:F2},");
        sb.AppendLine($"{indent}    \"coneAngle\": {sp.coneAngle:F2}");
        sb.AppendLine($"{indent}  }},");
        sb.AppendLine($"{indent}  \"visual\": {{");
        sb.AppendLine($"{indent}    \"meshPath\": \"{EscapeJson(sp.meshPath)}\",");
        sb.AppendLine($"{indent}    \"animationPath\": \"{EscapeJson(sp.animationPath)}\",");
        sb.AppendLine($"{indent}    \"scale\": {V3S(sp.scale)},");
        sb.AppendLine($"{indent}    \"color\": {Color32Json(sp.color)}");
        sb.AppendLine($"{indent}  }},");
        sb.AppendLine($"{indent}  \"animClips\": {{");
        sb.AppendLine($"{indent}    \"idle\": \"{EscapeJson(sp.clipIdle)}\",");
        sb.AppendLine($"{indent}    \"chase\": \"{EscapeJson(sp.clipChase)}\",");
        sb.AppendLine($"{indent}    \"attack\": \"{EscapeJson(sp.clipAttack)}\",");
        sb.AppendLine($"{indent}    \"stagger\": \"{EscapeJson(sp.clipStagger)}\",");
        sb.AppendLine($"{indent}    \"death\": \"{EscapeJson(sp.clipDeath)}\"");
        sb.AppendLine($"{indent}  }}");
        sb.Append($"{indent}}}");
    }

    void WriteMapObjectEntry(
        StringBuilder sb, GameObject go, bool isProp,
        string meshDir, string texOutputDir,
        Dictionary<int, string> exportedMeshes,
        Dictionary<string, int> nameCounter,
        Dictionary<string, string> exportedTextures)
    {
        var mf     = go.GetComponent<MeshFilter>();
        Mesh mesh  = mf.sharedMesh;
        int meshID = mesh.GetInstanceID();

        if (!exportedMeshes.TryGetValue(meshID, out string meshFileName))
        {
            string baseName = SanitizeFileName(string.IsNullOrEmpty(mesh.name) ? "mesh" : mesh.name);
            if (nameCounter.TryGetValue(baseName, out int cnt))
            {
                nameCounter[baseName] = cnt + 1;
                baseName = $"{baseName}_{cnt}";
            }
            else nameCounter[baseName] = 1;
            meshFileName = baseName + ".obj";
            ExportOBJ(mesh, Path.Combine(meshDir, meshFileName));
            exportedMeshes[meshID] = meshFileName;
        }

        var rend = go.GetComponent<Renderer>();

        sb.AppendLine($"    {{");
        sb.AppendLine($"      \"name\": \"{EscapeJson(go.name)}\",");
        sb.AppendLine($"      \"meshFile\": \"{m_meshSubfolder}/{meshFileName}\",");
        sb.AppendLine($"      \"position\": {V3(go.transform.position)},");
        sb.AppendLine($"      \"rotation\": {Quat(go.transform.rotation)},");
        sb.AppendLine($"      \"scale\": {V3S(go.transform.lossyScale)},");
        AppendMaterialFields(sb, rend, isProp, texOutputDir, exportedTextures);
        sb.Append($"    }}");
    }

    void AppendNavMesh(StringBuilder sb)
    {
        NavMeshTriangulation tri = NavMesh.CalculateTriangulation();

        sb.AppendLine($"  \"navMesh\": {{");
        sb.AppendLine("    \"vertices\": [");
        for (int i = 0; i < tri.vertices.Length; i++)
        {
            string comma = i < tri.vertices.Length - 1 ? "," : "";
            sb.AppendLine($"      {V3(tri.vertices[i])}{comma}");
        }
        sb.AppendLine("    ],");

        sb.Append("    \"indices\": [");
        for (int i = 0; i < tri.indices.Length; i++)
        {
            if (i % 12 == 0) sb.AppendLine().Append("      ");
            sb.Append(tri.indices[i]);
            if (i < tri.indices.Length - 1) sb.Append(", ");
        }
        sb.AppendLine();
        sb.AppendLine("    ]");
        sb.AppendLine("  }}");  // navMesh 닫기 (마지막 항목, 쉼표 없음)
    }

    // ─────────────────────────────────────────
    //  OBJ 익스포터
    // ─────────────────────────────────────────

    /// <summary>
    /// Unity Mesh → .obj 파일 저장.
    ///
    /// 좌표계 변환 (Unity 좌수계 → DirectX 우수계):
    ///   - 위치/법선: Z 부호 반전
    ///   - UV: V 반전 (Unity 좌하단 → DirectX 좌상단 원점)
    ///   - 와인딩: Z 반전으로 앞면이 뒤집히므로 삼각형 인덱스 b/c 교환
    /// </summary>
    static void ExportOBJ(Mesh mesh, string filePath)
    {
        var sb = new StringBuilder();
        sb.AppendLine($"# {mesh.name}");
        sb.AppendLine($"# Exported by MapExporter (Unity -> DirectX12)");
        sb.AppendLine($"# Vertices: {mesh.vertexCount}  Triangles: {mesh.triangles.Length / 3}");
        sb.AppendLine();

        // 정점 (Z 반전)
        foreach (var v in mesh.vertices)
            sb.AppendLine($"v {v.x:F6} {v.y:F6} {-v.z:F6}");
        sb.AppendLine();

        // 법선 (Z 반전)
        bool hasNormals = mesh.normals.Length > 0;
        if (hasNormals)
        {
            foreach (var n in mesh.normals)
                sb.AppendLine($"vn {n.x:F6} {n.y:F6} {-n.z:F6}");
            sb.AppendLine();
        }

        // UV (V 반전: Unity 좌하단 원점 → DirectX 좌상단 원점)
        bool hasUVs = mesh.uv.Length > 0;
        if (hasUVs)
        {
            foreach (var uv in mesh.uv)
                sb.AppendLine($"vt {uv.x:F6} {1.0f - uv.y:F6}");
            sb.AppendLine();
        }

        // 삼각형 (Z 반전으로 와인딩이 뒤집히므로 b, c 교환)
        for (int sub = 0; sub < mesh.subMeshCount; sub++)
        {
            if (mesh.subMeshCount > 1)
                sb.AppendLine($"g sub_{sub}");

            int[] tris = mesh.GetTriangles(sub);
            for (int t = 0; t < tris.Length; t += 3)
            {
                int a = tris[t]   + 1;
                int b = tris[t+2] + 1;  // b, c 교환
                int c = tris[t+1] + 1;

                if (hasNormals && hasUVs)
                    sb.AppendLine($"f {a}/{a}/{a} {b}/{b}/{b} {c}/{c}/{c}");
                else if (hasNormals)
                    sb.AppendLine($"f {a}//{a} {b}//{b} {c}//{c}");
                else if (hasUVs)
                    sb.AppendLine($"f {a}/{a} {b}/{b} {c}/{c}");
                else
                    sb.AppendLine($"f {a} {b} {c}");
            }
        }

        File.WriteAllText(filePath, sb.ToString(), Encoding.UTF8);
        Debug.Log($"[MapExporter] OBJ: {Path.GetFileName(filePath)} " +
                  $"({mesh.vertexCount}v / {mesh.triangles.Length / 3}tri)");
    }

    // ─────────────────────────────────────────
    //  유틸리티
    // ─────────────────────────────────────────

    static Bounds GetObjectBounds(GameObject go)
    {
        Renderer[] renderers = go.GetComponentsInChildren<Renderer>();
        if (renderers.Length > 0)
        {
            Bounds b = renderers[0].bounds;
            foreach (var r in renderers) b.Encapsulate(r.bounds);
            return b;
        }
        Collider[] cols = go.GetComponentsInChildren<Collider>();
        if (cols.Length > 0)
        {
            Bounds b = cols[0].bounds;
            foreach (var c in cols) b.Encapsulate(c.bounds);
            return b;
        }
        return new Bounds(go.transform.position, Vector3.zero);
    }

    // 위치/법선용: Z 반전 (Unity 좌수계 → DirectX 우수계)
    static string V3(Vector3 v)
        => $"[{v.x:F4}, {v.y:F4}, {-v.z:F4}]";

    // 스케일/크기용: 부호 변환 없이 그대로
    static string V3S(Vector3 v)
        => $"[{v.x:F4}, {v.y:F4}, {v.z:F4}]";

    // 쿼터니언: Z 반전 좌표계에서 (qx, qy, -qz, qw)
    static string Quat(Quaternion q)
        => $"[{q.x:F6}, {q.y:F6}, {-q.z:F6}, {q.w:F6}]";

    static string Color32Json(Color c)
    {
        Color32 c32 = c;
        return $"[{c32.r}, {c32.g}, {c32.b}, {c32.a}]";
    }

    // 단일 Material에서 데이터 추출 (색상, smoothness, metallic, emissive, 텍스처)
    struct MatData
    {
        public Color  color;
        public float  smooth;
        public float  metallic;
        public Color  emissive;
        public bool   hasEmissive;
        public string texJsonPath;
        public string emissiveTexJsonPath; // _EmissionMap 텍스처 경로
    }

    MatData ExtractMatData(Material mat, string texOutputDir, Dictionary<string, string> exportedTextures)
    {
        var d = new MatData { color = Color.white, smooth = 0.5f, metallic = 0f, emissive = Color.black };

        if (mat == null) return d;

        if      (mat.HasProperty("_BaseColor")) d.color = mat.GetColor("_BaseColor");
        else if (mat.HasProperty("_Color"))     d.color = mat.GetColor("_Color");

        if      (mat.HasProperty("_Smoothness"))  d.smooth   = mat.GetFloat("_Smoothness");
        else if (mat.HasProperty("_Glossiness"))  d.smooth   = mat.GetFloat("_Glossiness");
        if      (mat.HasProperty("_Metallic"))    d.metallic = mat.GetFloat("_Metallic");

        // Emission: Unity _EmissionColor는 HDR(채널값 > 1.0 가능).
        // maxComponent로 정규화해 Color32 clamp 현상(채널이 255로 뭉개짐)을 방지하고,
        // 색조(hue)는 보존하면서 최대 채널값을 1.0으로 맞춤.
        if (mat.HasProperty("_EmissionColor"))
        {
            Color em = mat.GetColor("_EmissionColor");
            float maxC = Mathf.Max(em.r, em.g, em.b);
            if (maxC > 0.001f)
            {
                if (maxC > 1f)
                    em = new Color(em.r / maxC, em.g / maxC, em.b / maxC, 1f);
                d.emissive    = em;
                d.hasEmissive = true;
            }
        }

        // Emission Map 텍스처 복사 (_EmissionMap)
        Texture2D emissionTex = null;
        if (mat.HasProperty("_EmissionMap")) emissionTex = mat.GetTexture("_EmissionMap") as Texture2D;
        if (emissionTex != null)
        {
            string assetPath = AssetDatabase.GetAssetPath(emissionTex);
            if (!string.IsNullOrEmpty(assetPath))
            {
                if (!exportedTextures.TryGetValue(assetPath, out string texFileName))
                {
                    string srcExt      = Path.GetExtension(assetPath).ToLower();
                    bool   needConvert = (srcExt == ".tga" || srcExt == ".psd" || srcExt == ".exr" || srcExt == ".hdr");
                    string texBaseName = Path.GetFileNameWithoutExtension(assetPath);
                    string destExt     = needConvert ? ".png" : srcExt;
                    texFileName        = texBaseName + destExt;
                    string destPath    = Path.Combine(texOutputDir, texFileName);
                    if (!File.Exists(destPath))
                    {
                        if (needConvert) { var r = MakeTextureReadable(emissionTex); File.WriteAllBytes(destPath, r.EncodeToPNG()); }
                        else { string srcFull = Path.Combine(Path.GetDirectoryName(Application.dataPath), assetPath); File.Copy(srcFull, destPath, true); }
                    }
                    exportedTextures[assetPath] = texFileName;
                }
                d.emissiveTexJsonPath = $"{m_meshSubfolder}/textures/{texFileName}";
                // Emission Map이 있으면 hasEmissive도 활성화 (_EmissionColor가 검은색이어도)
                if (!d.hasEmissive) { d.emissive = Color.white; d.hasEmissive = true; }
            }
        }

        // 알베도 텍스처 복사
        Texture2D albedo = null;
        if      (mat.HasProperty("_BaseMap")) albedo = mat.GetTexture("_BaseMap") as Texture2D;
        else if (mat.HasProperty("_MainTex")) albedo = mat.GetTexture("_MainTex") as Texture2D;

        if (albedo != null)
        {
            string assetPath = AssetDatabase.GetAssetPath(albedo);
            if (!string.IsNullOrEmpty(assetPath))
            {
                if (!exportedTextures.TryGetValue(assetPath, out string texFileName))
                {
                    string srcExt      = Path.GetExtension(assetPath).ToLower();
                    bool   needConvert = (srcExt == ".tga" || srcExt == ".psd" || srcExt == ".exr" || srcExt == ".hdr");
                    string texBaseName = Path.GetFileNameWithoutExtension(assetPath);
                    string destExt     = needConvert ? ".png" : srcExt;
                    texFileName        = texBaseName + destExt;
                    string destPath    = Path.Combine(texOutputDir, texFileName);
                    if (!File.Exists(destPath))
                    {
                        if (needConvert) { var r = MakeTextureReadable(albedo); File.WriteAllBytes(destPath, r.EncodeToPNG()); }
                        else { string srcFull = Path.Combine(Path.GetDirectoryName(Application.dataPath), assetPath); File.Copy(srcFull, destPath, true); }
                    }
                    exportedTextures[assetPath] = texFileName;
                }
                d.texJsonPath = $"{m_meshSubfolder}/textures/{texFileName}";
            }
        }
        return d;
    }

    // 머터리얼 JSON 필드를 sb에 추가.
    // 단일 머터리얼이면 color/smoothness/metallic/emissive/texture 플랫 필드,
    // 복수 머터리얼이면 "materials" 배열.
    void AppendMaterialFields(StringBuilder sb, Renderer rend, bool isProp,
                              string texOutputDir, Dictionary<string, string> exportedTextures)
    {
        if (rend == null || rend.sharedMaterials == null || rend.sharedMaterials.Length == 0)
        {
            // 머터리얼 없음: 기본값
            sb.AppendLine($"      \"color\": [255, 255, 255, 255],");
            sb.AppendLine($"      \"smoothness\": 0.500,");
            sb.AppendLine($"      \"metallic\": 0.000,");
            if (isProp) { sb.AppendLine($"      \"texture\": \"\","); sb.AppendLine($"      \"prop\": true"); }
            else          sb.AppendLine($"      \"texture\": \"\"");
            return;
        }

        var mats = rend.sharedMaterials;

        if (mats.Length == 1)
        {
            // 단일 머터리얼: 기존 플랫 필드
            var d = ExtractMatData(mats[0], texOutputDir, exportedTextures);
            sb.AppendLine($"      \"color\": {Color32Json(d.color)},");
            sb.AppendLine($"      \"smoothness\": {d.smooth:F3},");
            sb.AppendLine($"      \"metallic\": {d.metallic:F3},");
            if (d.hasEmissive)
                sb.AppendLine($"      \"emissive\": {Color32Json(d.emissive)},");
            if (!string.IsNullOrEmpty(d.emissiveTexJsonPath))
                sb.AppendLine($"      \"emissiveTexture\": \"{EscapeJson(d.emissiveTexJsonPath)}\",");
            if (isProp) { sb.AppendLine($"      \"texture\": \"{EscapeJson(d.texJsonPath)}\","); sb.AppendLine($"      \"prop\": true"); }
            else          sb.AppendLine($"      \"texture\": \"{EscapeJson(d.texJsonPath)}\"");
        }
        else
        {
            // 복수 머터리얼: "materials" 배열
            sb.AppendLine($"      \"materials\": [");
            for (int mi = 0; mi < mats.Length; mi++)
            {
                var d = ExtractMatData(mats[mi], texOutputDir, exportedTextures);
                sb.AppendLine($"        {{");
                sb.AppendLine($"          \"color\": {Color32Json(d.color)},");
                sb.AppendLine($"          \"smoothness\": {d.smooth:F3},");
                sb.AppendLine($"          \"metallic\": {d.metallic:F3},");
                if (d.hasEmissive)
                    sb.AppendLine($"          \"emissive\": {Color32Json(d.emissive)},");
                if (!string.IsNullOrEmpty(d.emissiveTexJsonPath))
                    sb.AppendLine($"          \"emissiveTexture\": \"{EscapeJson(d.emissiveTexJsonPath)}\",");
                bool lastMat = (mi == mats.Length - 1);
                string texLine = $"          \"texture\": \"{EscapeJson(d.texJsonPath)}\"";
                sb.AppendLine(texLine);
                sb.Append($"        }}");
                sb.AppendLine(lastMat ? "" : ",");
            }
            if (isProp)
            {
                sb.AppendLine($"      ],");
                sb.AppendLine($"      \"prop\": true");
            }
            else
            {
                sb.AppendLine($"      ]");  // 마지막 필드 - 쉼표 없음
            }
        }
    }

    static string EscapeJson(string s)
        => s?.Replace("\\", "\\\\").Replace("\"", "\\\"") ?? "";

    static string SanitizeFileName(string name)
    {
        foreach (char c in Path.GetInvalidFileNameChars())
            name = name.Replace(c, '_');
        return name.Replace(' ', '_');
    }

    static string PrettyPrint(string json) => json;

    /// <summary>
    /// TGA/PSD 등 WIC가 직접 읽기 어려운 포맷을 PNG로 변환하기 위해
    /// Unity의 임포트된 Texture2D를 읽기 가능한 복사본으로 만듭니다.
    /// </summary>
    static Texture2D MakeTextureReadable(Texture2D src)
    {
        // RenderTexture를 임시로 사용해 isReadable 여부에 관계없이 픽셀 읽기
        var rt = RenderTexture.GetTemporary(src.width, src.height, 0,
            RenderTextureFormat.ARGB32, RenderTextureReadWrite.Linear);
        Graphics.Blit(src, rt);
        var prev = RenderTexture.active;
        RenderTexture.active = rt;
        var copy = new Texture2D(src.width, src.height, TextureFormat.ARGB32, false);
        copy.ReadPixels(new Rect(0, 0, rt.width, rt.height), 0, 0);
        copy.Apply();
        RenderTexture.active = prev;
        RenderTexture.ReleaseTemporary(rt);
        return copy;
    }

    /// <summary>
    /// 태그가 미등록이면 예외 대신 빈 배열을 반환합니다.
    /// Unity는 등록되지 않은 태그로 FindGameObjectsWithTag 를 호출하면 UnityException 을 던집니다.
    /// </summary>
    static GameObject[] SafeFindWithTag(string tag)
    {
        try { return GameObject.FindGameObjectsWithTag(tag); }
        catch { return new GameObject[0]; }
    }
}
