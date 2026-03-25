#include "stdafx.h"
#include "WaveSlashBehavior.h"
#include "FluidSkillVFXManager.h"
#include "VFXLibrary.h"
#include "GameObject.h"
#include "TransformComponent.h"
#include "SkillComponent.h"

WaveSlashBehavior::WaveSlashBehavior()
    : m_SkillData(FireSkillPresets::FlameWave())
{
    // FlameWave 프리셋 사용 (이름만 변경)
    m_SkillData.name = "WaveSlash";
    m_SkillData.cooldown = 3.0f;
}

WaveSlashBehavior::WaveSlashBehavior(const SkillData& customData)
    : m_SkillData(customData)
{
}

void WaveSlashBehavior::Execute(GameObject* caster, const DirectX::XMFLOAT3& targetPosition, float damageMultiplier)
{
    m_bIsFinished = false;

    if (!m_pVFXManager)
    {
        OutputDebugString(L"[WaveSlash] Warning: No VFXManager set!\n");
        m_bIsFinished = true;
        return;
    }

    // 1. 플레이어 위치 (origin), 방향 (direction) 얻기
    XMFLOAT3 origin = { 0.f, 0.f, 0.f };
    XMFLOAT3 direction = { 0.f, 0.f, 1.f };

    if (caster && caster->GetTransform())
    {
        origin = caster->GetTransform()->GetPosition();
        origin.y += 1.0f; // 허리 높이

        // 타겟 방향 계산 (Y축 무시, 수평 방향)
        XMVECTOR originV = XMLoadFloat3(&origin);
        XMVECTOR targetV = XMLoadFloat3(&targetPosition);
        XMVECTOR dirV = XMVectorSubtract(targetV, originV);
        dirV = XMVectorSetY(dirV, 0.f); // 수평으로 플래튼
        dirV = XMVector3Normalize(dirV);

        // 방향이 유효하지 않으면 캐릭터 Look 방향 사용
        if (XMVectorGetX(XMVector3LengthSq(dirV)) < 0.001f)
        {
            dirV = caster->GetTransform()->GetLook();
            dirV = XMVectorSetY(dirV, 0.f);
            dirV = XMVector3Normalize(dirV);
        }

        XMStoreFloat3(&direction, dirV);
    }

    // 2. 룬 비트마스크 계산
    uint32_t runeFlags = GetRuneFlags(caster);

    // 3. VFXLibrary에서 시퀀스 정의 가져오기
    VFXSequenceDef seqDef = VFXLibrary::Get().GetDef(SkillSlot::Q, runeFlags);

    // 4. 시퀀스 이펙트 생성
    m_vfxId = m_pVFXManager->SpawnSequenceEffect(origin, direction, seqDef);

    wchar_t buf[256];
    swprintf_s(buf, 256, L"[WaveSlash] Execute: vfxId=%d, runeFlags=0x%X, dmgMult=%.1f\n",
        m_vfxId, runeFlags, damageMultiplier);
    OutputDebugString(buf);

    m_bIsFinished = true;
}

void WaveSlashBehavior::Update(float deltaTime)
{
    // VFX 업데이트는 FluidSkillVFXManager::Update()에서 자동 처리
}

bool WaveSlashBehavior::IsFinished() const
{
    return m_bIsFinished;
}

void WaveSlashBehavior::Reset()
{
    m_bIsFinished = true;
    m_vfxId = -1;
}

uint32_t WaveSlashBehavior::GetRuneFlags(GameObject* caster) const
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
