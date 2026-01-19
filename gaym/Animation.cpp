#include "stdafx.h"
#include "Animation.h"
#include <fstream>

// Helper to read C# 7-bit encoded string
std::string ReadString(FILE* pInFile)
{
    int nStrLength = 0;
    int shift = 0;
    BYTE byteRead = 0;
    do {
        if (fread(&byteRead, sizeof(BYTE), 1, pInFile) != 1) return "";
        nStrLength |= (byteRead & 0x7F) << shift;
        shift += 7;
    } while (byteRead & 0x80);

    if (nStrLength <= 0) return "";

    std::string str;
    str.resize(nStrLength);
    fread(&str[0], sizeof(char), nStrLength, pInFile);
    return str;
}

int ReadInt(FILE* pInFile)
{
    int i = 0;
    fread(&i, sizeof(int), 1, pInFile);
    return i;
}

float ReadFloat(FILE* pInFile)
{
    float f = 0;
    fread(&f, sizeof(float), 1, pInFile);
    return f;
}

XMFLOAT3 ReadVector3(FILE* pInFile)
{
    XMFLOAT3 v;
    fread(&v, sizeof(float), 3, pInFile);
    return v;
}

XMFLOAT4 ReadVector4(FILE* pInFile)
{
    XMFLOAT4 v;
    fread(&v, sizeof(float), 4, pInFile);
    return v;
}

bool VerifyTag(FILE* pInFile, const std::string& expectedTag)
{
    std::string tag = ReadString(pInFile);
    if (tag != expectedTag)
    {
        std::string errorMsg = "Animation Load Error: Expected '" + expectedTag + "', but got '" + tag + "' (Hex: ";
        for (size_t i = 0; i < tag.size() && i < 10; ++i)
        {
            char hex[8];
            sprintf_s(hex, sizeof(hex), "%02X ", (unsigned char)tag[i]);
            errorMsg += hex;
        }
        errorMsg += ")\n";
        OutputDebugStringA(errorMsg.c_str());
        return false;
    }
    return true;
}

bool AnimationSet::LoadAnimationFromFile(const char* pstrFileName)
{
    FILE* pInFile = nullptr;
    if (fopen_s(&pInFile, pstrFileName, "rb") != 0)
    {
        OutputDebugStringA("Failed to open animation file.\n");
        return false;
    }

    // Read Clip Count
    if (!VerifyTag(pInFile, "<ClipCount>:")) { fclose(pInFile); return false; }
    int nClips = ReadInt(pInFile);

    for (int i = 0; i < nClips; i++)
    {
        auto pClip = std::make_shared<AnimationClip>();

        // <Clip> name
        if (!VerifyTag(pInFile, "<Clip>:")) { fclose(pInFile); return false; }
        pClip->m_strName = ReadString(pInFile);

        // Duration
        if (!VerifyTag(pInFile, "<Duration>:")) { fclose(pInFile); return false; }
        pClip->m_fDuration = ReadFloat(pInFile);

        // FrameRate
        if (!VerifyTag(pInFile, "<FrameRate>:")) { fclose(pInFile); return false; }
        pClip->m_fFrameRate = ReadFloat(pInFile);

        // TotalFrames
        if (!VerifyTag(pInFile, "<TotalFrames>:")) { fclose(pInFile); return false; }
        pClip->m_nTotalFrames = ReadInt(pInFile);

        // KeyframeTracks count
        if (!VerifyTag(pInFile, "<KeyframeTracks>:")) { fclose(pInFile); return false; }
        int nTracks = ReadInt(pInFile);

        for (int k = 0; k < nTracks; k++)
        {
            BoneTrack track;
            
            // Track Bone Name
            if (!VerifyTag(pInFile, "<TrackBoneName>:")) { fclose(pInFile); return false; }
            track.m_strBoneName = ReadString(pInFile);

            // Keyframes Count
            if (!VerifyTag(pInFile, "<Keyframes>:")) { fclose(pInFile); return false; }
            int nKeyframes = ReadInt(pInFile);

            track.m_vKeyframes.resize(nKeyframes);
            for (int f = 0; f < nKeyframes; f++)
            {
                track.m_vKeyframes[f].m_nFrameIndex = ReadInt(pInFile);
                track.m_vKeyframes[f].m_xmf3Position = ReadVector3(pInFile);
                track.m_vKeyframes[f].m_xmf4Rotation = ReadVector4(pInFile);
                track.m_vKeyframes[f].m_xmf3Scale = ReadVector3(pInFile);
            }
            pClip->m_vBoneTracks.push_back(track);
        }
        if (!VerifyTag(pInFile, "</Clip>")) { fclose(pInFile); return false; }

        m_vClips.push_back(pClip);
        m_mapClips[pClip->m_strName] = pClip;
    }

    fclose(pInFile);
    return true;
}

AnimationClip* AnimationSet::GetClip(const std::string& strName)
{
    auto it = m_mapClips.find(strName);
    if (it != m_mapClips.end()) return it->second.get();
    return nullptr;
}

AnimationClip* AnimationSet::GetClip(int index)
{
    if (index >= 0 && index < m_vClips.size()) return m_vClips[index].get();
    return nullptr;
}
