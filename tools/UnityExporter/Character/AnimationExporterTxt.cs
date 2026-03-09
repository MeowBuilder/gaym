using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEditor;
using System.IO;

public class AnimationExporterTxt : Editor
{
    private static StreamWriter textWriter = null;

    [MenuItem("Tools/Export Animation to Text (.txt)")]
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

        string path = EditorUtility.SaveFilePanel("Save Animation Text File", "", selectedObject.name + "_Anim.txt", "txt");
        if (string.IsNullOrEmpty(path)) return;

        textWriter = new StreamWriter(File.Open(path, FileMode.Create));

        WriteString("<ClipCount>:");
        WriteInteger(clips.Count);

        List<Transform> allBones = new List<Transform>();
        CollectBones(selectedObject.transform, allBones);

        foreach (AnimationClip clip in clips)
        {
            WriteAnimationClip(selectedObject, clip, allBones);
        }

        textWriter.Flush();
        textWriter.Close();
        textWriter = null;

        Debug.Log("Animation Text Export Completed: " + path);
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
        WriteString("<Clip>:"); WriteString(clip.name);
        WriteString("<Duration>:"); WriteFloat(clip.length);
        WriteString("<FrameRate>:"); WriteFloat(clip.frameRate);
        
        int totalFrames = (int)(clip.length * clip.frameRate);
        WriteString("<TotalFrames>:"); WriteInteger(totalFrames);

        WriteString("<KeyframeTracks>:"); WriteInteger(bones.Count);

        float timePerFrame = 1.0f / clip.frameRate;

        foreach (Transform bone in bones)
        {
            WriteString("<TrackBoneName>:"); WriteString(bone.name);
            WriteString("<Keyframes>:"); WriteInteger(totalFrames);

            for (int i = 0; i < totalFrames; i++)
            {
                float time = i * timePerFrame;
                clip.SampleAnimation(rootObj, time);

                // Frame Index
                textWriter.WriteLine(i.ToString());
                // Pos
                WriteVector(bone.localPosition);
                // Rot
                WriteVector(bone.localRotation);
                // Scale
                WriteVector(bone.localScale);
            }
        }
        WriteString("</Clip>");
    }

    static void WriteString(string val) { textWriter.WriteLine(val); }
    static void WriteInteger(int val) { textWriter.WriteLine(val.ToString()); }
    static void WriteFloat(float val) { textWriter.WriteLine(val.ToString("F6")); }
    
    static void WriteVector(Vector3 v)
    {
        textWriter.WriteLine(v.x.ToString("F6"));
        textWriter.WriteLine(v.y.ToString("F6"));
        textWriter.WriteLine(v.z.ToString("F6"));
    }

    static void WriteVector(Quaternion q)
    {
        textWriter.WriteLine(q.x.ToString("F6"));
        textWriter.WriteLine(q.y.ToString("F6"));
        textWriter.WriteLine(q.z.ToString("F6"));
        textWriter.WriteLine(q.w.ToString("F6"));
    }
}
