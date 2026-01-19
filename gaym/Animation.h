#pragma once
#include "stdafx.h"
#include <string>
#include <vector>
#include <map>
#include <memory>

struct Keyframe
{
    int m_nFrameIndex;
    XMFLOAT3 m_xmf3Position;
    XMFLOAT4 m_xmf4Rotation;
    XMFLOAT3 m_xmf3Scale;
};

struct BoneTrack
{
    std::string m_strBoneName;
    std::vector<Keyframe> m_vKeyframes;
};

class AnimationClip
{
public:
    std::string m_strName;
    float m_fDuration;
    float m_fFrameRate;
    int m_nTotalFrames;
    
    std::vector<BoneTrack> m_vBoneTracks;
};

class AnimationSet
{
public:
    AnimationSet() {}
    ~AnimationSet() {}

    bool LoadAnimationFromFile(const char* pstrFileName);
    
    AnimationClip* GetClip(const std::string& strName);
    AnimationClip* GetClip(int index);

    std::vector<std::shared_ptr<AnimationClip>> m_vClips;
    std::map<std::string, std::shared_ptr<AnimationClip>> m_mapClips;
};
