#include "stdafx.h"
#include "AnimationComponent.h"
#include "GameObject.h"
#include "TransformComponent.h"
#include "Mesh.h"
#include <functional>
#include <vector>

bool AnimationComponent::s_bDebugStaticPose = false;

AnimationComponent::AnimationComponent(GameObject* pOwner) : Component(pOwner)
{
    m_pAnimationSet = std::make_shared<AnimationSet>();
}

AnimationComponent::~AnimationComponent()
{
}

void AnimationComponent::Init(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList)
{
    m_mapBoneTransforms.clear();
    m_vSkinnedMeshes.clear();
    m_vHierarchyTransforms.clear();
    BuildBoneCache(m_pOwner);
    CollectHierarchyNodes(m_pOwner);
}

void AnimationComponent::BuildBoneCache(GameObject* pGameObject)
{
    if (!pGameObject) return;

    if (pGameObject->m_pstrFrameName[0] != '\0')
    {
        m_mapBoneTransforms[pGameObject->m_pstrFrameName] = pGameObject->GetTransform();
    }

    if (pGameObject->m_pChild) BuildBoneCache(pGameObject->m_pChild);
    if (pGameObject->m_pSibling) BuildBoneCache(pGameObject->m_pSibling);
}

// 매 프레임 재귀 스캔(FindAllSkinnedMeshes / ForceUpdateTransforms) 비용 제거용.
// Init 시 한 번만 순회해서 SkinnedMesh 와 모든 TransformComponent 포인터를 캐싱.
void AnimationComponent::CollectHierarchyNodes(GameObject* pGameObject)
{
    if (!pGameObject) return;

    Mesh* pMesh = pGameObject->GetMesh();
    if (pMesh && (pMesh->GetType() & 0x10))
    {
        m_vSkinnedMeshes.push_back({ static_cast<SkinnedMesh*>(pMesh), pGameObject });
    }
    if (auto* pT = pGameObject->GetTransform())
        m_vHierarchyTransforms.push_back(pT);

    if (pGameObject->m_pChild)   CollectHierarchyNodes(pGameObject->m_pChild);
    if (pGameObject->m_pSibling) CollectHierarchyNodes(pGameObject->m_pSibling);
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
        // Apply time offset to desync animations between instances
        m_fCurrentTime = fmod(m_fTimeOffset, pClip->m_fDuration);
        m_bIsPlaying = true;
    }
}

void AnimationComponent::Stop()
{
    m_bIsPlaying = false;
}

void AnimationComponent::CrossFade(const std::string& strClipName, float fBlendDuration, bool bLoop, bool bForceRestart)
{
    AnimationClip* pNewClip = m_pAnimationSet->GetClip(strClipName);
    if (!pNewClip)
    {
        char buffer[256];
        sprintf_s(buffer, "CrossFade: Clip Not Found: %s\n", strClipName.c_str());
        OutputDebugStringA(buffer);
        return;
    }

    // If same clip and not forcing restart, just continue
    if (pNewClip == m_pCurrentClip && !bForceRestart) return;

    // Store previous clip state for blending
    m_pPreviousClip = m_pCurrentClip;
    m_fPreviousTime = m_fCurrentTime;
    m_bPreviousLoop = m_bLoop;

    // Set new clip
    m_pCurrentClip = pNewClip;
    // Apply time offset for looping animations to desync between instances
    m_fCurrentTime = bLoop ? fmod(m_fTimeOffset, pNewClip->m_fDuration) : 0.0f;
    m_bLoop = bLoop;
    m_bIsPlaying = true;

    // Start blending
    m_bIsBlending = true;
    m_fBlendDuration = fBlendDuration;
    m_fBlendTimer = 0.0f;

}

void AnimationComponent::Restart()
{
    if (m_pCurrentClip)
    {
        m_fCurrentTime = 0.0f;
        m_bIsPlaying = true;
    }
}

void AnimationComponent::Update(float deltaTime)
{
    if (!m_bIsPlaying || !m_pCurrentClip) return;

    // Apply playback speed
    float scaledDelta = deltaTime * m_fPlaybackSpeed;

    // Update blend timer
    if (m_bIsBlending)
    {
        m_fBlendTimer += deltaTime;  // 블렌드 타이머는 실제 시간 사용
        if (m_fBlendTimer >= m_fBlendDuration)
        {
            m_bIsBlending = false;
            m_pPreviousClip = nullptr;
        }

        // Also update previous clip time
        if (m_pPreviousClip)
        {
            m_fPreviousTime += scaledDelta;
            if (m_fPreviousTime >= m_pPreviousClip->m_fDuration)
            {
                if (m_bPreviousLoop)
                    m_fPreviousTime = fmod(m_fPreviousTime, m_pPreviousClip->m_fDuration);
                else
                    m_fPreviousTime = m_pPreviousClip->m_fDuration;
            }
        }
    }

    // Update current clip time
    m_fCurrentTime += scaledDelta;
    if (m_fCurrentTime >= m_pCurrentClip->m_fDuration)
    {
        if (m_bLoop)
            m_fCurrentTime = fmod(m_fCurrentTime, m_pCurrentClip->m_fDuration);
        else
        {
            m_fCurrentTime = m_pCurrentClip->m_fDuration;
            m_bIsPlaying = false;
        }
    }

    // Calculate blend weight (0 = previous, 1 = current)
    float fBlendWeight = m_bIsBlending ? (m_fBlendTimer / m_fBlendDuration) : 1.0f;

    // Sample current clip
    float fFrame = m_fCurrentTime * m_pCurrentClip->m_fFrameRate;
    int nFrame = (int)fFrame;
    int nNextFrame = nFrame + 1;
    float fRatio = fFrame - nFrame;
    if (nFrame >= m_pCurrentClip->m_nTotalFrames - 1)
    {
        nFrame = m_pCurrentClip->m_nTotalFrames - 1;
        nNextFrame = nFrame;
        fRatio = 0.0f;
    }

    // Sample previous clip (if blending)
    int nPrevFrame = 0, nPrevNextFrame = 0;
    float fPrevRatio = 0.0f;
    if (m_bIsBlending && m_pPreviousClip)
    {
        float fPrevFrameF = m_fPreviousTime * m_pPreviousClip->m_fFrameRate;
        nPrevFrame = (int)fPrevFrameF;
        nPrevNextFrame = nPrevFrame + 1;
        fPrevRatio = fPrevFrameF - nPrevFrame;
        if (nPrevFrame >= m_pPreviousClip->m_nTotalFrames - 1)
        {
            nPrevFrame = m_pPreviousClip->m_nTotalFrames - 1;
            nPrevNextFrame = nPrevFrame;
            fPrevRatio = 0.0f;
        }
    }

    // Apply to bones
    for (const auto& track : m_pCurrentClip->m_vBoneTracks)
    {
        auto it = m_mapBoneTransforms.find(track.m_strBoneName);
        if (it == m_mapBoneTransforms.end()) continue;

        TransformComponent* pTransform = it->second;

        // Skip root object - its transform is managed by game logic (position, scale)
        if (pTransform == m_pOwner->GetTransform()) continue;

        if (nFrame >= (int)track.m_vKeyframes.size() || nNextFrame >= (int)track.m_vKeyframes.size())
            continue;

        // Sample current clip pose
        const Keyframe& k1 = track.m_vKeyframes[nFrame];
        const Keyframe& k2 = track.m_vKeyframes[nNextFrame];

        XMVECTOR currPos = XMVectorLerp(XMLoadFloat3(&k1.m_xmf3Position), XMLoadFloat3(&k2.m_xmf3Position), fRatio);
        XMVECTOR currRot = XMQuaternionSlerp(XMLoadFloat4(&k1.m_xmf4Rotation), XMLoadFloat4(&k2.m_xmf4Rotation), fRatio);
        XMVECTOR currScale = XMVectorLerp(XMLoadFloat3(&k1.m_xmf3Scale), XMLoadFloat3(&k2.m_xmf3Scale), fRatio);

        XMVECTOR finalPos = currPos;
        XMVECTOR finalRot = currRot;
        XMVECTOR finalScale = currScale;

        // Blend with previous clip if blending.
        // AnimationClip::m_mapBoneNameToIndex 로 O(1) lookup (이전엔 이전 clip 본 트랙 전체 선형 서치 — O(본²))
        if (m_bIsBlending && m_pPreviousClip)
        {
            auto mapIt = m_pPreviousClip->m_mapBoneNameToIndex.find(track.m_strBoneName);
            if (mapIt != m_pPreviousClip->m_mapBoneNameToIndex.end())
            {
                const BoneTrack& prevTrack = m_pPreviousClip->m_vBoneTracks[mapIt->second];
                if (nPrevFrame < (int)prevTrack.m_vKeyframes.size() && nPrevNextFrame < (int)prevTrack.m_vKeyframes.size())
                {
                    const Keyframe& pk1 = prevTrack.m_vKeyframes[nPrevFrame];
                    const Keyframe& pk2 = prevTrack.m_vKeyframes[nPrevNextFrame];

                    XMVECTOR prevPos = XMVectorLerp(XMLoadFloat3(&pk1.m_xmf3Position), XMLoadFloat3(&pk2.m_xmf3Position), fPrevRatio);
                    XMVECTOR prevRot = XMQuaternionSlerp(XMLoadFloat4(&pk1.m_xmf4Rotation), XMLoadFloat4(&pk2.m_xmf4Rotation), fPrevRatio);
                    XMVECTOR prevScale = XMVectorLerp(XMLoadFloat3(&pk1.m_xmf3Scale), XMLoadFloat3(&pk2.m_xmf3Scale), fPrevRatio);

                    finalPos = XMVectorLerp(prevPos, currPos, fBlendWeight);
                    finalRot = XMQuaternionSlerp(prevRot, currRot, fBlendWeight);
                    finalScale = XMVectorLerp(prevScale, currScale, fBlendWeight);
                }
            }
        }

        XMFLOAT3 outPos, outScale;
        XMFLOAT4 outRot;
        XMStoreFloat3(&outPos, finalPos);
        XMStoreFloat3(&outScale, finalScale);
        XMStoreFloat4(&outRot, finalRot);

        pTransform->SetPosition(outPos);
        pTransform->SetScale(outScale);
        pTransform->SetRotation(outRot);
        pTransform->SetLocalMatrix(Matrix4x4::Identity());
    }

    // --- Skinning Logic ---
    // Init 시점에 캐싱한 m_vSkinnedMeshes / m_vHierarchyTransforms 를 그대로 사용.
    // 이전엔 std::function 재귀 람다로 매 프레임 트리 전체 스캔 (트리 크기 + lambda 오버헤드).
    m_bBoneLogDone = true;

    // F3: skip skinning → explicitly clear bIsSkinned so shader uses raw bind-pose vertices
    if (!m_vSkinnedMeshes.empty() && s_bDebugStaticPose)
    {
        for (const auto& entry : m_vSkinnedMeshes)
            entry.pHolder->SetSkinned(false);
        return;
    }

    if (!m_vSkinnedMeshes.empty())
    {
        // 캐시된 TransformComponent 들에 대해 Update(0) 호출 (world matrix 갱신)
        for (TransformComponent* pT : m_vHierarchyTransforms)
            pT->Update(0.0f);

        for (const auto& entry : m_vSkinnedMeshes)
        {
            SkinnedMesh* pSkinnedMesh = entry.pMesh;
            GameObject* pMeshHolder = entry.pHolder;

            XMMATRIX matRootInvWorld = XMMatrixInverse(nullptr, XMLoadFloat4x4(&pMeshHolder->GetTransform()->GetWorldMatrix()));

            for (size_t i = 0; i < pSkinnedMesh->m_vBoneNames.size(); ++i)
            {
                auto it = m_mapBoneTransforms.find(pSkinnedMesh->m_vBoneNames[i]);
                if (it != m_mapBoneTransforms.end())
                {
                    TransformComponent* pBoneTransform = it->second;

                    XMMATRIX matBoneWorld = XMLoadFloat4x4(&pBoneTransform->GetWorldMatrix());
                    XMMATRIX matInvBindPose = XMLoadFloat4x4(&pSkinnedMesh->m_vBindPoses[i]);

                    XMMATRIX matFinal = matInvBindPose * matBoneWorld * matRootInvWorld;
                    XMFLOAT4X4 f;
                    XMStoreFloat4x4(&f, XMMatrixTranspose(matFinal));
                    pMeshHolder->SetBoneTransform((int)i, f);
                }
            }
            pMeshHolder->SetSkinned(true);
        }
    }
}
