#include "stdafx.h"
#include "FireBeamBehavior.h"
#include "FluidSkillVFXManager.h"
#include "VFXLibrary.h"
#include "GameObject.h"
#include "TransformComponent.h"
#include "SkillComponent.h"
#include "Scene.h"
#include "Room.h"
#include "EnemyComponent.h"

// 코어 빔: swirlSpeed=0, 좁은 spreadRadius → 빔 축을 따라 빽빽하게 흐르는 직선
VFXSequenceDef FireBeamBehavior::BuildCoreBeamDef()
{
    VFXSequenceDef def;
    def.name          = "E_FireBeam_Core";
    def.element       = ElementType::Fire;
    def.particleCount = 300;
    def.spawnRadius   = 0.1f;
    def.particleSize  = 0.35f;

    def.overrideColors    = true;
    def.overrideCoreColor = { 1.0f, 0.92f, 0.55f, 1.0f };
    def.overrideEdgeColor = { 1.0f, 0.55f, 0.10f, 0.80f };

    VFXPhase p;
    p.startTime             = 0.f;
    p.duration              = 99.f;
    p.motionMode            = ParticleMotionMode::Beam;
    p.beamDesc.speedMin     = 10.f;
    p.beamDesc.speedMax     = 16.f;
    p.beamDesc.spreadRadius = 0.15f;
    p.beamDesc.swirlSpeed   = 0.f;
    p.beamDesc.swirlExpand  = false;
    p.beamDesc.enableFlow   = true;
    def.phases.push_back(p);
    return def;
}

// 나선: swirlExpand=true → 시작점에서 퍼지며 공전, swirlFadeEnd=10m에서 소멸
VFXSequenceDef FireBeamBehavior::BuildSwirlDef()
{
    VFXSequenceDef def;
    def.name          = "E_FireBeam_Swirl";
    def.element       = ElementType::Fire;
    def.particleCount = 80;
    def.spawnRadius   = 0.1f;
    def.particleSize  = 0.18f;

    def.overrideColors    = true;
    def.overrideCoreColor = { 0.95f, 0.10f, 0.02f, 0.85f };  // 짙은 크림슨 (코어빔 황금과 대비, 불꽃 계열)
    def.overrideEdgeColor = { 0.55f, 0.04f, 0.00f, 0.50f };

    VFXPhase p;
    p.startTime              = 0.f;
    p.duration               = 99.f;
    p.motionMode             = ParticleMotionMode::Beam;
    p.beamDesc.speedMin      = 7.f;
    p.beamDesc.speedMax      = 12.f;
    p.beamDesc.spreadRadius  = 2.0f;    // 최대 공전 반경
    p.beamDesc.swirlSpeed    = 10.f;    // 공전 각속도
    p.beamDesc.swirlExpand   = true;    // 시작점에서 바깥으로 퍼짐
    p.beamDesc.swirlFadeEnd  = 10.f;    // 10m 중간 지점이 피크, 그 후 사라짐
    p.beamDesc.swirlFadeInOut = true;   // 밝아지다가 사라지는 삼각파
    p.beamDesc.enableFlow    = true;
    def.phases.push_back(p);
    return def;
}

// 방사 스파크: swirlExpand=true, swirlSpeed=0 → 각 파티클이 고정 각도로 콘 형태 퍼짐
// swirlFadeEnd=8m → 8m에서 소멸
VFXSequenceDef FireBeamBehavior::BuildBurstDef()
{
    VFXSequenceDef def;
    def.name          = "E_FireBeam_Burst";
    def.element       = ElementType::Fire;
    def.particleCount = 120;
    def.spawnRadius   = 0.1f;
    def.particleSize  = 0.12f;

    def.overrideColors    = true;
    def.overrideCoreColor = { 1.00f, 0.32f, 0.02f, 1.00f };  // 선명한 적오렌지 (코어빔 황금보다 붉음, 불꽃 계열)
    def.overrideEdgeColor = { 0.80f, 0.10f, 0.01f, 0.80f };

    VFXPhase p;
    p.startTime              = 0.f;
    p.duration               = 99.f;
    p.motionMode             = ParticleMotionMode::Beam;
    p.beamDesc.speedMin      = 14.f;
    p.beamDesc.speedMax      = 20.f;
    p.beamDesc.spreadRadius  = 3.5f;   // 콘 최대 반경
    p.beamDesc.swirlSpeed    = 0.f;    // 각도 고정 → 직선 방사
    p.beamDesc.swirlExpand   = true;   // 시작점에서 퍼짐
    p.beamDesc.swirlFadeEnd  = 1.5f;   // 1.5m에서 거의 즉시 소멸
    p.beamDesc.enableFlow    = true;
    def.phases.push_back(p);
    return def;
}

FireBeamBehavior::FireBeamBehavior()
{
    m_SkillData.name = "FireBeam";
    m_SkillData.element = ElementType::Fire;
    m_SkillData.activationType = ActivationType::Channel;
    m_SkillData.damage = 15.0f;
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
    m_lastTargetPos = targetPosition;

    if (!m_bIsActive)
    {
        m_bIsFinished = false;
        m_bIsActive   = true;
        m_damageMult  = damageMultiplier > 0.f ? damageMultiplier : 1.f;
        m_hitTimer    = 0.f;

        XMFLOAT3 origin    = { 0.f, 0.f, 0.f };
        XMFLOAT3 direction = { 0.f, 0.f, 1.f };

        if (caster && caster->GetTransform())
        {
            origin = caster->GetTransform()->GetPosition();
            origin.y += 1.5f;

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
            XMStoreFloat3(&origin, XMVectorAdd(XMLoadFloat3(&origin),
                XMVectorScale(dirV, 1.3f)));
        }

        m_vfxCoreId  = m_pVFXManager->SpawnSequenceEffect(origin, direction, BuildCoreBeamDef(), true);
        m_vfxSwirlId = m_pVFXManager->SpawnSequenceEffect(origin, direction, BuildSwirlDef(), true);
        m_vfxBurstId = m_pVFXManager->SpawnSequenceEffect(origin, direction, BuildBurstDef(), true);

        wchar_t buf[96];
        swprintf_s(buf, 96, L"[FireBeam] Started core=%d swirl=%d burst=%d\n",
            m_vfxCoreId, m_vfxSwirlId, m_vfxBurstId);
        OutputDebugString(buf);
    }
    else
    {
        if (caster && caster->GetTransform())
        {
            XMFLOAT3 origin = caster->GetTransform()->GetPosition();
            origin.y += 1.5f;

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

            XMFLOAT3 direction;
            XMStoreFloat3(&direction, dirV);
            if (m_vfxCoreId  >= 0) m_pVFXManager->TrackEffect(m_vfxCoreId,  origin, direction);
            if (m_vfxSwirlId >= 0) m_pVFXManager->TrackEffect(m_vfxSwirlId, origin, direction);
            if (m_vfxBurstId >= 0) m_pVFXManager->TrackEffect(m_vfxBurstId, origin, direction);
        }
    }
}

void FireBeamBehavior::Update(float deltaTime)
{
    if (!m_bIsActive || !m_pVFXManager || !m_pCaster) return;
    if (!m_pCaster->GetTransform()) return;

    XMFLOAT3 origin = m_pCaster->GetTransform()->GetPosition();
    origin.y += 1.5f;

    XMVECTOR dirV = m_pCaster->GetTransform()->GetLook();
    dirV = XMVectorSetY(dirV, 0.f);
    dirV = XMVector3Normalize(dirV);

    XMFLOAT3 dir;
    XMStoreFloat3(&dir, dirV);

    XMFLOAT3 offsetOrigin;
    XMStoreFloat3(&offsetOrigin, XMVectorAdd(XMLoadFloat3(&origin),
        XMVectorScale(dirV, 1.3f)));

    if (m_vfxCoreId  >= 0) m_pVFXManager->TrackEffect(m_vfxCoreId,  offsetOrigin, dir);
    if (m_vfxSwirlId >= 0) m_pVFXManager->TrackEffect(m_vfxSwirlId, offsetOrigin, dir);
    if (m_vfxBurstId >= 0) m_pVFXManager->TrackEffect(m_vfxBurstId, offsetOrigin, dir);

    m_hitTimer += deltaTime;
    if (m_hitTimer >= HIT_INTERVAL) {
        m_hitTimer -= HIT_INTERVAL;
        HitEnemiesInBeam(m_SkillData.damage * m_damageMult);
    }
}

void FireBeamBehavior::HitEnemiesInBeam(float damage)
{
    if (!m_pScene || !m_pCaster || !m_pCaster->GetTransform()) return;

    CRoom* pRoom = m_pScene->GetCurrentRoom();
    if (!pRoom) return;

    XMFLOAT3 originF = m_pCaster->GetTransform()->GetPosition();
    originF.y += 1.5f;

    XMVECTOR originV = XMLoadFloat3(&originF);
    XMVECTOR dirV    = m_pCaster->GetTransform()->GetLook();
    dirV = XMVectorSetY(dirV, 0.f);
    dirV = XMVector3Normalize(dirV);

    const auto& gameObjects = pRoom->GetGameObjects();
    for (const auto& obj : gameObjects)
    {
        if (!obj) continue;
        EnemyComponent* pEnemy = obj->GetComponent<EnemyComponent>();
        if (!pEnemy || pEnemy->IsDead()) continue;

        TransformComponent* pTransform = obj->GetTransform();
        if (!pTransform) continue;

        XMFLOAT3 ePos = pTransform->GetPosition();
        ePos.y += 1.0f;
        XMVECTOR toEnemyV = XMVectorSubtract(XMLoadFloat3(&ePos), originV);

        float fwdProj = XMVectorGetX(XMVector3Dot(toEnemyV, dirV));
        if (fwdProj < 0.f || fwdProj > BEAM_RANGE) continue;

        XMVECTOR lateralV = XMVectorSubtract(toEnemyV, XMVectorScale(dirV, fwdProj));
        float lateralDist = XMVectorGetX(XMVector3Length(lateralV));

        XMFLOAT3 eScale = pTransform->GetScale();
        float eRadius = max(1.5f, max(eScale.x, eScale.z) * 1.5f);

        if (lateralDist < BEAM_RADIUS + eRadius)
            pEnemy->TakeDamage(damage, false);
    }
}

bool FireBeamBehavior::IsFinished() const
{
    return m_bIsFinished;
}

void FireBeamBehavior::Reset()
{
    if (m_bIsActive && m_pVFXManager)
    {
        if (m_vfxCoreId  >= 0) m_pVFXManager->StopEffect(m_vfxCoreId);
        if (m_vfxSwirlId >= 0) m_pVFXManager->StopEffect(m_vfxSwirlId);
        if (m_vfxBurstId >= 0) m_pVFXManager->StopEffect(m_vfxBurstId);
        OutputDebugString(L"[FireBeam] Stopped\n");
    }
    m_bIsFinished = true;
    m_bIsActive   = false;
    m_vfxCoreId   = -1;
    m_vfxSwirlId  = -1;
    m_vfxBurstId  = -1;
    m_pCaster     = nullptr;
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
