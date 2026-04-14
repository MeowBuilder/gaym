/*
 * TerrainExporterDX12.cs
 * Unity Editor Window → DirectX 12 용 Terrain 데이터 일괄 추출
 *
 * [사용법]
 * 1. 이 파일을 Unity 프로젝트의 Assets/Editor/ 폴더에 복사
 * 2. Unity 메뉴 → Tools → DX12 Terrain Exporter
 * 3. Terrain 선택, 출력 경로 설정 후 Export All
 *
 * [추출 목록]
 * - terrain_heightmap.r16      : 16-bit 높이맵 (little-endian)
 * - terrain_splatmap0.png ...  : 레이어 블렌딩 가중치 (RGBA, 4레이어/장)
 * - Textures/layer{i}_diffuse.png  : 레이어 디퓨즈 텍스처
 * - Textures/layer{i}_normal.png   : 레이어 노멀맵
 * - Textures/layer{i}_mask.png     : 레이어 마스크맵 (Metallic/AO/Height/Smoothness)
 * - terrain_objects.json       : 나무 위치/회전/스케일 (월드 좌표)
 * - terrain_details.json       : 풀/디테일 밀도맵 경로 목록 (Phase 2)
 * - terrain_config.json        : 전체 메타데이터 (크기, 해상도, 레이어 정보 등)
 */

using UnityEngine;
using UnityEditor;
using System.IO;
using System.Collections.Generic;
using System.Text;

public class TerrainExporterDX12 : EditorWindow
{
    // ── UI 상태 ──────────────────────────────────────────────────
    private Terrain     m_terrain;
    private string      m_exportPath  = "";
    private bool        m_doHeightmap = true;
    private bool        m_doSplatmaps = true;
    private bool        m_doTextures  = true;
    private bool        m_doTrees     = true;
    private bool        m_doDetails   = false;  // Phase 2 (grass 밀도맵)
    private int         m_maxTexSize  = 2048;   // 레이어 텍스처 최대 해상도 (0 = 원본)
    private Vector2     m_scrollPos;
    private string      m_log         = "";

    // ── 메뉴 등록 ────────────────────────────────────────────────
    [MenuItem("Tools/DX12 Terrain Exporter")]
    public static void ShowWindow()
    {
        var w = GetWindow<TerrainExporterDX12>("DX12 Terrain Exporter");
        w.minSize = new Vector2(480, 560);
    }

    // ── GUI ──────────────────────────────────────────────────────
    void OnGUI()
    {
        EditorGUILayout.Space(8);
        EditorGUILayout.LabelField("DX12 Terrain Exporter", EditorStyles.whiteLargeLabel);
        DrawLine();

        // Terrain 선택
        m_terrain = (Terrain)EditorGUILayout.ObjectField("Target Terrain", m_terrain, typeof(Terrain), true);
        if (m_terrain == null)
        {
            if (GUILayout.Button("  Scene에서 자동 탐색"))
            {
                m_terrain = FindObjectOfType<Terrain>();
                if (m_terrain == null) Log("[WARN] Scene에 Terrain이 없습니다.");
            }
        }
        else
        {
            TerrainData d = m_terrain.terrainData;
            EditorGUILayout.HelpBox(
                $"크기: {d.size.x}×{d.size.y}×{d.size.z}m  |  " +
                $"높이맵: {d.heightmapResolution}²  |  " +
                $"알파맵: {d.alphamapResolution}²  |  " +
                $"레이어: {d.terrainLayers.Length}개  |  " +
                $"나무: {d.treeInstances.Length}개",
                MessageType.Info);
        }

        EditorGUILayout.Space(6);

        // 출력 경로
        EditorGUILayout.BeginHorizontal();
        m_exportPath = EditorGUILayout.TextField("출력 경로", m_exportPath);
        if (GUILayout.Button("…", GUILayout.Width(28)))
        {
            string sel = EditorUtility.OpenFolderPanel("출력 폴더 선택", m_exportPath, "");
            if (!string.IsNullOrEmpty(sel)) m_exportPath = sel;
        }
        EditorGUILayout.EndHorizontal();

        EditorGUILayout.Space(8);
        DrawLine();
        EditorGUILayout.LabelField("추출 항목", EditorStyles.boldLabel);

        m_doHeightmap = EditorGUILayout.Toggle("Heightmap (.r16 16-bit)",       m_doHeightmap);
        m_doSplatmaps = EditorGUILayout.Toggle("Splatmaps (.png RGBA)",         m_doSplatmaps);
        m_doTextures  = EditorGUILayout.Toggle("Layer Textures (diffuse/normal/mask)", m_doTextures);
        m_doTrees     = EditorGUILayout.Toggle("Tree Instances (JSON)",          m_doTrees);
        m_doDetails   = EditorGUILayout.Toggle("Detail/Grass Density Maps (Phase 2)", m_doDetails);

        EditorGUILayout.Space(4);
        m_maxTexSize = EditorGUILayout.IntField("레이어 텍스처 최대 해상도 (0=원본)", m_maxTexSize);

        EditorGUILayout.Space(10);
        DrawLine();

        // Export 버튼
        GUI.enabled = m_terrain != null && !string.IsNullOrEmpty(m_exportPath);
        var btnStyle = new GUIStyle(GUI.skin.button) { fontSize = 14, fontStyle = FontStyle.Bold };
        if (GUILayout.Button("Export All", btnStyle, GUILayout.Height(40)))
            DoExport();
        GUI.enabled = true;

        // 로그
        EditorGUILayout.Space(8);
        EditorGUILayout.LabelField("Log", EditorStyles.boldLabel);
        m_scrollPos = EditorGUILayout.BeginScrollView(m_scrollPos, GUILayout.Height(160));
        EditorGUILayout.TextArea(m_log, GUILayout.ExpandHeight(true));
        EditorGUILayout.EndScrollView();

        if (GUILayout.Button("로그 지우기")) m_log = "";
    }

    // ── 메인 Export ──────────────────────────────────────────────
    void DoExport()
    {
        m_log = "";

        if (m_terrain == null)         { Log("[ERR] Terrain이 선택되지 않았습니다."); return; }
        if (string.IsNullOrEmpty(m_exportPath)) { Log("[ERR] 출력 경로를 지정하세요."); return; }

        // 출력 폴더 생성
        Directory.CreateDirectory(m_exportPath);
        Directory.CreateDirectory(Path.Combine(m_exportPath, "Textures"));

        TerrainData data = m_terrain.terrainData;
        Log($"=== Export Start: {m_terrain.name} ===");
        Log($"  World Pos  : {m_terrain.transform.position}");
        Log($"  Size       : {data.size}");
        Log($"  Heightmap  : {data.heightmapResolution}×{data.heightmapResolution}");
        Log($"  Alphamap   : {data.alphamapResolution}×{data.alphamapResolution}");
        Log($"  Layers     : {data.terrainLayers.Length}");
        Log($"  Trees      : {data.treeInstances.Length} instances / {data.treePrototypes.Length} protos");

        try
        {
            EditorUtility.DisplayProgressBar("Exporting Terrain", "Heightmap...", 0.1f);
            if (m_doHeightmap) ExportHeightmap(m_terrain, data);

            EditorUtility.DisplayProgressBar("Exporting Terrain", "Splatmaps...", 0.3f);
            if (m_doSplatmaps) ExportSplatmaps(data);

            EditorUtility.DisplayProgressBar("Exporting Terrain", "Layer Textures...", 0.5f);
            if (m_doTextures) ExportLayerTextures(data);

            EditorUtility.DisplayProgressBar("Exporting Terrain", "Tree Instances...", 0.75f);
            if (m_doTrees) ExportTreeInstances(m_terrain, data);

            EditorUtility.DisplayProgressBar("Exporting Terrain", "Detail/Grass...", 0.85f);
            if (m_doDetails) ExportDetailMaps(data);

            EditorUtility.DisplayProgressBar("Exporting Terrain", "Config JSON...", 0.95f);
            WriteConfigJSON(m_terrain, data);

            Log("=== Export Complete! ===");
            EditorUtility.DisplayDialog("완료", $"추출 완료!\n경로: {m_exportPath}", "확인");
        }
        catch (System.Exception e)
        {
            Log($"[ERR] {e.Message}");
            Debug.LogException(e);
            EditorUtility.DisplayDialog("오류", e.Message, "확인");
        }
        finally
        {
            EditorUtility.ClearProgressBar();
        }
    }

    // ════════════════════════════════════════════════════════════
    // 1. HEIGHTMAP  →  terrain_heightmap.r16
    // ════════════════════════════════════════════════════════════
    void ExportHeightmap(Terrain terrain, TerrainData data)
    {
        /*
         * 포맷: 16-bit unsigned int, little-endian, 행 우선 저장
         *
         * heights[row, col]:
         *   row 0   = Z = 0 (terrain 근거리)
         *   row N-1 = Z = size.z (terrain 원거리)
         *   col 0   = X = 0 (terrain 왼쪽)
         *   col N-1 = X = size.x (terrain 오른쪽)
         *
         * DX12에서 읽을 때:
         *   worldX = posX + col / (res-1) * sizeX
         *   worldY = posY + val/65535 * sizeY
         *   worldZ = posZ + row / (res-1) * sizeZ
         */

        int res = data.heightmapResolution;  // 항상 2^n + 1 (예: 513, 1025)
        float[,] heights = data.GetHeights(0, 0, res, res);

        byte[] bytes = new byte[res * res * 2];
        int idx = 0;

        for (int row = 0; row < res; row++)
        {
            for (int col = 0; col < res; col++)
            {
                ushort val = (ushort)(Mathf.Clamp01(heights[row, col]) * 65535f);
                bytes[idx++] = (byte)(val & 0xFF);         // low byte (little-endian)
                bytes[idx++] = (byte)((val >> 8) & 0xFF);  // high byte
            }
        }

        string path = Path.Combine(m_exportPath, "terrain_heightmap.r16");
        File.WriteAllBytes(path, bytes);
        Log($"  [OK] Heightmap: {res}×{res}, {bytes.Length / 1024}KB → {Path.GetFileName(path)}");
    }

    // ════════════════════════════════════════════════════════════
    // 2. SPLATMAPS  →  terrain_splatmap0.png, splatmap1.png, ...
    // ════════════════════════════════════════════════════════════
    void ExportSplatmaps(TerrainData data)
    {
        /*
         * Unity alphamaps[row, col, layerIndex]:
         *   레이어 가중치, 각 픽셀에서 모든 레이어 합산 = 1
         *   4레이어마다 RGBA 1장으로 팩
         *
         * Y-flip 이유:
         *   Unity SetPixels → row 0 = 이미지 하단
         *   DX12 로드 시    → row 0 = V=0 = 이미지 상단
         *   alphamap row 0 (Z=0)이 DX12 V=0에 오려면 → 상단에 저장해야 함
         *   → pixels[(res-1-row) * res + col] 에 저장 (Y flip)
         *
         * DX12 샘플링: splatmap.Sample(s, float2(worldX/sizeX, worldZ/sizeZ))
         */

        int layerCount = data.terrainLayers.Length;
        if (layerCount == 0) { Log("  [SKIP] Splatmap: 레이어 없음"); return; }

        int alphaRes   = data.alphamapResolution;
        int splatCount = Mathf.CeilToInt(layerCount / 4f);

        float[,,] alphamaps = data.GetAlphamaps(0, 0, alphaRes, alphaRes);

        for (int s = 0; s < splatCount; s++)
        {
            Texture2D tex = new Texture2D(alphaRes, alphaRes, TextureFormat.RGBA32, false, true);
            Color[] pixels = new Color[alphaRes * alphaRes];

            for (int row = 0; row < alphaRes; row++)
            {
                for (int col = 0; col < alphaRes; col++)
                {
                    int l0 = s * 4 + 0;
                    int l1 = s * 4 + 1;
                    int l2 = s * 4 + 2;
                    int l3 = s * 4 + 3;

                    float r = l0 < layerCount ? alphamaps[row, col, l0] : 0f;
                    float g = l1 < layerCount ? alphamaps[row, col, l1] : 0f;
                    float b = l2 < layerCount ? alphamaps[row, col, l2] : 0f;
                    float a = l3 < layerCount ? alphamaps[row, col, l3] : 0f;

                    // Y flip: alphamap row 0 (Z 근거리) → PNG 상단 → DX12 V=0
                    pixels[(alphaRes - 1 - row) * alphaRes + col] = new Color(r, g, b, a);
                }
            }

            tex.SetPixels(pixels);
            tex.Apply();

            string fname = $"terrain_splatmap{s}.png";
            File.WriteAllBytes(Path.Combine(m_exportPath, fname), tex.EncodeToPNG());
            DestroyImmediate(tex);

            int first = s * 4;
            int last  = Mathf.Min(first + 3, layerCount - 1);
            Log($"  [OK] Splatmap{s}: {alphaRes}×{alphaRes}, layer {first}~{last} (RGBA) → {fname}");
        }
    }

    // ════════════════════════════════════════════════════════════
    // 3. LAYER TEXTURES  →  Textures/layer{i}_diffuse/normal/mask.png
    // ════════════════════════════════════════════════════════════
    void ExportLayerTextures(TerrainData data)
    {
        /*
         * 각 TerrainLayer:
         *   diffuseTexture   : 베이스 컬러 (sRGB)
         *   normalMapTexture : 노멀맵 (Unity DXT5nm → GPU blit으로 복원)
         *   maskMapTexture   : HDRP 마스크 (R=Metallic, G=AO, B=Height, A=Smoothness)
         *
         * 읽기 불가 압축 텍스처 처리:
         *   RenderTexture blit으로 GPU에서 읽어온 뒤 PNG 저장
         */

        TerrainLayer[] layers = data.terrainLayers;
        if (layers == null || layers.Length == 0) { Log("  [SKIP] Textures: 레이어 없음"); return; }

        for (int i = 0; i < layers.Length; i++)
        {
            TerrainLayer layer = layers[i];
            if (layer == null) { Log($"  [WARN] Layer {i} is null"); continue; }

            // Diffuse (sRGB)
            if (layer.diffuseTexture != null)
            {
                string fname = $"layer{i}_diffuse.png";
                ExportTexture(layer.diffuseTexture, Path.Combine(m_exportPath, "Textures", fname), sRGB: true);
                Log($"  [OK] Layer {i} Diffuse: {layer.diffuseTexture.width}×{layer.diffuseTexture.height} → Textures/{fname}");
            }
            else Log($"  [WARN] Layer {i} Diffuse 없음");

            // Normal Map (linear, DXT5nm 압축 해제 필요)
            if (layer.normalMapTexture != null)
            {
                string fname = $"layer{i}_normal.png";
                ExportTexture(layer.normalMapTexture, Path.Combine(m_exportPath, "Textures", fname), sRGB: false);
                Log($"  [OK] Layer {i} Normal: {layer.normalMapTexture.width}×{layer.normalMapTexture.height} → Textures/{fname}");
            }
            else Log($"  [INFO] Layer {i} Normal 없음 (평면 노멀 사용)");

            // Mask Map (linear, HDRP 전용)
            if (layer.maskMapTexture != null)
            {
                string fname = $"layer{i}_mask.png";
                ExportTexture(layer.maskMapTexture, Path.Combine(m_exportPath, "Textures", fname), sRGB: false);
                Log($"  [OK] Layer {i} Mask (Metallic/AO/H/Smooth) → Textures/{fname}");
            }
        }
    }

    /// <summary>
    /// 텍스처를 PNG로 저장. GPU blit으로 읽기 불가 / 압축 텍스처도 처리 가능.
    /// </summary>
    void ExportTexture(Texture2D src, string destPath, bool sRGB)
    {
        // 해상도 제한 (m_maxTexSize == 0 이면 원본 그대로)
        int w = (m_maxTexSize > 0) ? Mathf.Min(src.width,  m_maxTexSize) : src.width;
        int h = (m_maxTexSize > 0) ? Mathf.Min(src.height, m_maxTexSize) : src.height;

        // RenderTextureFormat: sRGB이면 ARGB32(sRGB), linear이면 ARGB32(linear)
        var readWrite = sRGB ? RenderTextureReadWrite.sRGB : RenderTextureReadWrite.Linear;
        RenderTexture rt = RenderTexture.GetTemporary(w, h, 0, RenderTextureFormat.ARGB32, readWrite);
        Graphics.Blit(src, rt);

        RenderTexture prev = RenderTexture.active;
        RenderTexture.active = rt;

        Texture2D readable = new Texture2D(w, h, TextureFormat.RGBA32, false);
        readable.ReadPixels(new Rect(0, 0, w, h), 0, 0);
        readable.Apply();

        RenderTexture.active = prev;
        RenderTexture.ReleaseTemporary(rt);

        File.WriteAllBytes(destPath, readable.EncodeToPNG());
        DestroyImmediate(readable);
    }

    // ════════════════════════════════════════════════════════════
    // 4. TREE INSTANCES  →  terrain_objects.json
    // ════════════════════════════════════════════════════════════
    void ExportTreeInstances(Terrain terrain, TerrainData data)
    {
        /*
         * TreeInstance.position: (0~1, 0~1, 0~1) 정규화 좌표
         * 월드 좌표 변환:
         *   worldX = terrainPos.x + normX * size.x
         *   worldY = terrainPos.y + normY * size.y   ← 나무 발 위치
         *   worldZ = terrainPos.z + normZ * size.z
         *
         * 주의: 나무 메쉬(FBX)는 Unity에서 별도 추출 필요
         *       (FBX Exporter 패키지 또는 수동 export)
         *       terrain_objects.json에는 프리팹 이름과 경로만 기록
         */

        TreePrototype[] protos  = data.treePrototypes;
        TreeInstance[]  insts   = data.treeInstances;
        Vector3         tPos    = terrain.transform.position;
        Vector3         tSize   = data.size;

        var sb = new StringBuilder();
        sb.AppendLine("{");

        // ── 프로토타입 목록 ──
        sb.AppendLine("  \"prototypes\": [");
        for (int i = 0; i < protos.Length; i++)
        {
            TreePrototype p = protos[i];
            string name  = p.prefab != null ? p.prefab.name                           : "unknown";
            string aPath = p.prefab != null ? AssetDatabase.GetAssetPath(p.prefab)    : "";
            bool   last  = (i == protos.Length - 1);

            sb.AppendLine("    {");
            sb.AppendLine($"      \"index\": {i},");
            sb.AppendLine($"      \"name\": \"{EscapeJson(name)}\",");
            sb.AppendLine($"      \"assetPath\": \"{EscapeJson(aPath)}\",");
            sb.AppendLine($"      \"bendFactor\": {p.bendFactor:F3}");
            sb.AppendLine(last ? "    }" : "    },");
        }
        sb.AppendLine("  ],");
        sb.AppendLine();

        // ── 인스턴스 목록 ──
        // 같은 프로토타입끼리 정렬 → DX12 Instanced Rendering 효율
        var sorted = new List<TreeInstance>(insts);
        sorted.Sort((a, b) => a.prototypeIndex.CompareTo(b.prototypeIndex));

        sb.AppendLine("  \"instances\": [");
        for (int i = 0; i < sorted.Count; i++)
        {
            TreeInstance inst = sorted[i];

            // 월드 좌표 변환
            float wx = tPos.x + inst.position.x * tSize.x;
            float wy = tPos.y + inst.position.y * tSize.y;
            float wz = tPos.z + inst.position.z * tSize.z;

            bool last = (i == sorted.Count - 1);

            sb.AppendLine("    {");
            sb.AppendLine($"      \"proto\": {inst.prototypeIndex},");
            sb.AppendLine($"      \"posX\": {wx:F3}, \"posY\": {wy:F3}, \"posZ\": {wz:F3},");
            sb.AppendLine($"      \"rotY\": {inst.rotation:F4},");
            sb.AppendLine($"      \"scaleXZ\": {inst.widthScale:F4},");
            sb.AppendLine($"      \"scaleY\": {inst.heightScale:F4}");
            sb.AppendLine(last ? "    }" : "    },");
        }
        sb.AppendLine("  ]");
        sb.AppendLine("}");

        string path = Path.Combine(m_exportPath, "terrain_objects.json");
        File.WriteAllText(path, sb.ToString(), Encoding.UTF8);
        Log($"  [OK] Trees: {protos.Length} 프로토타입, {insts.Length} 인스턴스 → terrain_objects.json");

        // 프로토타입 이름 미리보기
        for (int i = 0; i < protos.Length; i++)
        {
            int cnt = 0;
            foreach (var inst in insts) if (inst.prototypeIndex == i) cnt++;
            string name = protos[i].prefab != null ? protos[i].prefab.name : "null";
            Log($"    proto[{i}] {name}: {cnt}개");
        }
    }

    // ════════════════════════════════════════════════════════════
    // 5. DETAIL/GRASS DENSITY MAPS  (Phase 2)
    // ════════════════════════════════════════════════════════════
    void ExportDetailMaps(TerrainData data)
    {
        /*
         * Detail layer: int[row, col] 밀도값 (0 = 없음, 최대 수십)
         * R8 grayscale PNG로 저장 (정규화: val / maxDensity)
         * DX12에서 billboard / mesh grass 스폰에 활용
         */

        DetailPrototype[] protos    = data.detailPrototypes;
        int               detailRes = data.detailResolution;

        if (protos == null || protos.Length == 0) { Log("  [SKIP] Detail: 프로토타입 없음"); return; }

        var detailList = new List<string>();

        for (int d = 0; d < protos.Length; d++)
        {
            int[,] density = data.GetDetailLayer(0, 0, detailRes, detailRes, d);

            // 최대값 탐색 (정규화용)
            int maxVal = 1;
            for (int r = 0; r < detailRes; r++)
                for (int c = 0; c < detailRes; c++)
                    if (density[r, c] > maxVal) maxVal = density[r, c];

            Texture2D tex    = new Texture2D(detailRes, detailRes, TextureFormat.R8, false, true);
            Color[]   pixels = new Color[detailRes * detailRes];

            for (int row = 0; row < detailRes; row++)
            {
                for (int col = 0; col < detailRes; col++)
                {
                    float v = density[row, col] / (float)maxVal;
                    // Y flip (splatmap과 동일)
                    pixels[(detailRes - 1 - row) * detailRes + col] = new Color(v, 0f, 0f, 1f);
                }
            }

            tex.SetPixels(pixels);
            tex.Apply();

            DetailPrototype p    = protos[d];
            string          pName = p.prototype         != null ? p.prototype.name
                                  : p.prototypeTexture  != null ? p.prototypeTexture.name
                                  : $"detail_{d}";
            string fname = $"detail_{d}_{SanitizeFileName(pName)}.png";
            File.WriteAllBytes(Path.Combine(m_exportPath, fname), tex.EncodeToPNG());
            DestroyImmediate(tex);
            detailList.Add(fname);

            Log($"  [OK] Detail[{d}] {pName}: {detailRes}×{detailRes}, maxDensity={maxVal} → {fname}");
        }

        // detail 목록 JSON 기록
        var sb = new StringBuilder();
        sb.AppendLine("{");
        sb.AppendLine("  \"detailResolution\": " + detailRes + ",");
        sb.AppendLine("  \"detailPrototypes\": [");
        for (int d = 0; d < protos.Length; d++)
        {
            DetailPrototype p     = protos[d];
            bool            last  = (d == protos.Length - 1);
            string          pName = p.prototype != null ? p.prototype.name
                                  : p.prototypeTexture != null ? p.prototypeTexture.name : "";
            string renderMode = p.renderMode.ToString();

            sb.AppendLine("    {");
            sb.AppendLine($"      \"index\": {d},");
            sb.AppendLine($"      \"name\": \"{EscapeJson(pName)}\",");
            sb.AppendLine($"      \"densityMap\": \"{detailList[d]}\",");
            sb.AppendLine($"      \"renderMode\": \"{renderMode}\",");
            sb.AppendLine($"      \"minWidth\": {p.minWidth:F3}, \"maxWidth\": {p.maxWidth:F3},");
            sb.AppendLine($"      \"minHeight\": {p.minHeight:F3}, \"maxHeight\": {p.maxHeight:F3},");
            sb.AppendLine($"      \"noiseSpread\": {p.noiseSpread:F3}");
            sb.AppendLine(last ? "    }" : "    },");
        }
        sb.AppendLine("  ]");
        sb.AppendLine("}");

        File.WriteAllText(Path.Combine(m_exportPath, "terrain_details.json"), sb.ToString(), Encoding.UTF8);
    }

    // ════════════════════════════════════════════════════════════
    // 6. CONFIG JSON  →  terrain_config.json  (항상 마지막에 작성)
    // ════════════════════════════════════════════════════════════
    void WriteConfigJSON(Terrain terrain, TerrainData data)
    {
        Vector3 pos   = terrain.transform.position;
        Vector3 size  = data.size;
        int     layers = data.terrainLayers.Length;
        int     splats = layers > 0 ? Mathf.CeilToInt(layers / 4f) : 0;

        var sb = new StringBuilder();
        sb.AppendLine("{");
        sb.AppendLine("  \"version\": 1,");
        sb.AppendLine();

        // ── Terrain 기본 정보 ──
        sb.AppendLine("  \"terrain\": {");
        sb.AppendLine($"    \"name\": \"{EscapeJson(terrain.name)}\",");
        sb.AppendLine($"    \"posX\": {pos.x:F3},  \"posY\": {pos.y:F3},  \"posZ\": {pos.z:F3},");
        sb.AppendLine($"    \"sizeX\": {size.x:F3}, \"sizeY\": {size.y:F3}, \"sizeZ\": {size.z:F3},");
        sb.AppendLine($"    \"heightmapResolution\": {data.heightmapResolution},");
        sb.AppendLine($"    \"alphamapResolution\":  {data.alphamapResolution},");
        sb.AppendLine($"    \"detailResolution\":    {data.detailResolution},");
        sb.AppendLine($"    \"heightmapFile\": \"terrain_heightmap.r16\",");
        sb.AppendLine($"    \"layerCount\": {layers},");
        sb.AppendLine($"    \"splatmapCount\": {splats}");
        sb.AppendLine("  },");
        sb.AppendLine();

        // ── 레이어 정보 ──
        sb.AppendLine("  \"layers\": [");
        TerrainLayer[] layerArr = data.terrainLayers;
        for (int i = 0; i < layerArr.Length; i++)
        {
            TerrainLayer l    = layerArr[i];
            bool         last = (i == layerArr.Length - 1);

            string diffuse = l.diffuseTexture    != null ? $"Textures/layer{i}_diffuse.png" : "";
            string normal  = l.normalMapTexture  != null ? $"Textures/layer{i}_normal.png"  : "";
            string mask    = l.maskMapTexture    != null ? $"Textures/layer{i}_mask.png"    : "";

            sb.AppendLine("    {");
            sb.AppendLine($"      \"index\": {i},");
            sb.AppendLine($"      \"name\": \"{EscapeJson(l.name ?? $"Layer{i}")}\",");
            sb.AppendLine($"      \"diffuse\": \"{diffuse}\",");
            sb.AppendLine($"      \"normal\":  \"{normal}\",");
            sb.AppendLine($"      \"mask\":    \"{mask}\",");
            sb.AppendLine($"      \"tileSizeX\":   {l.tileSize.x:F3},");
            sb.AppendLine($"      \"tileSizeZ\":   {l.tileSize.y:F3},");
            sb.AppendLine($"      \"tileOffsetX\": {l.tileOffset.x:F3},");
            sb.AppendLine($"      \"tileOffsetZ\": {l.tileOffset.y:F3},");
            sb.AppendLine($"      \"metallic\":    {l.metallic:F3},");
            sb.AppendLine($"      \"smoothness\":  {l.smoothness:F3},");
            // DX12에서 바로 쓸 수 있도록: 몇 번째 splatmap의 어떤 채널인지
            sb.AppendLine($"      \"splatmapIndex\":   {i / 4},");
            sb.AppendLine($"      \"splatmapChannel\": {i % 4}");
            sb.AppendLine(last ? "    }" : "    },");
        }
        sb.AppendLine("  ],");
        sb.AppendLine();

        // ── Splatmap 파일 목록 ──
        sb.AppendLine("  \"splatmaps\": [");
        for (int s = 0; s < splats; s++)
            sb.AppendLine(s < splats - 1 ? $"    \"terrain_splatmap{s}.png\"," : $"    \"terrain_splatmap{s}.png\"");
        sb.AppendLine("  ],");
        sb.AppendLine();

        // ── 나무/오브젝트 파일 ──
        sb.AppendLine("  \"objectsFile\": \"terrain_objects.json\",");
        sb.AppendLine("  \"detailsFile\": \"terrain_details.json\",");
        sb.AppendLine();

        // ── 좌표계 메모 (DX12 구현 시 참고) ──
        sb.AppendLine("  \"coordinateSystem\": {");
        sb.AppendLine("    \"axisConversion\": \"None (Unity Left-hand Y-up = DX12 Left-hand Y-up)\",");
        sb.AppendLine("    \"heightmapRowOrder\": \"row0=nearZ(small Z), colN=farX(large X)\",");
        sb.AppendLine("    \"splatmapYFlip\": true,");
        sb.AppendLine("    \"splatmapUVFormula\": \"U=worldX/sizeX, V=worldZ/sizeZ\",");
        sb.AppendLine("    \"worldHeightFormula\": \"worldY = posY + (raw/65535.0) * sizeY\",");
        sb.AppendLine("    \"worldXFormula\": \"worldX = posX + col/(res-1) * sizeX\",");
        sb.AppendLine("    \"worldZFormula\": \"worldZ = posZ + row/(res-1) * sizeZ\"");
        sb.AppendLine("  }");

        sb.AppendLine("}");

        string path = Path.Combine(m_exportPath, "terrain_config.json");
        File.WriteAllText(path, sb.ToString(), Encoding.UTF8);
        Log($"  [OK] Config JSON → {Path.GetFileName(path)}");
    }

    // ── 유틸리티 ─────────────────────────────────────────────────
    void Log(string msg)
    {
        m_log += msg + "\n";
        Debug.Log("[TerrainExporter] " + msg);
        Repaint();
    }

    static string EscapeJson(string s)
    {
        return s?.Replace("\\", "\\\\").Replace("\"", "\\\"") ?? "";
    }

    static string SanitizeFileName(string s)
    {
        foreach (char c in Path.GetInvalidFileNameChars())
            s = s.Replace(c, '_');
        return s;
    }

    static void DrawLine()
    {
        var rect = EditorGUILayout.GetControlRect(false, 1);
        EditorGUI.DrawRect(rect, new Color(0.5f, 0.5f, 0.5f, 1f));
        EditorGUILayout.Space(4);
    }
}
