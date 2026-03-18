#include "stdafx.h"
#include "FluidSkillVFXManager.h"
#include "DescriptorHeap.h"

void FluidSkillVFXManager::Init(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList,
                                 CDescriptorHeap* pDescriptorHeap, UINT nStartDescIndex)
{
    for (int i = 0; i < MAX_EFFECTS; ++i)
    {
        m_Slots[i].pSystem = std::make_unique<FluidParticleSystem>();
        m_Slots[i].pSystem->Init(pDevice, pCommandList, pDescriptorHeap, nStartDescIndex + i);
    }
    OutputDebugStringA("[FluidSkillVFXManager] Initialized\n");
}

int FluidSkillVFXManager::SpawnEffect(const XMFLOAT3& origin, const XMFLOAT3& direction,
                                       const FluidSkillVFXDef& def)
{
    for (int i = 0; i < MAX_EFFECTS; ++i)
    {
        if (!m_Slots[i].isActive)
        {
            FluidVFXSlot& slot = m_Slots[i];
            slot.isActive  = true;
            slot.elapsed   = 0.0f;
            slot.origin    = origin;
            slot.direction = direction;
            slot.def       = def;

            FluidParticleConfig cfg;
            cfg.element           = def.element;
            cfg.particleCount     = def.particleCount;
            cfg.spawnRadius       = def.spawnRadius;
            cfg.smoothingRadius   = 1.2f;
            cfg.restDensity       = 7.0f;
            cfg.stiffness         = 50.0f;
            cfg.viscosity         = 0.25f;
            cfg.boundaryStiffness = 150.0f;
            cfg.particleSize      = 0.25f;

            slot.pSystem->Spawn(origin, cfg);
            PushControlPoints(slot);

            wchar_t buf[128];
            swprintf_s(buf, 128, L"[FluidSkillVFXManager] SpawnEffect slot %d\n", i);
            OutputDebugString(buf);
            return i;
        }
    }
    OutputDebugStringA("[FluidSkillVFXManager] No free slot!\n");
    return -1;
}

void FluidSkillVFXManager::TrackEffect(int id, const XMFLOAT3& origin, const XMFLOAT3& direction)
{
    if (id < 0 || id >= MAX_EFFECTS || !m_Slots[id].isActive) return;
    m_Slots[id].origin    = origin;
    m_Slots[id].direction = direction;
}

void FluidSkillVFXManager::StopEffect(int id)
{
    if (id < 0 || id >= MAX_EFFECTS) return;
    m_Slots[id].isActive = false;
    m_Slots[id].pSystem->Clear();
}

void FluidSkillVFXManager::ImpactEffect(int id, const XMFLOAT3& impactPos)
{
    if (id < 0 || id >= MAX_EFFECTS || !m_Slots[id].isActive) return;
    auto& slot = m_Slots[id];

    // 단일 제어점을 충돌 위치에 배치 - 파티클이 충돌점으로 수렴
    FluidControlPoint cp;
    cp.position           = impactPos;
    cp.attractionStrength = 35.0f;   // 강한 인력으로 빠르게 집결
    cp.sphereRadius       = 0.25f;   // 매우 좁은 구체 -> 밀집
    slot.pSystem->SetControlPoints({ cp });

    slot.isFadingOut = true;
    slot.fadeTimer   = 0.7f;  // 0.7초 후 소멸
}

void FluidSkillVFXManager::Update(float deltaTime)
{
    for (auto& slot : m_Slots)
    {
        if (!slot.isActive) continue;

        if (slot.isFadingOut) {
            // fade-out 중: 제어점은 이미 충돌 위치에 고정됨, SPH만 업데이트
            slot.pSystem->Update(deltaTime);
            slot.fadeTimer -= deltaTime;
            if (slot.fadeTimer <= 0.0f) {
                slot.isActive    = false;
                slot.isFadingOut = false;
                slot.pSystem->Clear();
            }
            continue;
        }

        slot.elapsed += deltaTime;
        PushControlPoints(slot);
        slot.pSystem->Update(deltaTime);
    }
}

void FluidSkillVFXManager::Render(ID3D12GraphicsCommandList* pCommandList,
                                   const XMFLOAT4X4& viewProj, const XMFLOAT3& camRight, const XMFLOAT3& camUp)
{
    for (auto& slot : m_Slots)
    {
        if (!slot.isActive || !slot.pSystem->IsActive()) continue;
        slot.pSystem->Render(pCommandList, viewProj, camRight, camUp);
    }
}

void FluidSkillVFXManager::PushControlPoints(FluidVFXSlot& slot) const
{
    if (slot.def.cpDescs.empty()) return;

    XMVECTOR fwd = XMVector3Normalize(XMLoadFloat3(&slot.direction));
    XMVECTOR worldUp = XMVectorSet(0, 1, 0, 0);
    float dot = XMVectorGetX(XMVector3Dot(fwd, worldUp));
    XMVECTOR right = (fabsf(dot) > 0.99f)
        ? XMVectorSet(1, 0, 0, 0)
        : XMVector3Normalize(XMVector3Cross(worldUp, fwd));
    XMVECTOR up = XMVector3Cross(fwd, right);

    XMVECTOR originV = XMLoadFloat3(&slot.origin);

    std::vector<FluidControlPoint> cps;
    for (const auto& cpd : slot.def.cpDescs)
    {
        float angle = slot.elapsed * cpd.orbitSpeed + cpd.orbitPhase;
        XMVECTOR worldPos = originV
            + right * (cosf(angle) * cpd.orbitRadius)
            + up    * (sinf(angle) * cpd.orbitRadius)
            + fwd   * cpd.forwardBias;

        FluidControlPoint cp;
        XMStoreFloat3(&cp.position, worldPos);
        cp.attractionStrength = cpd.attractionStrength;
        cp.sphereRadius       = cpd.sphereRadius;
        cps.push_back(cp);
    }
    slot.pSystem->SetControlPoints(cps);
}

FluidSkillVFXDef FluidSkillVFXManager::GetVFXDef(ElementType element, const RuneCombo& combo)
{
    FluidSkillVFXDef def;
    def.element = element;
    def.particleCount = 80;
    def.spawnRadius   = 0.8f;

    switch (element)
    {
    case ElementType::Fire:
    {
        // 2개 CP, 빠른 궤도 - 혜성형
        FluidCPDesc cp0;
        cp0.orbitRadius = 0.65f; cp0.orbitSpeed = 5.0f;
        cp0.orbitPhase = 0.0f;   cp0.forwardBias = 0.3f;
        cp0.attractionStrength = 18.0f; cp0.sphereRadius = 1.8f;

        FluidCPDesc cp1 = cp0;
        cp1.orbitPhase = XM_PI;  cp1.forwardBias = -0.2f;
        cp1.attractionStrength = 14.0f;

        def.cpDescs = { cp0, cp1 };
        break;
    }
    case ElementType::Water:
    {
        // 3개 CP, 느린 삼각형 궤도 - 유선형 구체
        for (int i = 0; i < 3; ++i)
        {
            FluidCPDesc cp;
            cp.orbitRadius = 0.7f; cp.orbitSpeed = 2.5f;
            cp.orbitPhase  = (float)i * (2.0f * XM_PI / 3.0f);
            cp.forwardBias = 0.0f;
            cp.attractionStrength = 14.0f; cp.sphereRadius = 2.0f;
            def.cpDescs.push_back(cp);
        }
        break;
    }
    case ElementType::Wind:
    {
        // 2개 CP, 매우 빠른 궤도 - 와류형
        FluidCPDesc cp0;
        cp0.orbitRadius = 0.5f; cp0.orbitSpeed = 8.0f;
        cp0.orbitPhase = 0.0f;  cp0.forwardBias = 0.4f;
        cp0.attractionStrength = 12.0f; cp0.sphereRadius = 1.5f;

        FluidCPDesc cp1 = cp0;
        cp1.orbitPhase = XM_PI; cp1.forwardBias = -0.4f;

        def.cpDescs = { cp0, cp1 };
        break;
    }
    case ElementType::Earth:
    {
        // 1개 CP, 느린 궤도 - 밀집 구체
        FluidCPDesc cp0;
        cp0.orbitRadius = 0.3f; cp0.orbitSpeed = 1.5f;
        cp0.orbitPhase = 0.0f;  cp0.forwardBias = 0.2f;
        cp0.attractionStrength = 22.0f; cp0.sphereRadius = 1.6f;

        def.cpDescs = { cp0 };
        def.particleCount = 100;
        def.spawnRadius   = 0.6f;
        break;
    }
    default:
    {
        FluidCPDesc cp0;
        cp0.orbitRadius = 0.6f; cp0.orbitSpeed = 3.5f;
        cp0.orbitPhase = 0.0f;  cp0.forwardBias = 0.0f;
        cp0.attractionStrength = 15.0f; cp0.sphereRadius = 1.8f;
        def.cpDescs = { cp0 };
        break;
    }
    }

    // Rune-based modifications
    for (auto& cpd : def.cpDescs) {
        if (combo.hasCharge) {
            cpd.orbitRadius *= 0.65f;
            cpd.orbitSpeed  *= 1.6f;
            cpd.attractionStrength *= 1.4f;
            cpd.sphereRadius *= 0.8f;
        }
        if (combo.hasChannel) {
            cpd.orbitRadius *= 1.35f;
            cpd.orbitSpeed  *= 0.7f;
            cpd.sphereRadius *= 1.3f;
        }
        if (combo.hasEnhance) {
            cpd.orbitRadius *= 1.4f;
            cpd.attractionStrength *= 1.2f;
            cpd.sphereRadius *= 1.35f;
        }
    }
    if (combo.hasCharge)  { def.particleCount = (int)(def.particleCount * 1.3f); def.spawnRadius *= 0.8f; }
    if (combo.hasChannel) { def.particleCount = (int)(def.particleCount * 1.2f); }
    if (combo.hasEnhance) { def.particleCount = (int)(def.particleCount * 1.5f); }
    // particleCount 상한 제한
    if (def.particleCount > 128) def.particleCount = 128;

    return def;
}
