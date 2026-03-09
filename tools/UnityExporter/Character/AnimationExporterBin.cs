using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEditor;
using System.IO;

public class AnimationExporterBin : Editor
{
    private static BinaryWriter binaryWriter = null;

    [MenuItem("Tools/Export Animation to Binary (.bin)")]
    static void ExportAnimations()
    {
        GameObject selectedObject = Selection.activeGameObject;
        if (selectedObject == null)
        {
            EditorUtility.DisplayDialog("Error", "Please select a GameObject with an Animator or Animation component.", "OK");
            return;
        }

        Animator animator = selectedObject.GetComponent<Animator>();
        Animation animationLegacy = selectedObject.GetComponent<Animation>();

        List<AnimationClip> clips = new List<AnimationClip>();

        if (animator != null && animator.runtimeAnimatorController != null)
        {
            foreach (AnimationClip clip in animator.runtimeAnimatorController.animationClips)
            {
                if (!clips.Contains(clip)) clips.Add(clip);
            }
        }
        else if (animationLegacy != null)
        {
            foreach (AnimationState state in animationLegacy)
            {
                if (!clips.Contains(state.clip)) clips.Add(state.clip);
            }
        }

        if (clips.Count == 0)
        {
            EditorUtility.DisplayDialog("Error", "No Animation Clips found.", "OK");
            return;
        }

        // Folder selection
        string path = EditorUtility.SaveFilePanel("Save Animation Binary File", "", selectedObject.name + "_Anim.bin", "bin");
        if (string.IsNullOrEmpty(path)) return;

        binaryWriter = new BinaryWriter(File.Open(path, FileMode.Create));

        // 1. Export Clip Count
        WriteInteger("<ClipCount>:", clips.Count);

        // 2. Collect all bone transforms (flatten hierarchy)
        List<Transform> allBones = new List<Transform>();
        CollectBones(selectedObject.transform, allBones);
        
        // Write Bone Names Map (Optional, but good for validation)
        // WriteInteger("<BoneCount>:", allBones.Count);
        // foreach(Transform t in allBones) WriteString(t.name);

        foreach (AnimationClip clip in clips)
        {
            WriteAnimationClip(selectedObject, clip, allBones);
        }

        binaryWriter.Flush();
        binaryWriter.Close();
        binaryWriter = null;

        Debug.Log("Animation Binary Export Completed: " + path);
    }

    static void CollectBones(Transform current, List<Transform> bones)
    {
        bones.Add(current);
        for (int i = 0; i < current.childCount; i++)
        {
            CollectBones(current.GetChild(i), bones);
        }
    }

    static void WriteAnimationClip(GameObject rootObj, AnimationClip clip, List<Transform> bones)
    {
        WriteString("<Clip>:", clip.name);
        WriteFloat("<Duration>:", clip.length);
        WriteFloat("<FrameRate>:", clip.frameRate);
        
        int totalFrames = (int)(clip.length * clip.frameRate);
        WriteInteger("<TotalFrames>:", totalFrames);

        WriteInteger("<KeyframeTracks>:", bones.Count);

        // Prepare sampling
        float timePerFrame = 1.0f / clip.frameRate;

        // Iterate through each bone -> create a track
        foreach (Transform bone in bones)
        {
            WriteString("<TrackBoneName>:", bone.name);
            WriteInteger("<Keyframes>:", totalFrames); // We sample every frame for simplicity

            // Sample loop
            for (int i = 0; i < totalFrames; i++)
            {
                float time = i * timePerFrame;
                clip.SampleAnimation(rootObj, time); // Apply animation at time t

                WriteInteger(i); // Frame Index
                WriteVector(bone.localPosition);
                WriteVector(bone.localRotation); // Quaternion
                WriteVector(bone.localScale);
            }
        }
        WriteString("</Clip>");
    }

    // --- Helper Functions (Same as MeshExporter) ---

    static void WriteString(string strHeader, string strToWrite)
    {
        binaryWriter.Write(strHeader);
        binaryWriter.Write(strToWrite); // 7-bit encoded length + string
    }

    static void WriteString(string strToWrite)
    {
        binaryWriter.Write(strToWrite);
    }

    static void WriteInteger(string strHeader, int i)
    {
        binaryWriter.Write(strHeader);
        binaryWriter.Write(i);
    }

    static void WriteInteger(int i)
    {
        binaryWriter.Write(i);
    }

    static void WriteFloat(string strHeader, float f)
    {
        binaryWriter.Write(strHeader);
        binaryWriter.Write(f);
    }

    static void WriteVector(Vector3 v)
    {
        binaryWriter.Write(v.x);
        binaryWriter.Write(v.y);
        binaryWriter.Write(v.z);
    }

    static void WriteVector(Quaternion q)
    {
        binaryWriter.Write(q.x);
        binaryWriter.Write(q.y);
        binaryWriter.Write(q.z);
        binaryWriter.Write(q.w);
    }
}
