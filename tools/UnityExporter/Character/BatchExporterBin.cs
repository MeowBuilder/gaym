using System.Collections.Generic;
using UnityEngine;
using UnityEditor;
using System.IO;

public class BatchExporterBin : Editor
{
    private static BinaryWriter binaryWriter = null;
    private static int m_nFrames = 0;
    private static string s_textureExportDir = "";
    private static Dictionary<string, string> s_exportedTextures = new Dictionary<string, string>();

    // ─────────────────────────────────────────────
    //  Menu Item
    // ─────────────────────────────────────────────

    [MenuItem("Tools/Batch Export Mesh + Animation to Binary (.bin)")]
    static void BatchExportMeshAndAnimation()
    {
        GameObject[] selectedObjects = Selection.gameObjects;
        if (selectedObjects == null || selectedObjects.Length == 0)
        {
            EditorUtility.DisplayDialog("Error", "Please select one or more GameObjects to export.", "OK");
            return;
        }

        string rootFolder = EditorUtility.OpenFolderPanel("Select Export Root Folder", "", "");
        if (string.IsNullOrEmpty(rootFolder)) return;

        int meshCount = 0;
        int animCount = 0;
        int animSkipped = 0;

        foreach (GameObject go in selectedObjects)
        {
            string objFolder = Path.Combine(rootFolder, go.name);
            Directory.CreateDirectory(objFolder);

            // ── Mesh ──
            s_textureExportDir = Path.Combine(objFolder, "Textures");
            Directory.CreateDirectory(s_textureExportDir);
            s_exportedTextures.Clear();
            m_nFrames = 0;

            string meshPath = Path.Combine(objFolder, go.name + ".bin");
            binaryWriter = new BinaryWriter(File.Open(meshPath, FileMode.Create));
            WriteString("<Hierarchy>:");
            WriteFrameHierarchyInfo(go.transform);
            WriteString("</Hierarchy>");
            binaryWriter.Flush();
            binaryWriter.Close();
            binaryWriter = null;
            meshCount++;
            Debug.Log("Mesh Exported: " + meshPath);

            // ── Animation ──
            List<AnimationClip> clips = CollectClips(go);
            if (clips.Count == 0)
            {
                Debug.LogWarning($"Skipping animation for {go.name}: No clips found.");
                animSkipped++;
                continue;
            }

            string animPath = Path.Combine(objFolder, go.name + "_Anim.bin");
            binaryWriter = new BinaryWriter(File.Open(animPath, FileMode.Create));
            WriteInteger("<ClipCount>:", clips.Count);

            List<Transform> allBones = new List<Transform>();
            CollectBones(go.transform, allBones);
            foreach (AnimationClip clip in clips)
                WriteAnimationClip(go, clip, allBones);

            binaryWriter.Flush();
            binaryWriter.Close();
            binaryWriter = null;
            animCount++;
            Debug.Log("Animation Exported: " + animPath);
        }

        string msg = $"Exported {meshCount} meshes, {animCount} animations.";
        if (animSkipped > 0) msg += $"\nSkipped {animSkipped} animations (no clips found).";
        EditorUtility.DisplayDialog("Done", msg, "OK");
    }

    // ─────────────────────────────────────────────
    //  Animation Helpers
    // ─────────────────────────────────────────────

    static List<AnimationClip> CollectClips(GameObject go)
    {
        var clips = new List<AnimationClip>();
        Animator animator = go.GetComponent<Animator>();
        Animation animLegacy = go.GetComponent<Animation>();

        if (animator != null && animator.runtimeAnimatorController != null)
        {
            foreach (AnimationClip clip in animator.runtimeAnimatorController.animationClips)
                if (!clips.Contains(clip)) clips.Add(clip);
        }
        else if (animLegacy != null)
        {
            foreach (AnimationState state in animLegacy)
                if (!clips.Contains(state.clip)) clips.Add(state.clip);
        }
        return clips;
    }

    static void CollectBones(Transform current, List<Transform> bones)
    {
        bones.Add(current);
        for (int i = 0; i < current.childCount; i++)
            CollectBones(current.GetChild(i), bones);
    }

    static void WriteAnimationClip(GameObject rootObj, AnimationClip clip, List<Transform> bones)
    {
        WriteString("<Clip>:", clip.name);
        WriteFloat("<Duration>:", clip.length);
        WriteFloat("<FrameRate>:", clip.frameRate);

        int totalFrames = (int)(clip.length * clip.frameRate);
        WriteInteger("<TotalFrames>:", totalFrames);
        WriteInteger("<KeyframeTracks>:", bones.Count);

        float timePerFrame = 1.0f / clip.frameRate;
        foreach (Transform bone in bones)
        {
            WriteString("<TrackBoneName>:", bone.name);
            WriteInteger("<Keyframes>:", totalFrames);
            for (int i = 0; i < totalFrames; i++)
            {
                clip.SampleAnimation(rootObj, i * timePerFrame);
                WriteInteger(i);
                WriteVector(bone.localPosition);
                WriteVector(bone.localRotation);
                WriteVector(bone.localScale);
            }
        }
        WriteString("</Clip>");
    }

    // ─────────────────────────────────────────────
    //  Mesh Helpers
    // ─────────────────────────────────────────────

    static void WriteFrameHierarchyInfo(Transform child)
    {
        WriteFrameInfo(child);
        WriteInteger("<Children>:", child.childCount);
        for (int k = 0; k < child.childCount; k++)
            WriteFrameHierarchyInfo(child.GetChild(k));
        WriteString("</Frame>");
    }

    static void WriteFrameInfo(Transform current)
    {
        if (!current.gameObject.activeSelf) return;

        WriteObjectName("<Frame>:", m_nFrames++, current.gameObject);
        WriteTransform("<Transform>:", current);
        WriteLocalMatrix("<TransformMatrix>:", current);

        MeshFilter meshFilter = current.gameObject.GetComponent<MeshFilter>();
        MeshRenderer meshRenderer = current.gameObject.GetComponent<MeshRenderer>();
        SkinnedMeshRenderer skinnedMeshRenderer = current.gameObject.GetComponent<SkinnedMeshRenderer>();

        if (meshFilter && meshRenderer)
        {
            WriteMeshInfo(meshFilter.sharedMesh);
            if (meshRenderer.sharedMaterials.Length > 0) WriteMaterials(meshRenderer.sharedMaterials);
        }
        else if (skinnedMeshRenderer)
        {
            WriteMeshInfo(skinnedMeshRenderer.sharedMesh, skinnedMeshRenderer.bones);
            if (skinnedMeshRenderer.sharedMaterials.Length > 0) WriteMaterials(skinnedMeshRenderer.sharedMaterials);
        }
    }

    static void WriteMeshInfo(Mesh mesh, Transform[] bones = null)
    {
        WriteObjectName("<Mesh>:", mesh.vertexCount, mesh);
        WriteBoundingBox("<Bounds>:", mesh.bounds);
        WriteVectors("<Positions>:", mesh.vertices);
        WriteColors("<Colors>:", mesh.colors);
        WriteVectors("<Normals>:", mesh.normals);
        WriteVectors("<TexCoords>:", mesh.uv);

        if (bones != null && mesh.boneWeights.Length > 0)
        {
            WriteBoneWeights("<BoneWeights>:", mesh.boneWeights);
            WriteBindPoses("<BindPoses>:", mesh.bindposes);
            WriteBoneNames("<BoneNames>:", bones);
        }

        WriteInteger("<SubMeshes>:", mesh.subMeshCount);
        for (int i = 0; i < mesh.subMeshCount; i++)
            WriteIntegers("<SubMesh>:", i, mesh.GetTriangles(i));
        WriteString("</Mesh>");
    }

    static void WriteMaterials(Material[] materials)
    {
        WriteInteger("<Materials>:", materials.Length);
        for (int i = 0; i < materials.Length; i++)
        {
            WriteInteger("<Material>:", i);
            if (materials[i].HasProperty("_Color"))           WriteColor("<AlbedoColor>:", materials[i].GetColor("_Color"));
            if (materials[i].HasProperty("_MainTex"))
            {
                Texture pTexture = materials[i].GetTexture("_MainTex");
                if (pTexture)
                {
                    string assetPath = AssetDatabase.GetAssetPath(pTexture);
                    if (!string.IsNullOrEmpty(assetPath))
                        WriteString("<AlbedoMap>:", CopyTexture(assetPath));
                }
            }
            if (materials[i].HasProperty("_EmissionColor"))   WriteColor("<EmissiveColor>:", materials[i].GetColor("_EmissionColor"));
            if (materials[i].HasProperty("_SpecColor"))        WriteColor("<SpecularColor>:", materials[i].GetColor("_SpecColor"));
            if (materials[i].HasProperty("_Glossiness"))       WriteFloat("<Glossiness>:", materials[i].GetFloat("_Glossiness"));
            if (materials[i].HasProperty("_Smoothness"))       WriteFloat("<Smoothness>:", materials[i].GetFloat("_Smoothness"));
            if (materials[i].HasProperty("_Metallic"))         WriteFloat("<Metallic>:", materials[i].GetFloat("_Metallic"));
            if (materials[i].HasProperty("_SpecularHighlights")) WriteFloat("<SpecularHighlight>:", materials[i].GetFloat("_SpecularHighlights"));
            if (materials[i].HasProperty("_GlossyReflections")) WriteFloat("<GlossyReflection>:", materials[i].GetFloat("_GlossyReflections"));
        }
        WriteString("</Materials>");
    }

    static string CopyTexture(string assetPath)
    {
        if (s_exportedTextures.TryGetValue(assetPath, out string cached)) return cached;

        string srcExt = Path.GetExtension(assetPath).ToLower();
        bool needConvert = (srcExt == ".tga" || srcExt == ".psd" || srcExt == ".exr" || srcExt == ".hdr");
        string baseName = Path.GetFileNameWithoutExtension(assetPath);
        string destExt = needConvert ? ".png" : srcExt;
        string texFileName = baseName + destExt;
        string destPath = Path.Combine(s_textureExportDir, texFileName);

        if (!File.Exists(destPath))
        {
            if (needConvert)
            {
                Texture2D tex = AssetDatabase.LoadAssetAtPath<Texture2D>(assetPath);
                if (tex != null) File.WriteAllBytes(destPath, MakeTextureReadable(tex).EncodeToPNG());
            }
            else
            {
                string srcFull = Path.Combine(Path.GetDirectoryName(Application.dataPath), assetPath);
                File.Copy(srcFull, destPath, true);
            }
        }

        s_exportedTextures[assetPath] = texFileName;
        return texFileName;
    }

    static Texture2D MakeTextureReadable(Texture2D src)
    {
        var rt = RenderTexture.GetTemporary(src.width, src.height, 0, RenderTextureFormat.ARGB32, RenderTextureReadWrite.Linear);
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

    // ─────────────────────────────────────────────
    //  Binary Write Primitives
    // ─────────────────────────────────────────────

    static void WriteObjectName(string strHeader, int i, Object obj)
    {
        binaryWriter.Write(strHeader);
        binaryWriter.Write(i);
        binaryWriter.Write(obj ? string.Copy(obj.name).Replace(" ", "_") : "null");
    }

    static void WriteString(string s)                         { binaryWriter.Write(s); }
    static void WriteString(string header, string s)          { binaryWriter.Write(header); binaryWriter.Write(s); }
    static void WriteInteger(int i)                           { binaryWriter.Write(i); }
    static void WriteInteger(string header, int i)            { binaryWriter.Write(header); binaryWriter.Write(i); }
    static void WriteFloat(string header, float f)            { binaryWriter.Write(header); binaryWriter.Write(f); }

    static void WriteVector(Vector2 v)  { binaryWriter.Write(v.x); binaryWriter.Write(v.y); }
    static void WriteVector(Vector3 v)  { binaryWriter.Write(v.x); binaryWriter.Write(v.y); binaryWriter.Write(v.z); }
    static void WriteVector(Vector4 v)  { binaryWriter.Write(v.x); binaryWriter.Write(v.y); binaryWriter.Write(v.z); binaryWriter.Write(v.w); }
    static void WriteVector(Quaternion q) { binaryWriter.Write(q.x); binaryWriter.Write(q.y); binaryWriter.Write(q.z); binaryWriter.Write(q.w); }

    static void WriteColor(Color c) { binaryWriter.Write(c.r); binaryWriter.Write(c.g); binaryWriter.Write(c.b); binaryWriter.Write(c.a); }
    static void WriteColor(string header, Color c) { binaryWriter.Write(header); WriteColor(c); }

    static void WriteVectors(string header, Vector2[] vecs) { binaryWriter.Write(header); binaryWriter.Write(vecs.Length); foreach (var v in vecs) WriteVector(v); }
    static void WriteVectors(string header, Vector3[] vecs) { binaryWriter.Write(header); binaryWriter.Write(vecs.Length); foreach (var v in vecs) WriteVector(v); }
    static void WriteColors(string header, Color[] cols)    { binaryWriter.Write(header); binaryWriter.Write(cols.Length); foreach (var c in cols) WriteColor(c); }

    static void WriteIntegers(string header, int i, int[] vals)
    {
        binaryWriter.Write(header); binaryWriter.Write(i); binaryWriter.Write(vals.Length);
        foreach (int val in vals) binaryWriter.Write(val);
    }

    static void WriteBoundingBox(string header, Bounds b)
    {
        binaryWriter.Write(header); WriteVector(b.center); WriteVector(b.extents);
    }

    static void WriteMatrix(Matrix4x4 m)
    {
        binaryWriter.Write(m.m00); binaryWriter.Write(m.m10); binaryWriter.Write(m.m20); binaryWriter.Write(m.m30);
        binaryWriter.Write(m.m01); binaryWriter.Write(m.m11); binaryWriter.Write(m.m21); binaryWriter.Write(m.m31);
        binaryWriter.Write(m.m02); binaryWriter.Write(m.m12); binaryWriter.Write(m.m22); binaryWriter.Write(m.m32);
        binaryWriter.Write(m.m03); binaryWriter.Write(m.m13); binaryWriter.Write(m.m23); binaryWriter.Write(m.m33);
    }

    static void WriteTransform(string header, Transform t)
    {
        binaryWriter.Write(header);
        WriteVector(t.localPosition); WriteVector(t.localEulerAngles);
        WriteVector(t.localScale);    WriteVector(t.localRotation);
    }

    static void WriteLocalMatrix(string header, Transform t)
    {
        binaryWriter.Write(header);
        Matrix4x4 m = Matrix4x4.identity;
        m.SetTRS(t.localPosition, t.localRotation, t.localScale);
        WriteMatrix(m);
    }

    static void WriteBoneWeights(string header, BoneWeight[] bws)
    {
        binaryWriter.Write(header); binaryWriter.Write(bws.Length);
        foreach (BoneWeight bw in bws)
        {
            binaryWriter.Write(bw.boneIndex0); binaryWriter.Write(bw.weight0);
            binaryWriter.Write(bw.boneIndex1); binaryWriter.Write(bw.weight1);
            binaryWriter.Write(bw.boneIndex2); binaryWriter.Write(bw.weight2);
            binaryWriter.Write(bw.boneIndex3); binaryWriter.Write(bw.weight3);
        }
    }

    static void WriteBindPoses(string header, Matrix4x4[] poses)
    {
        binaryWriter.Write(header); binaryWriter.Write(poses.Length);
        foreach (Matrix4x4 mat in poses) WriteMatrix(mat);
    }

    static void WriteBoneNames(string header, Transform[] bones)
    {
        binaryWriter.Write(header); binaryWriter.Write(bones.Length);
        foreach (Transform bone in bones)
            binaryWriter.Write(bone ? string.Copy(bone.name).Replace(" ", "_") : "null");
    }
}
