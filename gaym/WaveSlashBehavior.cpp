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
#include <algorithm>

WaveSlashBehavior::WaveSlashBehavior()
    : m_SkillData(FireSkillPresets::FlameWave())
{
    m_SkillData.name     = "WaveSlash";
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

    // 1. 플레이어 위치·방향
    XMFLOAT3 origin    = { 0.f, 0.f, 0.f };
    XMFLOAT3 direction = { 0.f, 0.f, 1.f };

    if (caster && caster->GetTransform())
    {
        origin = caster->GetTransform()->GetPosition();
        origin.y += 0.5f;

        XMVECTOR originV = XMLoadFloat3(&origin);
        XMVECTOR targetV = XMLoadFloat3(&targetPosition);
        XMVECTOR dirV    = XMVectorSubtract(targetV, originV);
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

    // 2. 룬 플래그 → VFX 시퀀스 정의
    uint32_t runeFlags = GetRuneFlags(caster);
    VFXSequenceDef seqDef = VFXLibrary::Get().GetDef(SkillSlot::Q, runeFlags, m_SkillData.element);

    // 3. VFX 스폰
    m_vfxId = m_pVFXManager->SpawnSequenceEffect(origin, direction, seqDef);

    wchar_t buf[256];
    swprintf_s(buf, 256, L"[WaveSlash] Execute: vfxId=%d, runeFlags=0x%X, dmgMult=%.1f\n",
        m_vfxId, runeFlags, damageMultiplier);
    OutputDebugString(buf);

    if (m_vfxId >= 0)
    {
        m_bWaveActive    = true;
        m_damageMult     = damageMultiplier > 0.f ? damageMultiplier : 1.f;
        m_waveElapsed    = 0.f;
        m_trailDropTimer = 0.f;
        m_hitEnemies.clear();
        m_fireTrail.clear();
    }
    else
    {
        m_bIsFinished = true;
    }
}

void WaveSlashBehavior::Update(float deltaTime)
{
    if (!m_bWaveActive || m_vfxId < 0 || !m_pVFXManager) return;

    m_waveElapsed += deltaTime;

    // 1. 파도 본체: VFX 영역 안 적 단타 (아직 안 맞은 적만)
    HitEnemiesInWave(m_SkillData.damage * m_damageMult);

    // 2. 파도 자국 생성: 현재 파도 선두 위치에 불꽃 존 드롭
    m_trailDropTimer += deltaTime;
    if (m_trailDropTimer >= TRAIL_DROP_INTERVAL)
    {
        m_trailDropTimer = 0.f;
        DropFireTrail();
    }

    // 3. 불꽃 자국 DoT 업데이트
    UpdateFireTrail(deltaTime);

    // 4. 파도 종료 (안전 타이머)
    if (m_waveElapsed >= WAVE_DURATION)
    {
        m_bWaveActive = false;
        m_bIsFinished = true;
    }
}

void WaveSlashBehavior::HitEnemiesInWave(float damage)
{
    if (!m_pScene || m_vfxId < 0 || !m_pVFXManager) return;

    CRoom* pRoom = m_pScene->GetCurrentRoom();
    if (!pRoom) return;

    // 실제 파티클 선두 위치: VFX 스폰 원점 + elapsed × WAVE_PARTICLE_SPEED
    // (waveDist 타이머는 waveSpeed=10 m/s — 파티클보다 2배 느림)
    XMFLOAT3 waveOrigin = m_pVFXManager->GetWaveOrigin(m_vfxId);
    XMFLOAT3 waveDir    = m_pVFXManager->GetWaveDir(m_vfxId);
    XMVECTOR originV    = XMLoadFloat3(&waveOrigin);
    XMVECTOR dirV       = XMVector3Normalize(XMLoadFloat3(&waveDir));

    float hitFront = m_waveElapsed * WAVE_PARTICLE_SPEED;
    float hitBack  = (std::max)(0.f, hitFront - WAVE_HIT_DEPTH);

    const auto& gameObjects = pRoom->GetGameObjects();
    for (const auto& obj : gameObjects)
    {
        if (!obj) continue;
        EnemyComponent* pEnemy = obj->GetComponent<EnemyComponent>();
        if (!pEnemy || pEnemy->IsDead()) continue;
        if (m_hitEnemies.count(pEnemy)) continue;

        TransformComponent* pTransform = obj->GetTransform();
        if (!pTransform) continue;

        XMFLOAT3 ePos    = pTransform->GetPosition();
        XMVECTOR toEnemy = XMVectorSubtract(XMLoadFloat3(&ePos), originV);

        // 전진 방향 거리: 슬랩 범위 체크
        float fwdProj = XMVectorGetX(XMVector3Dot(toEnemy, dirV));
        if (fwdProj < hitBack || fwdProj > hitFront) continue;

        // 수평 측면 거리
        XMVECTOR lateralV = XMVectorSubtract(toEnemy, XMVectorScale(dirV, fwdProj));
        lateralV = XMVectorSetY(lateralV, 0.f);
        if (XMVectorGetX(XMVector3Length(lateralV)) > WAVE_HALF_W) continue;

        // 수직 범위
        if (fabsf(ePos.y - waveOrigin.y) > WAVE_HALF_H) continue;

        pEnemy->TakeDamage(damage, false);
        m_hitEnemies.insert(pEnemy);
    }
}

void WaveSlashBehavior::DropFireTrail()
{
    if (m_vfxId < 0 || !m_pVFXManager) return;

    // 히트 판정과 동일한 기준: 파티클 선두 위치 추정
    XMFLOAT3 waveOrigin = m_pVFXManager->GetWaveOrigin(m_vfxId);
    XMFLOAT3 waveDir    = m_pVFXManager->GetWaveDir(m_vfxId);
    XMVECTOR frontV = XMVectorAdd(
        XMLoadFloat3(&waveOrigin),
        XMVectorScale(XMVector3Normalize(XMLoadFloat3(&waveDir)),
                      m_waveElapsed * WAVE_PARTICLE_SPEED));
    XMFLOAT3 frontPos;
    XMStoreFloat3(&frontPos, frontV);
    frontPos.y = 0.f;

    FireZone zone;
    zone.center    = frontPos;
    zone.lifetime  = TRAIL_LIFETIME;
    zone.tickTimer = 0.f;
    m_fireTrail.push_back(zone);
}

void WaveSlashBehavior::UpdateFireTrail(float deltaTime)
{
    if (!m_pScene || m_fireTrail.empty()) return;

    CRoom* pRoom = m_pScene->GetCurrentRoom();
    if (!pRoom) return;

    const auto& gameObjects = pRoom->GetGameObjects();
    float dotDamage = m_SkillData.damage * m_damageMult * TRAIL_DMG_MULT;

    for (auto& zone : m_fireTrail)
    {
        zone.lifetime  -= deltaTime;
        zone.tickTimer += deltaTime;

        if (zone.tickTimer < TRAIL_TICK_INTERVAL) continue;
        zone.tickTimer = 0.f;

        // 존 안의 적에게 DoT (경직 없음)
        for (const auto& obj : gameObjects)
        {
            if (!obj) continue;
            EnemyComponent* pEnemy = obj->GetComponent<EnemyComponent>();
            if (!pEnemy || pEnemy->IsDead()) continue;

            TransformComponent* pT = obj->GetTransform();
            if (!pT) continue;

            XMFLOAT3 ePos = pT->GetPosition();
            float dx = ePos.x - zone.center.x;
            float dz = ePos.z - zone.center.z;
            if (dx * dx + dz * dz <= TRAIL_ZONE_RADIUS * TRAIL_ZONE_RADIUS)
            {
                pEnemy->TakeDamage(dotDamage, false);
            }
        }
    }

    // 만료된 존 제거
    m_fireTrail.erase(
        std::remove_if(m_fireTrail.begin(), m_fireTrail.end(),
            [](const FireZone& z) { return z.lifetime <= 0.f; }),
        m_fireTrail.end());
}

bool WaveSlashBehavior::IsFinished() const
{
    return m_bIsFinished;
}

void WaveSlashBehavior::Reset()
{
    m_bIsFinished = true;
    m_bWaveActive = false;
    m_vfxId       = -1;
    m_hitEnemies.clear();
    m_fireTrail.clear();
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
