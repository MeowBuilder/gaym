using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEditor;
using System.IO;

public class MeshExporterBin : Editor
{
    private static BinaryWriter binaryWriter = null;
    private static int m_nFrames = 0;

    [MenuItem("Tools/Export to Binary (.bin)")]
    static void ExportToBinary()
    {
        GameObject selectedObject = Selection.activeGameObject;
        if (selectedObject == null)
        {
            EditorUtility.DisplayDialog("Error", "Please select a GameObject to export.", "OK");
            return;
        }

        string path = EditorUtility.SaveFilePanel("Save Binary File", "", selectedObject.name + ".bin", "bin");
        if (string.IsNullOrEmpty(path)) return;

        m_nFrames = 0;
        binaryWriter = new BinaryWriter(File.Open(path, FileMode.Create));

        WriteString("<Hierarchy>:");
        WriteFrameHierarchyInfo(selectedObject.transform);
        WriteString("</Hierarchy>");

        binaryWriter.Flush();
        binaryWriter.Close();
        binaryWriter = null;

        Debug.Log("Model Binary Write Completed: " + path);
    }

    static void WriteObjectName(Object obj)
    {
        binaryWriter.Write((obj) ? string.Copy(obj.name).Replace(" ", "_") : "null");
    }

    static void WriteObjectName(int i, Object obj)
    {
        binaryWriter.Write(i);
        binaryWriter.Write((obj) ? string.Copy(obj.name).Replace(" ", "_") : "null");
    }

    static void WriteObjectName(string strHeader, Object obj)
    {
        binaryWriter.Write(strHeader);
        binaryWriter.Write((obj) ? string.Copy(obj.name).Replace(" ", "_") : "null");
    }

    static void WriteObjectName(string strHeader, int i, Object obj)
    {
        binaryWriter.Write(strHeader);
        binaryWriter.Write(i);
        binaryWriter.Write((obj) ? string.Copy(obj.name).Replace(" ", "_") : "null");
    }

    static void WriteString(string strToWrite)
    {
        binaryWriter.Write(strToWrite);
    }

    static void WriteString(string strHeader, string strToWrite)
    {
        binaryWriter.Write(strHeader);
        binaryWriter.Write(strToWrite);
    }

    static void WriteString(string strToWrite, int i)
    {
        binaryWriter.Write(strToWrite);
        binaryWriter.Write(i);
    }

    static void WriteInteger(int i)
    {
        binaryWriter.Write(i);
    }

    static void WriteInteger(string strHeader, int i)
    {
        binaryWriter.Write(strHeader);
        binaryWriter.Write(i);
    }

    static void WriteFloat(string strHeader, float f)
    {
        binaryWriter.Write(strHeader);
        binaryWriter.Write(f);
    }

    static void WriteVector(Vector2 v)
    {
        binaryWriter.Write(v.x);
        binaryWriter.Write(v.y);
    }

    static void WriteVector(string strHeader, Vector2 v)
    {
        binaryWriter.Write(strHeader);
        WriteVector(v);
    }

    static void WriteVector(Vector3 v)
    {
        binaryWriter.Write(v.x);
        binaryWriter.Write(v.y);
        binaryWriter.Write(v.z);
    }

    static void WriteVector(string strHeader, Vector3 v)
    {
        binaryWriter.Write(strHeader);
        WriteVector(v);
    }

    static void WriteVector(Vector4 v)
    {
        binaryWriter.Write(v.x);
        binaryWriter.Write(v.y);
        binaryWriter.Write(v.z);
        binaryWriter.Write(v.w);
    }

    static void WriteVector(string strHeader, Vector4 v)
    {
        binaryWriter.Write(strHeader);
        WriteVector(v);
    }

    static void WriteVector(Quaternion q)
    {
        binaryWriter.Write(q.x);
        binaryWriter.Write(q.y);
        binaryWriter.Write(q.z);
        binaryWriter.Write(q.w);
    }

    static void WriteColor(Color c)
    {
        binaryWriter.Write(c.r);
        binaryWriter.Write(c.g);
        binaryWriter.Write(c.b);
        binaryWriter.Write(c.a);
    }

    static void WriteColor(string strHeader, Color c)
    {
        binaryWriter.Write(strHeader);
        WriteColor(c);
    }

    static void WriteVectors(string strHeader, Vector2[] vectors)
    {
        binaryWriter.Write(strHeader);
        binaryWriter.Write(vectors.Length);
        if (vectors.Length > 0) foreach (Vector2 v in vectors) WriteVector(v);
    }

    static void WriteVectors(string strHeader, Vector3[] vectors)
    {
        binaryWriter.Write(strHeader);
        binaryWriter.Write(vectors.Length);
        if (vectors.Length > 0) foreach (Vector3 v in vectors) WriteVector(v);
    }

    static void WriteColors(string strHeader, Color[] colors)
    {
        binaryWriter.Write(strHeader);
        binaryWriter.Write(colors.Length);
        if (colors.Length > 0) foreach (Color c in colors) WriteColor(c);
    }

    static void WriteIntegers(string strHeader, int i, int[] pIntegers)
    {
        binaryWriter.Write(strHeader);
        binaryWriter.Write(i);
        binaryWriter.Write(pIntegers.Length);
        if (pIntegers.Length > 0) foreach (int val in pIntegers) binaryWriter.Write(val);
    }

    static void WriteBoundingBox(string strHeader, Bounds bounds)
    {
        binaryWriter.Write(strHeader);
        WriteVector(bounds.center);
        WriteVector(bounds.extents);
    }

    static void WriteMatrix(Matrix4x4 matrix)
    {
        binaryWriter.Write(matrix.m00); binaryWriter.Write(matrix.m10); binaryWriter.Write(matrix.m20); binaryWriter.Write(matrix.m30);
        binaryWriter.Write(matrix.m01); binaryWriter.Write(matrix.m11); binaryWriter.Write(matrix.m21); binaryWriter.Write(matrix.m31);
        binaryWriter.Write(matrix.m02); binaryWriter.Write(matrix.m12); binaryWriter.Write(matrix.m22); binaryWriter.Write(matrix.m32);
        binaryWriter.Write(matrix.m03); binaryWriter.Write(matrix.m13); binaryWriter.Write(matrix.m23); binaryWriter.Write(matrix.m33);
    }

    static void WriteTransform(string strHeader, Transform current)
    {
        binaryWriter.Write(strHeader);
        WriteVector(current.localPosition);
        WriteVector(current.localEulerAngles);
        WriteVector(current.localScale);
        WriteVector(current.localRotation);
    }

    static void WriteLocalMatrix(string strHeader, Transform current)
    {
        binaryWriter.Write(strHeader);
        Matrix4x4 matrix = Matrix4x4.identity;
        matrix.SetTRS(current.localPosition, current.localRotation, current.localScale);
        WriteMatrix(matrix);
    }

    static void WriteBoneWeights(string strHeader, BoneWeight[] boneWeights)
    {
        binaryWriter.Write(strHeader);
        binaryWriter.Write(boneWeights.Length);
        foreach (BoneWeight bw in boneWeights)
        {
            binaryWriter.Write(bw.boneIndex0); binaryWriter.Write(bw.weight0);
            binaryWriter.Write(bw.boneIndex1); binaryWriter.Write(bw.weight1);
            binaryWriter.Write(bw.boneIndex2); binaryWriter.Write(bw.weight2);
            binaryWriter.Write(bw.boneIndex3); binaryWriter.Write(bw.weight3);
        }
    }

    static void WriteBindPoses(string strHeader, Matrix4x4[] bindPoses)
    {
        binaryWriter.Write(strHeader);
        binaryWriter.Write(bindPoses.Length);
        foreach (Matrix4x4 mat in bindPoses) WriteMatrix(mat);
    }

    static void WriteBoneNames(string strHeader, Transform[] bones)
    {
        binaryWriter.Write(strHeader);
        binaryWriter.Write(bones.Length);
        foreach (Transform bone in bones)
        {
            binaryWriter.Write((bone) ? string.Copy(bone.name).Replace(" ", "_") : "null");
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
