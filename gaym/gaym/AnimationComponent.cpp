#include "stdafx.h"
#include "AnimationComponent.h"
#include "GameObject.h"
#include "TransformComponent.h"

AnimationComponent::AnimationComponent(GameObject* pOwner) : Component(pOwner)
{
    m_pAnimationSet = std::make_shared<AnimationSet>();
}

AnimationComponent::~AnimationComponent()
{
}

void AnimationComponent::Init(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList)
{
    // Build the bone cache starting from the owner (root of the model)
    m_mapBoneTransforms.clear();
    BuildBoneCache(m_pOwner);
}

void AnimationComponent::BuildBoneCache(GameObject* pGameObject)
{
    if (!pGameObject) return;

    // Map the frame name to the TransformComponent
    m_mapBoneTransforms[pGameObject->m_pstrFrameName] = pGameObject->GetTransform();

    if (pGameObject->m_pChild) BuildBoneCache(pGameObject->m_pChild);
    if (pGameObject->m_pSibling) BuildBoneCache(pGameObject->m_pSibling);
}

void AnimationComponent::LoadAnimation(const char* pstrFileName)
{
    m_pAnimationSet->LoadAnimationFromFile(pstrFileName);
}

void AnimationComponent::Play(std::string strClipName, bool bLoop)
{
    AnimationClip* pClip = m_pAnimationSet->GetClip(strClipName);
    if (pClip)
    {
        m_pCurrentClip = pClip;
        m_bLoop = bLoop;
        m_fCurrentTime = 0.0f;
        m_bIsPlaying = true;

        char buffer[256];
        sprintf_s(buffer, "Animation Playing: %s (Duration: %f)\n", strClipName.c_str(), pClip->m_fDuration);
        OutputDebugStringA(buffer);
    }
    else
    {
        char buffer[256];
        sprintf_s(buffer, "Animation Clip Not Found: %s\n", strClipName.c_str());
        OutputDebugStringA(buffer);
    }
}

void AnimationComponent::Stop()
{
    m_bIsPlaying = false;
}

void AnimationComponent::Update(float deltaTime)
{
    if (!m_bIsPlaying || !m_pCurrentClip) return;

    m_fCurrentTime += deltaTime;

    // Handle Looping / End of Clip
    if (m_fCurrentTime >= m_pCurrentClip->m_fDuration)
    {
        if (m_bLoop)
        {
            m_fCurrentTime = fmod(m_fCurrentTime, m_pCurrentClip->m_fDuration);
        }
        else
        {
            m_fCurrentTime = m_pCurrentClip->m_fDuration;
            m_bIsPlaying = false; // Stop at end
        }
    }

    // Calculate current frame index (float)
    float fFrame = m_fCurrentTime * m_pCurrentClip->m_fFrameRate;
    int nFrame = (int)fFrame;
    int nNextFrame = nFrame + 1;
    float fRatio = fFrame - nFrame;

    // Clamp frames
    if (nFrame >= m_pCurrentClip->m_nTotalFrames - 1)
    {
        nFrame = m_pCurrentClip->m_nTotalFrames - 1;
        nNextFrame = nFrame;
        fRatio = 0.0f;
    }

    static bool s_bFirstFrameLog = true;
    if (s_bFirstFrameLog) {
        OutputDebugStringA("--- Animation Bone Mapping Start ---\n");
    }

    // Update Bones
    for (const auto& track : m_pCurrentClip->m_vBoneTracks)
    {
        auto it = m_mapBoneTransforms.find(track.m_strBoneName);
        if (it != m_mapBoneTransforms.end())
        {
            if (s_bFirstFrameLog)
            {
                char buffer[256];
                sprintf_s(buffer, "Matched Bone: %s\n", track.m_strBoneName.c_str());
                OutputDebugStringA(buffer);
            }

            TransformComponent* pTransform = it->second;

            if (nFrame < track.m_vKeyframes.size() && nNextFrame < track.m_vKeyframes.size())
            {
                const Keyframe& k1 = track.m_vKeyframes[nFrame];
                const Keyframe& k2 = track.m_vKeyframes[nNextFrame];

                // Interpolate Position
                XMVECTOR p1 = XMLoadFloat3(&k1.m_xmf3Position);
                XMVECTOR p2 = XMLoadFloat3(&k2.m_xmf3Position);
                XMVECTOR p = XMVectorLerp(p1, p2, fRatio);

                // Interpolate Rotation (Slerp)
                XMVECTOR q1 = XMLoadFloat4(&k1.m_xmf4Rotation);
                XMVECTOR q2 = XMLoadFloat4(&k2.m_xmf4Rotation);
                XMVECTOR q = XMQuaternionSlerp(q1, q2, fRatio);

                // Interpolate Scale
                XMVECTOR s1 = XMLoadFloat3(&k1.m_xmf3Scale);
                XMVECTOR s2 = XMLoadFloat3(&k2.m_xmf3Scale);
                XMVECTOR s = XMVectorLerp(s1, s2, fRatio);

                // Store the interpolated values back
                XMFLOAT3 finalPos, finalScale;
                XMFLOAT4 finalRot;
                XMStoreFloat3(&finalPos, p);
                XMStoreFloat3(&finalScale, s);
                XMStoreFloat4(&finalRot, q);

                pTransform->SetPosition(finalPos);
                pTransform->SetScale(finalScale);
                pTransform->SetRotation(finalRot);
            }
        }
        else
        {
            // Optional: Log missing bones (uncomment if needed)
            /*
            if (s_bFirstFrameLog)
            {
                char buffer[256];
                sprintf_s(buffer, "Bone Not Found: %s\n", track.m_strBoneName.c_str());
                OutputDebugStringA(buffer);
            }
            */
        }
    }

    if (s_bFirstFrameLog) {
        OutputDebugStringA("--- Animation Bone Mapping End ---\n");
        s_bFirstFrameLog = false;
    }
}
