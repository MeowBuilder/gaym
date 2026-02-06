#pragma once
#include "Component.h"
#include "Animation.h"
#include <map>

class TransformComponent;

class AnimationComponent : public Component
{
public:
    AnimationComponent(GameObject* pOwner);
    virtual ~AnimationComponent();

    virtual void Init(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList) override;
    virtual void Update(float deltaTime) override;
    virtual void Render(ID3D12GraphicsCommandList* pCommandList) override {}

    void LoadAnimation(const char* pstrFileName);
    void Play(std::string strClipName, bool bLoop = true);
    void Stop();

    // Blending
    void CrossFade(const std::string& strClipName, float fBlendDuration, bool bLoop = true);
    bool IsBlending() const { return m_bIsBlending; }
    bool IsPlaying() const { return m_bIsPlaying; }

private:
    std::shared_ptr<AnimationSet> m_pAnimationSet;
    AnimationClip* m_pCurrentClip = nullptr;

    float m_fCurrentTime = 0.0f;
    bool m_bLoop = true;
    bool m_bIsPlaying = false;

    // Blending state
    AnimationClip* m_pPreviousClip = nullptr;
    float m_fPreviousTime = 0.0f;
    bool m_bPreviousLoop = false;

    bool m_bIsBlending = false;
    float m_fBlendDuration = 0.2f;
    float m_fBlendTimer = 0.0f;

    // Cache to quickly find bone TransformComponents by name
    std::map<std::string, TransformComponent*> m_mapBoneTransforms;

    void BuildBoneCache(GameObject* pGameObject);
};
