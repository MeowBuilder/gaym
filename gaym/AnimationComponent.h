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

private:
    std::shared_ptr<AnimationSet> m_pAnimationSet;
    AnimationClip* m_pCurrentClip = nullptr;

    float m_fCurrentTime = 0.0f;
    bool m_bLoop = true;
    bool m_bIsPlaying = false;

    // Cache to quickly find bone TransformComponents by name
    std::map<std::string, TransformComponent*> m_mapBoneTransforms;

    void BuildBoneCache(GameObject* pGameObject);
};
