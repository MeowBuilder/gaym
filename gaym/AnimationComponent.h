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
    void CrossFade(const std::string& strClipName, float fBlendDuration, bool bLoop = true, bool bForceRestart = false);
    bool IsBlending() const { return m_bIsBlending; }
    bool IsPlaying() const { return m_bIsPlaying; }

    // Restart current animation from beginning
    void Restart();

    // Animation time offset (for desynchronizing multiple instances)
    void SetTimeOffset(float fOffset) { m_fTimeOffset = fOffset; }
    float GetTimeOffset() const { return m_fTimeOffset; }

    // Debug: F3 = static bind pose (no skinning), F4 = no texture (see raw geometry)
    static bool s_bDebugStaticPose;
    bool m_bBoneLogDone = false;  // per-instance bone match log flag

    // Playback speed (1.0 = normal, 0.5 = half speed, 2.0 = double speed)
    void SetPlaybackSpeed(float fSpeed) { m_fPlaybackSpeed = fSpeed; }
    float GetPlaybackSpeed() const { return m_fPlaybackSpeed; }

private:
    std::shared_ptr<AnimationSet> m_pAnimationSet;
    AnimationClip* m_pCurrentClip = nullptr;

    float m_fCurrentTime = 0.0f;
    float m_fTimeOffset = 0.0f;  // Random offset to desync animations
    float m_fPlaybackSpeed = 1.0f;  // Playback speed multiplier
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
