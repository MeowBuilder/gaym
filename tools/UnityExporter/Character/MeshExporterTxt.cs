using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEditor;
using System.IO;

public class MeshExporterTxt : Editor
{
    private static StreamWriter textWriter = null;
    private static int m_nFrames = 0;

    [MenuItem("Tools/Export to Text (.txt)")]
    static void ExportToText()
    {
        GameObject selectedObject = Selection.activeGameObject;
        if (selectedObject == null)
        {
            EditorUtility.DisplayDialog("Error", "Please select a GameObject to export.", "OK");
            return;
        }

        string path = EditorUtility.SaveFilePanel("Save Text File", "", selectedObject.name + ".txt", "txt");
        if (string.IsNullOrEmpty(path)) return;

        m_nFrames = 0;
        textWriter = new StreamWriter(File.Open(path, FileMode.Create));

        WriteString("<Hierarchy>:");
        WriteFrameHierarchyInfo(selectedObject.transform);
        WriteString("</Hierarchy>");

        textWriter.Flush();
        textWriter.Close();
        textWriter = null;

        Debug.Log("Model Text Write Completed: " + path);
    }

    static void WriteLine(string value)
    {
        textWriter.WriteLine(value);
    }

    static void WriteObjectName(Object obj)
    {
        WriteLine((obj) ? string.Copy(obj.name).Replace(" ", "_") : "null");
    }

    static void WriteObjectName(int i, Object obj)
    {
        WriteLine(i.ToString());
        WriteLine((obj) ? string.Copy(obj.name).Replace(" ", "_") : "null");
    }

    static void WriteObjectName(string strHeader, Object obj)
    {
        WriteLine(strHeader);
        WriteLine((obj) ? string.Copy(obj.name).Replace(" ", "_") : "null");
    }

    static void WriteObjectName(string strHeader, int i, Object obj)
    {
        WriteLine(strHeader);
        WriteLine(i.ToString());
        WriteLine((obj) ? string.Copy(obj.name).Replace(" ", "_") : "null");
    }

    static void WriteString(string strToWrite)
    {
        WriteLine(strToWrite);
    }

    static void WriteString(string strHeader, string strToWrite)
    {
        WriteLine(strHeader);
        WriteLine(strToWrite);
    }

    static void WriteString(string strToWrite, int i)
    {
        WriteLine(strToWrite);
        WriteLine(i.ToString());
    }

    static void WriteInteger(int i)
    {
        WriteLine(i.ToString());
    }

    static void WriteInteger(string strHeader, int i)
    {
        WriteLine(strHeader);
        WriteLine(i.ToString());
    }

    static void WriteFloat(string strHeader, float f)
    {
        WriteLine(strHeader);
        WriteLine(f.ToString("F6"));
    }

    static void WriteVector(Vector2 v)
    {
        WriteLine(v.x.ToString("F6"));
        WriteLine(v.y.ToString("F6"));
    }

    static void WriteVector(string strHeader, Vector2 v)
    {
        WriteLine(strHeader);
        WriteVector(v);
    }

    static void WriteVector(Vector3 v)
    {
        WriteLine(v.x.ToString("F6"));
        WriteLine(v.y.ToString("F6"));
        WriteLine(v.z.ToString("F6"));
    }

    static void WriteVector(string strHeader, Vector3 v)
    {
        WriteLine(strHeader);
        WriteVector(v);
    }

    static void WriteVector(Vector4 v)
    {
        WriteLine(v.x.ToString("F6"));
        WriteLine(v.y.ToString("F6"));
        WriteLine(v.z.ToString("F6"));
        WriteLine(v.w.ToString("F6"));
    }

    static void WriteVector(string strHeader, Vector4 v)
    {
        WriteLine(strHeader);
        WriteVector(v);
    }

    static void WriteVector(Quaternion q)
    {
        WriteLine(q.x.ToString("F6"));
        WriteLine(q.y.ToString("F6"));
        WriteLine(q.z.ToString("F6"));
        WriteLine(q.w.ToString("F6"));
    }

    static void WriteColor(Color c)
    {
        WriteLine(c.r.ToString("F6"));
        WriteLine(c.g.ToString("F6"));
        WriteLine(c.b.ToString("F6"));
        WriteLine(c.a.ToString("F6"));
    }

    static void WriteColor(string strHeader, Color c)
    {
        WriteLine(strHeader);
        WriteColor(c);
    }

    static void WriteVectors(string strHeader, Vector2[] vectors)
    {
        WriteLine(strHeader);
        WriteLine(vectors.Length.ToString());
        if (vectors.Length > 0) foreach (Vector2 v in vectors) WriteVector(v);
    }

    static void WriteVectors(string strHeader, Vector3[] vectors)
    {
        WriteLine(strHeader);
        WriteLine(vectors.Length.ToString());
        if (vectors.Length > 0) foreach (Vector3 v in vectors) WriteVector(v);
    }

    static void WriteColors(string strHeader, Color[] colors)
    {
        WriteLine(strHeader);
        WriteLine(colors.Length.ToString());
        if (colors.Length > 0) foreach (Color c in colors) WriteColor(c);
    }

    static void WriteIntegers(string strHeader, int i, int[] pIntegers)
    {
        WriteLine(strHeader);
        WriteLine(i.ToString());
        WriteLine(pIntegers.Length.ToString());
        if (pIntegers.Length > 0) foreach (int val in pIntegers) WriteLine(val.ToString());
    }

    static void WriteBoundingBox(string strHeader, Bounds bounds)
    {
        WriteLine(strHeader);
        WriteVector(bounds.center);
        WriteVector(bounds.extents);
    }

    static void WriteMatrix(Matrix4x4 matrix)
    {
        WriteLine(matrix.m00.ToString("F6")); WriteLine(matrix.m10.ToString("F6")); WriteLine(matrix.m20.ToString("F6")); WriteLine(matrix.m30.ToString("F6"));
        WriteLine(matrix.m01.ToString("F6")); WriteLine(matrix.m11.ToString("F6")); WriteLine(matrix.m21.ToString("F6")); WriteLine(matrix.m31.ToString("F6"));
        WriteLine(matrix.m02.ToString("F6")); WriteLine(matrix.m12.ToString("F6")); WriteLine(matrix.m22.ToString("F6")); WriteLine(matrix.m32.ToString("F6"));
        WriteLine(matrix.m03.ToString("F6")); WriteLine(matrix.m13.ToString("F6")); WriteLine(matrix.m23.ToString("F6")); WriteLine(matrix.m33.ToString("F6"));
    }

    static void WriteTransform(string strHeader, Transform current)
    {
        WriteLine(strHeader);
        WriteVector(current.localPosition);
        WriteVector(current.localEulerAngles);
        WriteVector(current.localScale);
        WriteVector(current.localRotation);
    }

    static void WriteLocalMatrix(string strHeader, Transform current)
    {
        WriteLine(strHeader);
        Matrix4x4 matrix = Matrix4x4.identity;
        matrix.SetTRS(current.localPosition, current.localRotation, current.localScale);
        WriteMatrix(matrix);
    }

    static void WriteBoneWeights(string strHeader, BoneWeight[] boneWeights)
    {
        WriteLine(strHeader);
        WriteLine(boneWeights.Length.ToString());
        foreach (BoneWeight bw in boneWeights)
        {
            WriteLine(bw.boneIndex0.ToString()); WriteLine(bw.weight0.ToString("F6"));
            WriteLine(bw.boneIndex1.ToString()); WriteLine(bw.weight1.ToString("F6"));
            WriteLine(bw.boneIndex2.ToString()); WriteLine(bw.weight2.ToString("F6"));
            WriteLine(bw.boneIndex3.ToString()); WriteLine(bw.weight3.ToString("F6"));
        }
    }

    static void WriteBindPoses(string strHeader, Matrix4x4[] bindPoses)
    {
        WriteLine(strHeader);
        WriteLine(bindPoses.Length.ToString());
        foreach (Matrix4x4 mat in bindPoses) WriteMatrix(mat);
    }

    static void WriteBoneNames(string strHeader, Transform[] bones)
    {
        WriteLine(strHeader);
        WriteLine(bones.Length.ToString());
        foreach (Transform bone in bones)
        {
            WriteLine((bone) ? string.Copy(bone.name).Replace(" ", "_") : "null");
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
        if (mesh.subMeshCount > 0)
        {
            for (int i = 0; i < mesh.subMeshCount; i++)
            {
                int[] subindicies = mesh.GetTriangles(i);
                WriteIntegers("<SubMesh>:", i, subindicies);
            }
        }
        WriteString("</Mesh>");
    }

    static void WriteMaterials(Material[] materials)
    {
        WriteInteger("<Materials>:", materials.Length);
        for (int i = 0; i < materials.Length; i++)
        {
            WriteInteger("<Material>:", i);
            if (materials[i].HasProperty("_Color")) WriteColor("<AlbedoColor>:", materials[i].GetColor("_Color"));
            if (materials[i].HasProperty("_MainTex"))
            {
                Texture pTexture = materials[i].GetTexture("_MainTex");
                if (pTexture)
                {
                    string path = AssetDatabase.GetAssetPath(pTexture);
                    WriteString("<AlbedoMap>:", Path.GetFileName(path));
                }
            }
            if (materials[i].HasProperty("_EmissionColor")) WriteColor("<EmissiveColor>:", materials[i].GetColor("_EmissionColor"));
            if (materials[i].HasProperty("_SpecColor")) WriteColor("<SpecularColor>:", materials[i].GetColor("_SpecColor"));
            if (materials[i].HasProperty("_Glossiness")) WriteFloat("<Glossiness>:", materials[i].GetFloat("_Glossiness"));
            if (materials[i].HasProperty("_Smoothness")) WriteFloat("<Smoothness>:", materials[i].GetFloat("_Smoothness"));
            if (materials[i].HasProperty("_Metallic")) WriteFloat("<Metallic>:", materials[i].GetFloat("_Metallic"));
            if (materials[i].HasProperty("_SpecularHighlights")) WriteFloat("<SpecularHighlight>:", materials[i].GetFloat("_SpecularHighlights"));
            if (materials[i].HasProperty("_GlossyReflections")) WriteFloat("<GlossyReflection>:", materials[i].GetFloat("_GlossyReflections"));
        }
        WriteString("</Materials>");
    }

    static void WriteFrameInfo(Transform current)
    {
        if (current.gameObject.activeSelf)
        {
            WriteObjectName("<Frame>:", m_nFrames++, current.gameObject);
            WriteTransform("<Transform>:", current);
            WriteLocalMatrix("<TransformMatrix>:", current);

            MeshFilter meshFilter = current.gameObject.GetComponent<MeshFilter>();
            MeshRenderer meshRenderer = current.gameObject.GetComponent<MeshRenderer>();
            SkinnedMeshRenderer skinnedMeshRenderer = current.gameObject.GetComponent<SkinnedMeshRenderer>();

            if (meshFilter && meshRenderer)
            {
                WriteMeshInfo(meshFilter.sharedMesh);
                Material[] materials = meshRenderer.sharedMaterials;
                if (materials.Length > 0) WriteMaterials(materials);
            }
            else if (skinnedMeshRenderer)
            {
                WriteMeshInfo(skinnedMeshRenderer.sharedMesh, skinnedMeshRenderer.bones);
                Material[] materials = skinnedMeshRenderer.sharedMaterials;
                if (materials.Length > 0) WriteMaterials(materials);
            }
        }
    }

    static void WriteFrameHierarchyInfo(Transform child)
    {
        WriteFrameInfo(child);
        WriteInteger("<Children>:", child.childCount);
        if (child.childCount > 0)
        {
            for (int k = 0; k < child.childCount; k++)
            {
                WriteFrameHierarchyInfo(child.GetChild(k));
            }
        }
        WriteString("</Frame>");
    }
}
