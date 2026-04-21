#pragma once
#include "stdafx.h"
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
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

    // 본 이름 → m_vBoneTracks 인덱스. 블렌드 시 이전 clip 의 같은 본을 O(1) lookup 하려고 씀.
    // 로더(AnimationSet::LoadAnimationFromFile) 가 모든 트랙 push 후 BuildBoneIndex() 호출.
    std::unordered_map<std::string, int> m_mapBoneNameToIndex;

    void BuildBoneIndex()
    {
        m_mapBoneNameToIndex.clear();
        m_mapBoneNameToIndex.reserve(m_vBoneTracks.size());
        for (int i = 0; i < (int)m_vBoneTracks.size(); ++i)
            m_mapBoneNameToIndex[m_vBoneTracks[i].m_strBoneName] = i;
    }
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
