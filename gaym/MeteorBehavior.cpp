#include "stdafx.h"
#include "MeteorBehavior.h"
#include "FluidSkillVFXManager.h"
#include "VFXLibrary.h"
#include "GameObject.h"
#include "TransformComponent.h"
#include "SkillComponent.h"

MeteorBehavior::MeteorBehavior()
    : m_SkillData(FireSkillPresets::Meteor())
{
}

MeteorBehavior::MeteorBehavior(const SkillData& customData)
    : m_SkillData(customData)
{
}

void MeteorBehavior::Execute(GameObject* caster, const DirectX::XMFLOAT3& targetPosition, float damageMultiplier)
{
    m_bIsFinished = false;

    if (!m_pVFXManager)
    {
        OutputDebugString(L"[Meteor] Warning: No VFXManager set!\n");
        m_bIsFinished = true;
        return;
    }

    // 1. 타겟 위치 결정 (targetPosition 사용, 마우스 클릭 위치)
    //    SkillComponent::CalculateTargetPosition()이 이미 계산해서 넘겨줌
    XMFLOAT3 targetPos = targetPosition;

    // targetPosition이 (0,0,0)이면 플레이어 전방으로 폴백
    if (fabsf(targetPos.x) < 0.001f && fabsf(targetPos.z) < 0.001f)
    {
        if (caster && caster->GetTransform())
        {
            XMFLOAT3 casterPos = caster->GetTransform()->GetPosition();
            XMVECTOR look = caster->GetTransform()->GetLook();
            look = XMVectorSetY(look, 0.f);
            look = XMVector3Normalize(look);

            XMVECTOR targetV = XMVectorAdd(
                XMLoadFloat3(&casterPos),
                XMVectorScale(look, METEOR_FORWARD_DIST)
            );
            XMStoreFloat3(&targetPos, targetV);
        }
    }

    // 2. 스폰 위치 = 타겟 상공
    XMFLOAT3 spawnPos = { targetPos.x, targetPos.y + METEOR_SPAWN_HEIGHT, targetPos.z };

    // 3. 방향 = 아래 (낙하)
    XMFLOAT3 direction = { 0.f, -1.f, 0.f };

    // 4. 룬 플래그
    uint32_t runeFlags = GetRuneFlags(caster);

    // 5. VFXLibrary에서 메테오 시퀀스 정의 가져오기 (스킬 속성색 적용)
    VFXSequenceDef seqDef = VFXLibrary::Get().GetDef(SkillSlot::R, runeFlags, m_SkillData.element);

    // 6. 시퀀스 이펙트 생성
    //    SpawnSequenceEffect 내부에서 masterCPPos = { origin.x, origin.y + 50, origin.z } 로 설정됨
    //    origin = spawnPos (이미 상공 위치)이므로, masterCPPos를 spawnPos 자체로 쓰기 위해
    //    spawnPos를 origin으로 전달
    m_vfxId = m_pVFXManager->SpawnSequenceEffect(spawnPos, direction, seqDef);

    wchar_t buf[256];
    swprintf_s(buf, 256,
        L"[Meteor] Execute: vfxId=%d, target=(%.1f,%.1f,%.1f), spawn=(%.1f,%.1f,%.1f), runeFlags=0x%X\n",
        m_vfxId, targetPos.x, targetPos.y, targetPos.z,
        spawnPos.x, spawnPos.y, spawnPos.z, runeFlags);
    OutputDebugString(buf);

    m_bIsFinished = true;
}

void MeteorBehavior::Update(float deltaTime)
{
    // 메테오 VFX 업데이트는 FluidSkillVFXManager::Update()에서 자동 처리
    // OrbitalCP 낙하 -> Gravity 폭발 전환도 UpdatePhase에서 처리됨
}

bool MeteorBehavior::IsFinished() const
{
    return m_bIsFinished;
}

void MeteorBehavior::Reset()
{
    m_bIsFinished = true;
    m_vfxId = -1;
}

uint32_t MeteorBehavior::GetRuneFlags(GameObject* caster) const
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
