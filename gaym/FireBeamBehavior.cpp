#include "stdafx.h"
#include "FireBeamBehavior.h"
#include "FluidSkillVFXManager.h"
#include "VFXLibrary.h"
#include "GameObject.h"
#include "TransformComponent.h"
#include "SkillComponent.h"

FireBeamBehavior::FireBeamBehavior()
{
    m_SkillData.name = "FireBeam";
    m_SkillData.element = ElementType::Fire;
    m_SkillData.activationType = ActivationType::Channel;
    m_SkillData.damage = 15.0f;   // DPS (채널 틱당)
    m_SkillData.cooldown = 4.0f;
    m_SkillData.castTime = 0.0f;
    m_SkillData.range = 20.0f;
    m_SkillData.radius = 1.0f;
    m_SkillData.manaCost = 30.0f;
}

FireBeamBehavior::FireBeamBehavior(const SkillData& customData)
    : m_SkillData(customData)
{
}

void FireBeamBehavior::Execute(GameObject* caster, const DirectX::XMFLOAT3& targetPosition, float damageMultiplier)
{
    if (!m_pVFXManager)
    {
        OutputDebugString(L"[FireBeam] Warning: No VFXManager set!\n");
        m_bIsFinished = true;
        return;
    }

    m_pCaster = caster;

    // 첫 실행 시 VFX 생성 (아직 활성 빔이 없을 때만)
    if (!m_bIsActive)
    {
        m_bIsFinished = false;
        m_bIsActive = true;

        // 플레이어 위치/방향 계산
        XMFLOAT3 origin = { 0.f, 0.f, 0.f };
        XMFLOAT3 direction = { 0.f, 0.f, 1.f };

        if (caster && caster->GetTransform())
        {
            origin = caster->GetTransform()->GetPosition();
            origin.y += 1.5f; // 가슴 높이

            // 타겟 방향 (수평)
            XMVECTOR originV = XMLoadFloat3(&origin);
            XMVECTOR targetV = XMLoadFloat3(&targetPosition);
            XMVECTOR dirV = XMVectorSubtract(targetV, originV);
            dirV = XMVectorSetY(dirV, 0.f);
            dirV = XMVector3Normalize(dirV);

            if (XMVectorGetX(XMVector3LengthSq(dirV)) < 0.001f)
            {
                dirV = caster->GetTransform()->GetLook();
                dirV = XMVectorSetY(dirV, 0.f);
                dirV = XMVector3Normalize(dirV);
            }

            XMStoreFloat3(&direction, dirV);
        }

        // 룬 플래그
        uint32_t runeFlags = GetRuneFlags(caster);

        // VFXLibrary에서 빔 시퀀스 정의 가져오기
        VFXSequenceDef seqDef = VFXLibrary::Get().GetDef(SkillSlot::E, runeFlags);

        // 시퀀스 이펙트 생성
        m_vfxId = m_pVFXManager->SpawnSequenceEffect(origin, direction, seqDef);

        wchar_t buf[256];
        swprintf_s(buf, 256, L"[FireBeam] Started: vfxId=%d, runeFlags=0x%X\n", m_vfxId, runeFlags);
        OutputDebugString(buf);
    }
    else
    {
        // 채널링 중: 빔 방향 업데이트 (매 틱마다 Execute가 호출됨)
        if (caster && caster->GetTransform() && m_vfxId >= 0)
        {
            XMFLOAT3 origin = caster->GetTransform()->GetPosition();
            origin.y += 1.5f;

            XMVECTOR originV = XMLoadFloat3(&origin);
            XMVECTOR targetV = XMLoadFloat3(&targetPosition);
            XMVECTOR dirV = XMVectorSubtract(targetV, originV);
            dirV = XMVectorSetY(dirV, 0.f);
            dirV = XMVector3Normalize(dirV);

            if (XMVectorGetX(XMVector3LengthSq(dirV)) < 0.001f)
            {
                dirV = caster->GetTransform()->GetLook();
                dirV = XMVectorSetY(dirV, 0.f);
                dirV = XMVector3Normalize(dirV);
            }

            XMFLOAT3 direction;
            XMStoreFloat3(&direction, dirV);

            m_pVFXManager->TrackEffect(m_vfxId, origin, direction);
        }
    }
}

void FireBeamBehavior::Update(float deltaTime)
{
    // 빔 방향 추적은 SkillComponent가 채널링 중 Execute()를 매 틱마다 호출하므로
    // 거기서 TrackEffect가 처리됨.
    // VFX 자체 업데이트는 FluidSkillVFXManager::Update()에서 자동 처리.
}

bool FireBeamBehavior::IsFinished() const
{
    return m_bIsFinished;
}

void FireBeamBehavior::Reset()
{
    // 채널링 종료 시 호출됨 - 빔 이펙트 정지
    if (m_bIsActive && m_vfxId >= 0 && m_pVFXManager)
    {
        m_pVFXManager->StopEffect(m_vfxId);
        OutputDebugString(L"[FireBeam] Stopped\n");
    }
    m_bIsFinished = true;
    m_bIsActive = false;
    m_vfxId = -1;
    m_pCaster = nullptr;
}

uint32_t FireBeamBehavior::GetRuneFlags(GameObject* caster) const
{
    uint32_t flags = 0;
    if (!caster) return flags;

    auto* pSkillComp = caster->GetComponent<SkillComponent>();
    if (!pSkillComp || m_slot == SkillSlot::Count) return flags;

    RuneCombo combo = pSkillComp->GetRuneCombo(m_slot);
    if (combo.hasInstant) flags |= RUNE_INSTANT;
    if (combo.hasCharge)  flags |= RUNE_CHARGE;
    if (combo.hasChannel) flags |= RUNE_CHANNEL;
    if (combo.hasPlace)   flags |= RUNE_PLACE;
    if (combo.hasEnhance) flags |= RUNE_ENHANCE;
    if (combo.hasSplit)   flags |= RUNE_SPLIT;
    return flags;
}
