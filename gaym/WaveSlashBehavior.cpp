#include "stdafx.h"
#include "WaveSlashBehavior.h"
#include "FluidSkillVFXManager.h"
#include "VFXLibrary.h"
#include "GameObject.h"
#include "TransformComponent.h"
#include "SkillComponent.h"
#include "Scene.h"
#include "Room.h"
#include "EnemyComponent.h"
#include <cmath>

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
    m_bWaveActive = false;

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
        origin.y += 0.5f; // 지면 근처 — 불길이 바닥에 깔리도록

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

    // 3. VFXLibrary에서 시퀀스 정의 가져오기 (스킬 속성색 적용)
    VFXSequenceDef seqDef = VFXLibrary::Get().GetDef(SkillSlot::Q, runeFlags, m_SkillData.element);

    // 4. 시퀀스 이펙트 생성
    m_vfxId = m_pVFXManager->SpawnSequenceEffect(origin, direction, seqDef);

    wchar_t buf[256];
    swprintf_s(buf, 256, L"[WaveSlash] Execute: vfxId=%d, runeFlags=0x%X, dmgMult=%.1f\n",
        m_vfxId, runeFlags, damageMultiplier);
    OutputDebugString(buf);

    if (m_vfxId >= 0) {
        m_bWaveActive = true;
        m_waveOrigin  = origin;
        m_waveDir     = direction;
        m_damageMult  = damageMultiplier > 0.f ? damageMultiplier : 1.f;
        m_hitTimer    = 0.f;
        // m_bIsFinished = false 유지 → Update()에서 충돌 감지 후 true로 설정
    } else {
        m_bIsFinished = true;
    }
}

void WaveSlashBehavior::Update(float deltaTime)
{
    if (!m_bWaveActive || m_vfxId < 0 || !m_pVFXManager) return;

    // 파도가 최대 거리에 도달하면 종료
    if (!m_pVFXManager->IsWaveActive(m_vfxId)) {
        m_bWaveActive = false;
        m_bIsFinished = true;
        return;
    }

    // 다단 히트: HIT_INTERVAL마다 파도 범위 안 적 데미지
    m_hitTimer += deltaTime;
    if (m_hitTimer >= HIT_INTERVAL) {
        m_hitTimer -= HIT_INTERVAL;
        HitEnemiesInWave(m_SkillData.damage * m_damageMult);
    }
}

void WaveSlashBehavior::HitEnemiesInWave(float damage)
{
    if (!m_pScene || m_vfxId < 0 || !m_pVFXManager) return;

    CRoom* pRoom = m_pScene->GetCurrentRoom();
    if (!pRoom) return;

    float waveDist = m_pVFXManager->GetWaveDist(m_vfxId);
    if (waveDist <= 0.01f) return;

    XMVECTOR originV = XMLoadFloat3(&m_waveOrigin);
    XMVECTOR dirV    = XMVector3Normalize(XMLoadFloat3(&m_waveDir));

    // 파티클은 파도 선두 근처에 집중됨 — 선두에서 WAVE_HIT_DEPTH 만큼만 판정
    float frontZ  = waveDist;
    float backZ   = (std::max)(0.f, waveDist - WAVE_HIT_DEPTH);

    const auto& gameObjects = pRoom->GetGameObjects();
    for (const auto& obj : gameObjects)
    {
        if (!obj) continue;
        EnemyComponent* pEnemy = obj->GetComponent<EnemyComponent>();
        if (!pEnemy || pEnemy->IsDead()) continue;

        TransformComponent* pTransform = obj->GetTransform();
        if (!pTransform) continue;

        XMFLOAT3 ePos = pTransform->GetPosition();
        XMVECTOR toEnemyV = XMVectorSubtract(XMLoadFloat3(&ePos), originV);

        // 전진 축 투영: 파도 선두 슬랩 안에 있어야 함
        float fwdProj = XMVectorGetX(XMVector3Dot(toEnemyV, dirV));
        if (fwdProj < backZ || fwdProj > frontZ) continue;

        // 수평 측면 거리 (파도 너비)
        XMVECTOR lateralV = XMVectorSubtract(toEnemyV, XMVectorScale(dirV, fwdProj));
        lateralV = XMVectorSetY(lateralV, 0.f);
        float lateralDist = XMVectorGetX(XMVector3Length(lateralV));
        if (lateralDist > WAVE_HALF_W) continue;

        // 수직 범위
        float heightDiff = fabsf(ePos.y - m_waveOrigin.y);
        if (heightDiff > WAVE_HALF_H) continue;

        // 다단히트: 경직 없음
        pEnemy->TakeDamage(damage, false);
    }
}

bool WaveSlashBehavior::IsFinished() const
{
    return m_bIsFinished;
}

void WaveSlashBehavior::Reset()
{
    m_bIsFinished = true;
    m_bWaveActive = false;
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
