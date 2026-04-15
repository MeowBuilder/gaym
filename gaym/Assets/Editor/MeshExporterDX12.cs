// MeshExporterDX12.cs
// Place in Assets/Editor/ inside your Unity HDRP project.
// Exports a skinned (or static) mesh + all animations to the .bin format
// read by MeshLoader.cpp and AnimationSet::LoadAnimationFromFile().
//
// Usage:
//   Select the root GameObject in the Hierarchy, then
//   Tools > DX12 Exporter > Export Mesh  (creates <name>_mesh.bin)
//   Tools > DX12 Exporter > Export Animations (creates <name>_anim.bin)
//   Tools > DX12 Exporter > Export All (both at once)
//
// Output files land in Assets/ExportedBins/.

using System;
using System.Collections.Generic;
using System.IO;
using UnityEditor;
using UnityEngine;

public static class MeshExporterDX12
{
    private const string OutDir = "Assets/ExportedBins";

    // ──────────────────────────────────────────────────────────────────
    // Menu items
    // ──────────────────────────────────────────────────────────────────
    [MenuItem("Tools/DX12 Exporter/Export Mesh")]
    public static void ExportMeshMenu()
    {
        var go = Selection.activeGameObject;
        if (go == null) { EditorUtility.DisplayDialog("DX12 Exporter", "Select a GameObject first.", "OK"); return; }
        ExportMesh(go);
    }

    [MenuItem("Tools/DX12 Exporter/Export Animations")]
    public static void ExportAnimationsMenu()
    {
        var go = Selection.activeGameObject;
        if (go == null) { EditorUtility.DisplayDialog("DX12 Exporter", "Select a GameObject first.", "OK"); return; }
        ExportAnimations(go);
    }

    [MenuItem("Tools/DX12 Exporter/Export All")]
    public static void ExportAllMenu()
    {
        var go = Selection.activeGameObject;
        if (go == null) { EditorUtility.DisplayDialog("DX12 Exporter", "Select a GameObject first.", "OK"); return; }
        ExportMesh(go);
        ExportAnimations(go);
    }

    [MenuItem("Tools/DX12 Exporter/Dump to TXT (Debug)")]
    public static void DumpToTxtMenu()
    {
        var go = Selection.activeGameObject;
        if (go == null) { EditorUtility.DisplayDialog("DX12 Exporter", "Select a GameObject first.", "OK"); return; }
        DumpMeshTxt(go);
        DumpAnimTxt(go);
    }

    // ──────────────────────────────────────────────────────────────────
    // Mesh export
    // ──────────────────────────────────────────────────────────────────
    public static void ExportMesh(GameObject root)
    {
        Directory.CreateDirectory(OutDir);
        string path = $"{OutDir}/{root.name}_mesh.bin";

        using var fs = File.Open(path, FileMode.Create);
        using var bw = new BinaryWriter(fs);

        // <Hierarchy>:
        Write7BitString(bw, "<Hierarchy>:");
        WriteFrame(bw, root, BuildBoneMap(root));
        // </Hierarchy>
        Write7BitString(bw, "</Hierarchy>");

        Debug.Log($"[DX12 Exporter] Mesh → {path}");
        AssetDatabase.Refresh();
        EditorUtility.DisplayDialog("DX12 Exporter", $"Mesh exported:\n{path}", "OK");
    }

    // ──────────────────────────────────────────────────────────────────
    // Animation export
    // ──────────────────────────────────────────────────────────────────
    public static void ExportAnimations(GameObject root)
    {
        var animator = root.GetComponentInChildren<Animator>();
        if (animator == null)
        {
            EditorUtility.DisplayDialog("DX12 Exporter", "No Animator found on the selected object.", "OK");
            return;
        }

        // Gather AnimationClips from the AnimatorController
        var clips = GetAllClips(animator);
        if (clips.Count == 0)
        {
            EditorUtility.DisplayDialog("DX12 Exporter", "No AnimationClips found in the Animator.", "OK");
            return;
        }

        Directory.CreateDirectory(OutDir);
        string path = $"{OutDir}/{root.name}_anim.bin";

        using var fs = File.Open(path, FileMode.Create);
        using var bw = new BinaryWriter(fs);

        // <ClipCount>: [int]
        Write7BitString(bw, "<ClipCount>:");
        bw.Write(clips.Count);

        // Collect all transform paths once (bone ordering must match mesh export)
        var boneMap = BuildBoneMap(root);

        foreach (var clip in clips)
        {
            WriteClip(bw, clip, root, boneMap);
        }

        Debug.Log($"[DX12 Exporter] Animations ({clips.Count} clips) → {path}");
        AssetDatabase.Refresh();
        EditorUtility.DisplayDialog("DX12 Exporter", $"Animations exported:\n{path}\n{clips.Count} clips.", "OK");
    }

    // ──────────────────────────────────────────────────────────────────
    // Frame recursive writer (matches MeshLoader::LoadFrameHierarchyFromFile)
    // ──────────────────────────────────────────────────────────────────
    static int s_frameIndex = 0;

    static void WriteFrame(BinaryWriter bw, GameObject go, Dictionary<string, int> boneMap)
    {
        // <Frame>: [int frameIdx] [string name]
        Write7BitString(bw, "<Frame>:");
        bw.Write(s_frameIndex++);
        Write7BitString(bw, go.name);

        // <Transform>: pos(3f) euler(3f) scale(3f) quat(4f)
        Write7BitString(bw, "<Transform>:");
        var t = go.transform;
        WriteVector3(bw, t.localPosition);
        WriteVector3(bw, t.localEulerAngles);
        WriteVector3(bw, t.localScale);
        WriteVector4(bw, new Vector4(t.localRotation.x, t.localRotation.y, t.localRotation.z, t.localRotation.w));

        // <TransformMatrix>: 16f (row-major 4×4)
        Write7BitString(bw, "<TransformMatrix>:");
        WriteMatrix(bw, Matrix4x4.TRS(t.localPosition, t.localRotation, t.localScale));

        // Mesh?
        var smr = go.GetComponent<SkinnedMeshRenderer>();
        var mf  = go.GetComponent<MeshFilter>();
        Mesh mesh = smr != null ? smr.sharedMesh : (mf != null ? mf.sharedMesh : null);

        if (mesh != null)
        {
            Write7BitString(bw, "<Mesh>:");
            WriteMesh(bw, mesh, smr, boneMap);

            // <Materials>:
            Renderer rend = smr != null ? (Renderer)smr : go.GetComponent<MeshRenderer>();
            if (rend != null)
            {
                Write7BitString(bw, "<Materials>:");
                WriteMaterials(bw, rend.sharedMaterials, OutDir);
            }
        }

        // <Children>: [int count]  (recursive)
        int childCount = go.transform.childCount;
        Write7BitString(bw, "<Children>:");
        bw.Write(childCount);
        for (int i = 0; i < childCount; i++)
            WriteFrame(bw, go.transform.GetChild(i).gameObject, boneMap);

        Write7BitString(bw, "</Frame>");
    }

    // ──────────────────────────────────────────────────────────────────
    // Mesh writer (matches MeshLoader::LoadMeshInfoFromFile)
    // ──────────────────────────────────────────────────────────────────
    static void WriteMesh(BinaryWriter bw, Mesh mesh, SkinnedMeshRenderer smr, Dictionary<string, int> boneMap)
    {
        int vertCount = mesh.vertexCount;

        // [int vertCount] [string meshName]
        bw.Write(vertCount);
        Write7BitString(bw, mesh.name);

        // <Bounds>: center(3f) extents(3f)
        Write7BitString(bw, "<Bounds>:");
        WriteVector3(bw, mesh.bounds.center);
        WriteVector3(bw, mesh.bounds.extents);

        // <Positions>: [int count] [float3 * count]
        var verts = mesh.vertices;
        Write7BitString(bw, "<Positions>:");
        bw.Write(vertCount);
        foreach (var v in verts) WriteVector3(bw, v);

        // <Normals>:
        var normals = mesh.normals;
        if (normals != null && normals.Length == vertCount)
        {
            Write7BitString(bw, "<Normals>:");
            bw.Write(vertCount);
            foreach (var n in normals) WriteVector3(bw, n);
        }

        // <TexCoords>: Y-flip to match loader (loader does 1-v on read)
        var uvs = mesh.uv;
        if (uvs != null && uvs.Length == vertCount)
        {
            Write7BitString(bw, "<TexCoords>:");
            bw.Write(vertCount);
            foreach (var uv in uvs)
            {
                bw.Write(uv.x);
                bw.Write(uv.y);   // loader flips (1-y) on read: Unity(Y-up) → DX12(Y-down)
            }
        }

        // <BoneWeights>:  (only for skinned meshes)
        if (smr != null)
        {
            var boneWeights = mesh.boneWeights;
            if (boneWeights != null && boneWeights.Length == vertCount)
            {
                Write7BitString(bw, "<BoneWeights>:");
                bw.Write(vertCount);
                foreach (var bw4 in boneWeights)
                {
                    bw.Write(bw4.boneIndex0); bw.Write(bw4.weight0);
                    bw.Write(bw4.boneIndex1); bw.Write(bw4.weight1);
                    bw.Write(bw4.boneIndex2); bw.Write(bw4.weight2);
                    bw.Write(bw4.boneIndex3); bw.Write(bw4.weight3);
                }
            }

            // <BindPoses>: [int count] [matrix * count]
            var bindPoses = mesh.bindposes;
            if (bindPoses != null && bindPoses.Length > 0)
            {
                Write7BitString(bw, "<BindPoses>:");
                bw.Write(bindPoses.Length);
                foreach (var bp in bindPoses) WriteMatrix(bw, bp);
            }

            // <BoneNames>: [int count] [string * count]
            if (smr.bones != null && smr.bones.Length > 0)
            {
                Write7BitString(bw, "<BoneNames>:");
                bw.Write(smr.bones.Length);
                foreach (var bone in smr.bones)
                    Write7BitString(bw, bone != null ? bone.name : "");
            }
        }

        // <Indices>: flat triangle list
        var tris = mesh.triangles;
        Write7BitString(bw, "<Indices>:");
        bw.Write(tris.Length);
        foreach (var idx in tris) bw.Write(idx);

        // <SubMeshes>: multiple material slots
        if (mesh.subMeshCount > 1)
        {
            Write7BitString(bw, "<SubMeshes>:");
            bw.Write(mesh.subMeshCount);
            for (int s = 0; s < mesh.subMeshCount; s++)
            {
                Write7BitString(bw, "<SubMesh>:");
                bw.Write(s);
                var subTris = mesh.GetTriangles(s);
                bw.Write(subTris.Length);
                foreach (var idx in subTris) bw.Write((uint)idx);
            }
        }

        Write7BitString(bw, "</Mesh>");
    }

    // ──────────────────────────────────────────────────────────────────
    // Materials writer  (matches MeshLoader::LoadMaterialsInfoFromFile)
    // Handles Standard, URP/Lit, HDRP/Lit, HDRP/LayeredLit
    // ──────────────────────────────────────────────────────────────────
    static void WriteMaterials(BinaryWriter bw, Material[] mats, string outDir)
    {
        bw.Write(mats.Length);

        string texOutDir = Path.Combine(outDir, "Textures");
        Directory.CreateDirectory(texOutDir);

        for (int i = 0; i < mats.Length; i++)
        {
            var mat = mats[i];

            Write7BitString(bw, "<Material>:");
            bw.Write(i);

            if (mat == null) continue;

            // Albedo color
            Color albedo = Color.white;
            if      (mat.HasProperty("_BaseColor"))     albedo = mat.GetColor("_BaseColor");
            else if (mat.HasProperty("_Color"))         albedo = mat.GetColor("_Color");
            else if (mat.HasProperty("_BaseColorMap"))  albedo = Color.white;
            Write7BitString(bw, "<AlbedoColor>:");
            bw.Write(albedo.r); bw.Write(albedo.g); bw.Write(albedo.b); bw.Write(albedo.a);

            // Albedo texture
            Texture2D albedoTex = null;
            if      (mat.HasProperty("_BaseColorMap")) albedoTex = mat.GetTexture("_BaseColorMap") as Texture2D;
            else if (mat.HasProperty("_BaseMap"))      albedoTex = mat.GetTexture("_BaseMap")      as Texture2D;
            else if (mat.HasProperty("_MainTex"))      albedoTex = mat.GetTexture("_MainTex")      as Texture2D;

            if (albedoTex != null)
            {
                string texName = ExportTexturePNG(albedoTex, texOutDir);
                Write7BitString(bw, "<AlbedoMap>:");
                Write7BitString(bw, texName);
            }

            // Emissive
            Color emissive = Color.black;
            if (mat.HasProperty("_EmissiveColor")) emissive = mat.GetColor("_EmissiveColor");
            else if (mat.HasProperty("_EmissionColor")) emissive = mat.GetColor("_EmissionColor");
            Write7BitString(bw, "<EmissiveColor>:");
            bw.Write(emissive.r); bw.Write(emissive.g); bw.Write(emissive.b); bw.Write(emissive.a);

            // Smoothness / Metallic
            float smoothness = mat.HasProperty("_Smoothness") ? mat.GetFloat("_Smoothness") :
                               mat.HasProperty("_Glossiness") ? mat.GetFloat("_Glossiness") : 0.5f;
            Write7BitString(bw, "<Smoothness>:");
            bw.Write(smoothness);

            float metallic = mat.HasProperty("_Metallic") ? mat.GetFloat("_Metallic") : 0f;
            Write7BitString(bw, "<Metallic>:");
            bw.Write(metallic);
        }

        Write7BitString(bw, "</Materials>");
    }

    // ──────────────────────────────────────────────────────────────────
    // Animation clip writer
    // ──────────────────────────────────────────────────────────────────
    static void WriteClip(BinaryWriter bw, AnimationClip clip, GameObject root, Dictionary<string, int> boneMap)
    {
        float frameRate   = clip.frameRate > 0f ? clip.frameRate : 30f;
        float duration    = clip.length;
        int   totalFrames = Mathf.RoundToInt(duration * frameRate) + 1;

        // <Clip>: [string name]
        Write7BitString(bw, "<Clip>:");
        Write7BitString(bw, clip.name);

        // <Duration>: [float]
        Write7BitString(bw, "<Duration>:");
        bw.Write(duration);

        // <FrameRate>: [float]
        Write7BitString(bw, "<FrameRate>:");
        bw.Write(frameRate);

        // <TotalFrames>: [int]
        Write7BitString(bw, "<TotalFrames>:");
        bw.Write(totalFrames);

        // Group bindings by bone path
        var bindings = AnimationUtility.GetCurveBindings(clip);
        var trackMap = new Dictionary<string, TrackData>();

        foreach (var binding in bindings)
        {
            if (binding.type != typeof(Transform)) continue;
            if (!trackMap.TryGetValue(binding.path, out var track))
            {
                track = new TrackData();
                trackMap[binding.path] = track;
            }

            var curve = AnimationUtility.GetEditorCurve(clip, binding);
            string prop = binding.propertyName;

            // Collect times
            foreach (var key in curve.keys)
            {
                int frameIdx = Mathf.RoundToInt(key.time * frameRate);
                if (!track.frames.ContainsKey(frameIdx))
                    track.frames[frameIdx] = new FrameData();
            }

            foreach (var key in curve.keys)
            {
                int frameIdx = Mathf.RoundToInt(key.time * frameRate);
                var fd = track.frames[frameIdx];

                if      (prop == "m_LocalPosition.x") { fd.pos.x   = key.value; }
                else if (prop == "m_LocalPosition.y") { fd.pos.y   = key.value; }
                else if (prop == "m_LocalPosition.z") { fd.pos.z   = key.value; }
                else if (prop == "m_LocalRotation.x") { fd.rot.x   = key.value; }
                else if (prop == "m_LocalRotation.y") { fd.rot.y   = key.value; }
                else if (prop == "m_LocalRotation.z") { fd.rot.z   = key.value; }
                else if (prop == "m_LocalRotation.w") { fd.rot.w   = key.value; }
                else if (prop == "localEulerAnglesRaw.x") { fd.eulerSet = true; fd.euler.x = key.value; }
                else if (prop == "localEulerAnglesRaw.y") { fd.eulerSet = true; fd.euler.y = key.value; }
                else if (prop == "localEulerAnglesRaw.z") { fd.eulerSet = true; fd.euler.z = key.value; }
                else if (prop == "m_LocalScale.x")    { fd.scale.x = key.value; }
                else if (prop == "m_LocalScale.y")    { fd.scale.y = key.value; }
                else if (prop == "m_LocalScale.z")    { fd.scale.z = key.value; }
            }
        }

        // <KeyframeTracks>: [int trackCount]
        Write7BitString(bw, "<KeyframeTracks>:");
        bw.Write(trackMap.Count);

        foreach (var kv in trackMap)
        {
            string bonePath = kv.Key;
            string boneName = bonePath.Contains("/") ? bonePath.Substring(bonePath.LastIndexOf('/') + 1) : bonePath;

            Write7BitString(bw, "<TrackBoneName>:");
            Write7BitString(bw, boneName);

            var sortedFrames = new List<int>(kv.Value.frames.Keys);
            sortedFrames.Sort();

            Write7BitString(bw, "<Keyframes>:");
            bw.Write(sortedFrames.Count);

            foreach (int fi in sortedFrames)
            {
                var fd = kv.Value.frames[fi];

                // If euler angles were recorded, convert to quaternion
                if (fd.eulerSet)
                {
                    var q = Quaternion.Euler(fd.euler.x, fd.euler.y, fd.euler.z);
                    fd.rot = new Vector4(q.x, q.y, q.z, q.w);
                }

                bw.Write(fi);
                WriteVector3(bw, fd.pos);
                WriteVector4(bw, fd.rot);
                WriteVector3(bw, fd.scale.x == 0 && fd.scale.y == 0 && fd.scale.z == 0
                    ? Vector3.one : fd.scale);
            }
        }

        Write7BitString(bw, "</Clip>");
    }

    // ──────────────────────────────────────────────────────────────────
    // Helpers
    // ──────────────────────────────────────────────────────────────────

    // Build a name→index map for all bones in the hierarchy
    static Dictionary<string, int> BuildBoneMap(GameObject root)
    {
        var map = new Dictionary<string, int>();
        int idx = 0;
        foreach (var t in root.GetComponentsInChildren<Transform>(true))
        {
            if (!map.ContainsKey(t.name))
                map[t.name] = idx++;
        }
        return map;
    }

    static List<AnimationClip> GetAllClips(Animator animator)
    {
        var clips = new List<AnimationClip>();
        if (animator.runtimeAnimatorController == null) return clips;

        foreach (var clip in animator.runtimeAnimatorController.animationClips)
        {
            if (clip != null && !clips.Contains(clip))
                clips.Add(clip);
        }
        return clips;
    }

    // Export texture as PNG to outDir, return filename only
    static string ExportTexturePNG(Texture2D tex, string outDir)
    {
        string texName = tex.name + ".png";
        string fullPath = Path.Combine(outDir, texName);

        try
        {
            // Make readable copy
            var rt = RenderTexture.GetTemporary(tex.width, tex.height, 0, RenderTextureFormat.ARGB32);
            Graphics.Blit(tex, rt);
            var prev = RenderTexture.active;
            RenderTexture.active = rt;
            var readable = new Texture2D(tex.width, tex.height, TextureFormat.ARGB32, false);
            readable.ReadPixels(new Rect(0, 0, tex.width, tex.height), 0, 0);
            readable.Apply();
            RenderTexture.active = prev;
            RenderTexture.ReleaseTemporary(rt);

            File.WriteAllBytes(fullPath, readable.EncodeToPNG());
            UnityEngine.Object.DestroyImmediate(readable);
        }
        catch (Exception e)
        {
            Debug.LogWarning($"[DX12 Exporter] Could not export texture '{tex.name}': {e.Message}");
        }

        return texName;
    }

    // C# BinaryWriter.Write7BitEncodedInt compatible
    static void Write7BitString(BinaryWriter bw, string s)
    {
        byte[] bytes = System.Text.Encoding.UTF8.GetBytes(s);
        int len = bytes.Length;
        // Write 7-bit encoded length
        while (len >= 0x80)
        {
            bw.Write((byte)(len | 0x80));
            len >>= 7;
        }
        bw.Write((byte)len);
        bw.Write(bytes);
    }

    static void WriteVector3(BinaryWriter bw, Vector3 v)
    {
        bw.Write(v.x); bw.Write(v.y); bw.Write(v.z);
    }

    static void WriteVector4(BinaryWriter bw, Vector4 v)
    {
        bw.Write(v.x); bw.Write(v.y); bw.Write(v.z); bw.Write(v.w);
    }

    // Unity는 Column-major(열 우선), DX12 XMFLOAT4X4는 Row-major(행 우선)
    // → Transpose해서 써야 DX12 엔진에서 올바르게 읽힘
    static void WriteMatrix(BinaryWriter bw, Matrix4x4 m)
    {
        bw.Write(m.m00); bw.Write(m.m10); bw.Write(m.m20); bw.Write(m.m30);
        bw.Write(m.m01); bw.Write(m.m11); bw.Write(m.m21); bw.Write(m.m31);
        bw.Write(m.m02); bw.Write(m.m12); bw.Write(m.m22); bw.Write(m.m32);
        bw.Write(m.m03); bw.Write(m.m13); bw.Write(m.m23); bw.Write(m.m33);
    }

    // ──────────────────────────────────────────────────────────────────
    // Helper data classes
    // ──────────────────────────────────────────────────────────────────
    class FrameData
    {
        public Vector3 pos   = Vector3.zero;
        public Vector4 rot   = new Vector4(0, 0, 0, 1);   // identity quat
        public Vector3 scale = Vector3.one;
        public Vector3 euler = Vector3.zero;
        public bool eulerSet = false;
    }

    class TrackData
    {
        public Dictionary<int, FrameData> frames = new Dictionary<int, FrameData>();
    }

    // ──────────────────────────────────────────────────────────────────
    // TXT Dump (human-readable debug output)
    // ──────────────────────────────────────────────────────────────────
    static void DumpMeshTxt(GameObject root)
    {
        Directory.CreateDirectory(OutDir);
        string path = $"{OutDir}/{root.name}_mesh_dump.txt";
        using var sw = new StreamWriter(path);

        sw.WriteLine($"=== MESH DUMP: {root.name} ===");
        sw.WriteLine($"Generated: {DateTime.Now}");
        sw.WriteLine();
        DumpFrameTxt(sw, root, 0);

        Debug.Log($"[DX12 Exporter] Mesh dump → {path}");
        AssetDatabase.Refresh();
    }

    static void DumpFrameTxt(StreamWriter sw, GameObject go, int depth)
    {
        string indent = new string(' ', depth * 2);
        sw.WriteLine($"{indent}<Frame> [{go.name}]");

        var t = go.transform;
        sw.WriteLine($"{indent}  pos={t.localPosition} rot={t.localEulerAngles} scale={t.localScale}");

        // Mesh info
        var smr = go.GetComponent<SkinnedMeshRenderer>();
        var mf  = go.GetComponent<MeshFilter>();
        Mesh mesh = smr != null ? smr.sharedMesh : (mf != null ? mf.sharedMesh : null);

        if (mesh != null)
        {
            sw.WriteLine($"{indent}  <Mesh> name={mesh.name}");
            sw.WriteLine($"{indent}    vertices    : {mesh.vertexCount}");
            sw.WriteLine($"{indent}    triangles   : {mesh.triangles.Length / 3} tris ({mesh.triangles.Length} indices)");
            sw.WriteLine($"{indent}    normals     : {mesh.normals.Length}");
            sw.WriteLine($"{indent}    uvs         : {mesh.uv.Length}");
            sw.WriteLine($"{indent}    subMeshes   : {mesh.subMeshCount}");
            sw.WriteLine($"{indent}    bounds      : center={mesh.bounds.center} extents={mesh.bounds.extents}");

            if (smr != null)
            {
                sw.WriteLine($"{indent}    bones       : {(smr.bones != null ? smr.bones.Length : 0)}");
                sw.WriteLine($"{indent}    bindPoses   : {mesh.bindposes.Length}");
                bool hasBW = mesh.boneWeights != null && mesh.boneWeights.Length > 0;
                sw.WriteLine($"{indent}    boneWeights : {(hasBW ? mesh.boneWeights.Length.ToString() : "none")}");

                if (smr.bones != null && smr.bones.Length > 0)
                {
                    sw.WriteLine($"{indent}    boneNames:");
                    for (int i = 0; i < smr.bones.Length; i++)
                        sw.WriteLine($"{indent}      [{i:D3}] {(smr.bones[i] != null ? smr.bones[i].name : "(null)")}");
                }
            }

            // Materials
            Renderer rend = smr != null ? (Renderer)smr : go.GetComponent<MeshRenderer>();
            if (rend != null && rend.sharedMaterials != null)
            {
                sw.WriteLine($"{indent}    materials   : {rend.sharedMaterials.Length}");
                foreach (var mat in rend.sharedMaterials)
                {
                    if (mat == null) { sw.WriteLine($"{indent}      (null)"); continue; }
                    sw.WriteLine($"{indent}      [{mat.name}] shader={mat.shader.name}");

                    // Texture props
                    string[] texProps = { "_BaseColorMap", "_BaseMap", "_MainTex", "_NormalMap", "_MaskMap" };
                    foreach (var prop in texProps)
                    {
                        if (mat.HasProperty(prop))
                        {
                            var tex = mat.GetTexture(prop);
                            sw.WriteLine($"{indent}        {prop} = {(tex != null ? tex.name : "(none)")}");
                        }
                    }
                }
            }
        }

        // Children
        int childCount = go.transform.childCount;
        if (childCount > 0)
        {
            sw.WriteLine($"{indent}  <Children> count={childCount}");
            for (int i = 0; i < childCount; i++)
                DumpFrameTxt(sw, go.transform.GetChild(i).gameObject, depth + 1);
        }

        sw.WriteLine($"{indent}</Frame>");
    }

    static void DumpAnimTxt(GameObject root)
    {
        var animator = root.GetComponentInChildren<Animator>();
        if (animator == null || animator.runtimeAnimatorController == null)
        {
            Debug.LogWarning("[DX12 Exporter] No Animator/Controller found for anim dump.");
            return;
        }

        Directory.CreateDirectory(OutDir);
        string path = $"{OutDir}/{root.name}_anim_dump.txt";
        using var sw = new StreamWriter(path);

        sw.WriteLine($"=== ANIMATION DUMP: {root.name} ===");
        sw.WriteLine($"Generated: {DateTime.Now}");
        sw.WriteLine();

        var clips = GetAllClips(animator);
        sw.WriteLine($"Total clips: {clips.Count}");
        sw.WriteLine();

        for (int ci = 0; ci < clips.Count; ci++)
        {
            var clip = clips[ci];
            float fps = clip.frameRate > 0 ? clip.frameRate : 30f;
            int totalFrames = Mathf.RoundToInt(clip.length * fps) + 1;

            sw.WriteLine($"[{ci:D2}] Clip: \"{clip.name}\"");
            sw.WriteLine($"     Duration   : {clip.length:F4}s");
            sw.WriteLine($"     FrameRate  : {fps}");
            sw.WriteLine($"     TotalFrames: {totalFrames}");

            // Count tracks
            var bindings = AnimationUtility.GetCurveBindings(clip);
            var bonePaths = new HashSet<string>();
            foreach (var b in bindings)
                if (b.type == typeof(Transform)) bonePaths.Add(b.path);

            sw.WriteLine($"     BoneTracks : {bonePaths.Count}");

            foreach (var bp in bonePaths)
            {
                string boneName = bp.Contains("/") ? bp.Substring(bp.LastIndexOf('/') + 1) : bp;
                // Count keyframes (use position.x as representative)
                var posBinding = new UnityEditor.EditorCurveBinding
                    { path = bp, type = typeof(Transform), propertyName = "m_LocalPosition.x" };
                var curve = AnimationUtility.GetEditorCurve(clip, posBinding);
                int kfCount = curve != null ? curve.keys.Length : 0;
                sw.WriteLine($"       bone: {boneName,-40} keyframes: {kfCount}");
            }
            sw.WriteLine();
        }

        Debug.Log($"[DX12 Exporter] Anim dump → {path}");
        AssetDatabase.Refresh();
    }
}
