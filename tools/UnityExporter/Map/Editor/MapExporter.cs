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
        m_exportObstacles = EditorGUILayout.Toggle("장애물 콜라이더 (Obstacle 태그)", m_exportObstacles);

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

        // ── 익스포트 버튼 ──
        GUI.backgroundColor = new Color(0.3f, 0.7f, 0.4f);
        if (GUILayout.Button("Export", GUILayout.Height(36)))
            Export();
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
        if (m_exportObstacles) AppendObstacles(sb);
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
        var meshObjects = GameObject.FindGameObjectsWithTag("MapMesh");
        if (meshObjects.Length == 0) return;

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
        for (int i = 0; i < meshObjects.Length; i++)
        {
            var go = meshObjects[i];
            var mf = go.GetComponent<MeshFilter>();
            if (mf == null || mf.sharedMesh == null)
            {
                Debug.LogWarning($"[MapExporter] {go.name}: MeshFilter 없음, 건너뜀");
                continue;
            }

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

            // ── 머터리얼 정보 수집 ──────────────────────────────────────────────
            Color  matColor    = Color.white;
            float  matSmooth   = 0.5f;
            float  matMetallic = 0.0f;
            string texJsonPath = "";   // JSON에 쓸 상대 경로 (없으면 빈 문자열)

            var rend = go.GetComponent<Renderer>();
            if (rend != null && rend.sharedMaterial != null)
            {
                var mat = rend.sharedMaterial;

                // 기본 색상 (색조 곱, _BaseColor(URP) 또는 _Color(Built-in))
                if      (mat.HasProperty("_BaseColor")) matColor = mat.GetColor("_BaseColor");
                else if (mat.HasProperty("_Color"))     matColor = mat.GetColor("_Color");

                if (mat.HasProperty("_Smoothness")) matSmooth   = mat.GetFloat("_Smoothness");
                else if (mat.HasProperty("_Glossiness")) matSmooth = mat.GetFloat("_Glossiness");
                if (mat.HasProperty("_Metallic"))   matMetallic = mat.GetFloat("_Metallic");

                // 알베도(메인) 텍스처 복사
                Texture2D albedo = null;
                if      (mat.HasProperty("_BaseMap"))  albedo = mat.GetTexture("_BaseMap")  as Texture2D;
                else if (mat.HasProperty("_MainTex"))  albedo = mat.GetTexture("_MainTex")  as Texture2D;

                if (albedo != null)
                {
                    string assetPath = AssetDatabase.GetAssetPath(albedo);
                    if (!string.IsNullOrEmpty(assetPath))
                    {
                        if (!exportedTextures.TryGetValue(assetPath, out string texFileName))
                        {
                            // 확장자 유지하되 DX12가 읽을 수 있는 형식(PNG/JPG)으로 한정
                            string srcExt = Path.GetExtension(assetPath).ToLower();
                            // .tga/.psd/.exr → 런타임에서 처리하기 어려우므로 PNG로 재저장
                            bool needConvert = (srcExt == ".tga" || srcExt == ".psd"
                                             || srcExt == ".exr" || srcExt == ".hdr");

                            string texBaseName = Path.GetFileNameWithoutExtension(assetPath);
                            string destExt     = needConvert ? ".png" : srcExt;
                            texFileName        = texBaseName + destExt;
                            string destPath    = Path.Combine(texOutputDir, texFileName);

                            if (!File.Exists(destPath))
                            {
                                if (needConvert)
                                {
                                    // Unity가 이미 임포트한 Texture2D를 PNG로 인코딩
                                    var readable = MakeTextureReadable(albedo);
                                    File.WriteAllBytes(destPath, readable.EncodeToPNG());
                                }
                                else
                                {
                                    // 그대로 복사
                                    string srcFull = Path.Combine(
                                        Path.GetDirectoryName(Application.dataPath), assetPath);
                                    File.Copy(srcFull, destPath, true);
                                }
                            }
                            exportedTextures[assetPath] = texFileName;
                        }
                        texJsonPath = $"{m_meshSubfolder}/textures/{texFileName}";
                    }
                }
            }

            sb.AppendLine($"    {{");
            sb.AppendLine($"      \"name\": \"{EscapeJson(go.name)}\",");
            sb.AppendLine($"      \"meshFile\": \"{m_meshSubfolder}/{meshFileName}\",");
            sb.AppendLine($"      \"position\": {V3(go.transform.position)},");
            sb.AppendLine($"      \"rotation\": {Quat(go.transform.rotation)},");
            sb.AppendLine($"      \"scale\": {V3S(go.transform.lossyScale)},");
            sb.AppendLine($"      \"color\": {Color32Json(matColor)},");
            sb.AppendLine($"      \"smoothness\": {matSmooth:F3},");
            sb.AppendLine($"      \"metallic\": {matMetallic:F3},");
            sb.AppendLine($"      \"texture\": \"{EscapeJson(texJsonPath)}\""  );
            // 다음 항목이 있는지 확인해 쉼표 추가
            bool hasNext = false;
            for (int j = i + 1; j < meshObjects.Length; j++)
            {
                var nextMf = meshObjects[j].GetComponent<MeshFilter>();
                if (nextMf != null && nextMf.sharedMesh != null) { hasNext = true; break; }
            }
            sb.Append("    }");
            sb.AppendLine(hasNext ? "," : "");
            written++;
        }

        sb.AppendLine("  ],");
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
