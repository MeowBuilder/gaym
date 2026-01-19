#include "stdafx.h"
#include "AnimationComponent.h"
#include "GameObject.h"
#include "TransformComponent.h"
#include "Mesh.h"
#include <functional>

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
    BuildBoneCache(m_pOwner);
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

    static float s_fLogTimer = 0.0f;
    s_fLogTimer += deltaTime;
    if (s_fLogTimer >= 1.0f)
    {
        char buffer[64];
        sprintf_s(buffer, "Anim Time: %.2f / %.2f\n", m_fCurrentTime, m_pCurrentClip->m_fDuration);
        OutputDebugStringA(buffer);
        s_fLogTimer = 0.0f;
    }

    if (m_fCurrentTime >= m_pCurrentClip->m_fDuration)
    {
        if (m_bLoop) m_fCurrentTime = fmod(m_fCurrentTime, m_pCurrentClip->m_fDuration); 
        else { m_fCurrentTime = m_pCurrentClip->m_fDuration; m_bIsPlaying = false; }
    }

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

    static bool s_bFirstFrameLog = true;
    if (s_bFirstFrameLog) OutputDebugStringA("---" "Animation Bone Mapping Start" "---\n");

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
            if (nFrame < (int)track.m_vKeyframes.size() && nNextFrame < (int)track.m_vKeyframes.size())
            {
                const Keyframe& k1 = track.m_vKeyframes[nFrame];
                const Keyframe& k2 = track.m_vKeyframes[nNextFrame];

                XMVECTOR p = XMVectorLerp(XMLoadFloat3(&k1.m_xmf3Position), XMLoadFloat3(&k2.m_xmf3Position), fRatio);
                XMVECTOR q = XMQuaternionSlerp(XMLoadFloat4(&k1.m_xmf4Rotation), XMLoadFloat4(&k2.m_xmf4Rotation), fRatio);
                XMVECTOR s = XMVectorLerp(XMLoadFloat3(&k1.m_xmf3Scale), XMLoadFloat3(&k2.m_xmf3Scale), fRatio);

                XMFLOAT3 finalPos, finalScale; XMFLOAT4 finalRot;
                XMStoreFloat3(&finalPos, p); XMStoreFloat3(&finalScale, s); XMStoreFloat4(&finalRot, q);

                pTransform->SetPosition(finalPos);
                pTransform->SetScale(finalScale);
                pTransform->SetRotation(finalRot);
                
                // Prevent double transformation: The animation data (Pos/Rot/Scale) 
                // already represents the full local transform. 
                // TransformComponent combines (S*R*T) * matLocal.
                // So we must reset matLocal to Identity.
                pTransform->SetLocalMatrix(Matrix4x4::Identity());
            }
        }
    }

    if (s_bFirstFrameLog) { 
        OutputDebugStringA("---" "Animation Bone Mapping End" "---\\n");
        s_bFirstFrameLog = false;
    }

    // --- Skinning Logic (Search for SkinnedMesh in children if not on Root) ---
    static bool s_bCastLog = false;
    
    SkinnedMesh* pSkinnedMesh = nullptr;
    GameObject* pMeshHolder = nullptr;

    std::function<void(GameObject*)> FindSkinnedMesh = [&](GameObject* pObj)
    {
        if (pSkinnedMesh) return;
        Mesh* pMesh = pObj->GetMesh();
        if (pMesh && (pMesh->GetType() & 0x10))
        {
            pSkinnedMesh = static_cast<SkinnedMesh*>(pMesh);
            pMeshHolder = pObj;
        }
        if (pObj->m_pChild) FindSkinnedMesh(pObj->m_pChild);
        if (pObj->m_pSibling) FindSkinnedMesh(pObj->m_pSibling);
    };
    FindSkinnedMesh(m_pOwner);

    if (!s_bCastLog)
    {
        if (pSkinnedMesh) { 
            char buf[256];
            sprintf_s(buf, "AnimComponent: Found SkinnedMesh on [%s]\n", pMeshHolder->m_pstrFrameName);
            OutputDebugStringA(buf);
        }
        else OutputDebugStringA("AnimComponent: SkinnedMesh NOT FOUND in hierarchy\n");
        s_bCastLog = true;
    }

    if (pSkinnedMesh)
    {
        std::function<void(GameObject*)> ForceUpdateTransforms = [&](GameObject* pObj)
        {
            pObj->GetTransform()->Update(0.0f);
            if (pObj->m_pChild) ForceUpdateTransforms(pObj->m_pChild);
            if (pObj->m_pSibling) ForceUpdateTransforms(pObj->m_pSibling);
        };
        ForceUpdateTransforms(m_pOwner);

        XMMATRIX matRootInvWorld = XMMatrixInverse(nullptr, XMLoadFloat4x4(&pMeshHolder->GetTransform()->GetWorldMatrix()));

        for (size_t i = 0; i < pSkinnedMesh->m_vBoneNames.size(); ++i)
        {
            auto it = m_mapBoneTransforms.find(pSkinnedMesh->m_vBoneNames[i]);
            if (it != m_mapBoneTransforms.end())
            {
                                                TransformComponent* pBoneTransform = it->second;
                                                
                                                XMMATRIX matBoneWorld = XMLoadFloat4x4(&pBoneTransform->GetWorldMatrix());
                                                XMMATRIX matInvBindPose = XMLoadFloat4x4(&pSkinnedMesh->m_vBindPoses[i]);
                                                
                                                XMMATRIX matFinal = matInvBindPose * matBoneWorld * matRootInvWorld;                XMFLOAT4X4 f; XMStoreFloat4x4(&f, XMMatrixTranspose(matFinal));
                pMeshHolder->SetBoneTransform((int)i, f);
            }
        }
        pMeshHolder->SetSkinned(true);
    }
}
