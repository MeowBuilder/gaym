#pragma once

#include "stdafx.h"
#include "FluidParticle.h"
#include "VFXTypes.h"
#include <vector>
#include <array>

class CDescriptorHeap;

class FluidParticleSystem
{
public:
    FluidParticleSystem();
    ~FluidParticleSystem();

    // D3D12 initialization - call once after Scene creates descriptor heap
    void Init(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList,
              CDescriptorHeap* pDescriptorHeap, UINT nSrvDescriptorIndex);

    // Spawn particles around a center point (replaces current particles)
    void Spawn(const XMFLOAT3& center, const FluidParticleConfig& config);

    // Remove all particles
    void Clear();

    // Control points
    void SetControlPoints(const std::vector<FluidControlPoint>& cps);
    void AddControlPoint(const FluidControlPoint& cp);
    void ClearControlPoints();

    // Per-frame update (SPH simulation)
    void Update(float deltaTime);

    // Render - call after main render pass
    void Render(ID3D12GraphicsCommandList* pCommandList,
                const XMFLOAT4X4& viewProj, const XMFLOAT3& cameraRight, const XMFLOAT3& cameraUp);

    bool IsActive() const { return !m_Particles.empty(); }
    int  GetParticleCount() const { return static_cast<int>(m_Particles.size()); }

    // Shift all particle positions by delta (used to co-move with a projectile)
    void OffsetParticles(const XMFLOAT3& delta);

    // 운동 모드 설정
    void SetMotionMode(ParticleMotionMode mode);
    void SetConfinementBox(const ConfinementBoxDesc& box);
    void SetBeamDesc(const BeamDesc& beam);
    void SetGravityDesc(const GravityDesc& grav);
    void InitBeamParticles();  // Beam 모드 전용 초기화
    void ApplyRadialBurst(XMFLOAT3 center, float minSpeed, float maxSpeed);

private:
    // SPH phases
    void BuildSpatialHash();
    void ComputeDensityPressure();
    void ComputeForces();
    void Integrate(float dt);

    // Collect neighbors for particle at index i (writes into out, returns count)
    int  GetNeighbors(int i, int* out, int maxOut) const;

    // SPH kernel helpers
    float    Poly6(float r2, float h2) const;
    XMFLOAT3 SpikyGrad(const XMFLOAT3& r, float rLen, float h) const;
    float    ViscLaplacian(float r, float h) const;

    // Hash helpers
    int  HashCell(int cx, int cy, int cz) const;
    void CellFromPos(const XMFLOAT3& p, int& cx, int& cy, int& cz) const;

    // Upload visible particles to GPU buffer and set m_nActiveCount
    void UploadRenderData();

    // Simulation state
    std::vector<FluidParticle>     m_Particles;
    std::vector<FluidControlPoint> m_ControlPoints;
    FluidParticleConfig            m_Config;
    FluidElementColor              m_Colors;

    // 확장: 운동 모드 관련 멤버
    ParticleMotionMode             m_MotionMode = ParticleMotionMode::ControlPoint;
    ConfinementBoxDesc             m_ConfinementBox;
    BeamDesc                       m_BeamDesc;
    GravityDesc                    m_GravityDesc;
    std::vector<FluidControlPoint> m_OrbitalCPs; // OrbitalCP 모드 위성 CP 포함

    // Spatial hash (power-of-2 table, direct-mapped cells)
    static constexpr int HASH_TABLE_SIZE      = 4096;
    static constexpr int MAX_PER_CELL         = 20;
    static constexpr int MAX_NEIGHBOR_SEARCH  = 27 * MAX_PER_CELL;
    static constexpr int MAX_PARTICLES        = 512;

    struct SpatialHashCell
    {
        int particles[MAX_PER_CELL];
        int count = 0;
    };
    std::array<SpatialHashCell, HASH_TABLE_SIZE> m_HashTable;

    // D3D12 resources
    ComPtr<ID3D12Resource>      m_pParticleBuffer;      // Upload heap, persistently mapped
    FluidParticleRenderData*    m_pMappedParticles  = nullptr;
    int                         m_nActiveCount      = 0;

    ComPtr<ID3D12Resource>      m_pPassCB;              // Internal pass constant buffer
    void*                       m_pMappedPassCB     = nullptr;

    CDescriptorHeap*            m_pDescriptorHeap       = nullptr;
    UINT                        m_nSrvDescriptorIndex   = 0;

    // D3D12 shared pipeline (static - compiled once for all instances)
    static ComPtr<ID3D12RootSignature> s_pRootSignature;
    static ComPtr<ID3D12PipelineState> s_pPSO;
    static void BuildSharedPipeline(ID3D12Device* pDevice);

    // PRNG for spawn
    uint32_t m_Seed = 12345;
    float    Rand01();
    float    RandRange(float lo, float hi);
    XMFLOAT3 RandInSphere(float radius);
};
