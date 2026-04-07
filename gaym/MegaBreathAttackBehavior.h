#pragma once
#include "IAttackBehavior.h"
#include <DirectXMath.h>
#include <DirectXCollision.h>
#include <vector>

using namespace DirectX;

class GameObject;
class CRoom;
class ParticleSystem;

// 메가 브레스 공격: 보스가 벽으로 이동 후 맵 전체를 덮는 브레스 공격
// 플레이어는 엄폐물 뒤에 숨어서 회피해야 함
class MegaBreathAttackBehavior : public IAttackBehavior
{
public:
    MegaBreathAttackBehavior(
        float fDamagePerTick = 15.0f,
        float fTickInterval = 0.2f,
        float fMoveSpeed = 20.0f,
        float fMoveToWallTime = 2.0f,
        float fWindupTime = 2.0f,
        float fBreathDuration = 4.0f,
        float fRecoveryTime = 1.0f,
        float fCoverObjectSize = 3.0f
    );
    virtual ~MegaBreathAttackBehavior() = default;

    virtual void Execute(EnemyComponent* pEnemy) override;
    virtual void Update(float dt, EnemyComponent* pEnemy) override;
    virtual bool IsFinished() const override;
    virtual void Reset() override;

private:
    enum class Phase
    {
        TakeOff,      // 이륙
        MoveToWall,   // 벽 끝으로 비행
        Landing,      // 착륙
        SpawnCover,   // 엄폐물 생성
        Windup,       // 브레스 준비
        Breath,       // 브레스 발사
        Recovery      // 회복
    };

    // 벽 이동 관련
    XMFLOAT3 CalculateWallPosition();
    void UpdateMoveToWall(float dt, EnemyComponent* pEnemy);

    // 엄폐물 관리
    void SpawnCoverObjects(EnemyComponent* pEnemy);
    void DestroyCoverObjects();

    // 엄폐 판정
    bool IsPlayerBehindCover(const XMFLOAT3& breathOrigin, const XMFLOAT3& playerPos);
    bool RayIntersectsAABB(const XMFLOAT3& rayOrigin, const XMFLOAT3& rayDir,
                          const BoundingBox& box, float maxDist);

    // 데미지 처리
    void ApplyBreathDamage(EnemyComponent* pEnemy);

private:
    // Room 참조
    CRoom* m_pRoom = nullptr;

    // 파라미터
    float m_fDamagePerTick = 15.0f;
    float m_fTickInterval = 0.2f;
    float m_fMoveSpeed = 20.0f;
    float m_fMoveToWallTime = 2.0f;
    float m_fWindupTime = 2.0f;
    float m_fBreathDuration = 4.0f;
    float m_fRecoveryTime = 1.0f;
    float m_fCoverObjectSize = 3.0f;

    // 비행 관련
    float m_fTakeOffTime = 0.5f;
    float m_fLandingTime = 0.5f;
    float m_fFlyHeight = 15.0f;

    // 런타임 상태
    Phase m_ePhase = Phase::TakeOff;
    float m_fTimer = 0.0f;
    float m_fDamageTickTimer = 0.0f;
    bool m_bFinished = false;

    // 벽 이동
    XMFLOAT3 m_xmf3WallPosition = { 0.0f, 0.0f, 0.0f };
    XMFLOAT3 m_xmf3StartPosition = { 0.0f, 0.0f, 0.0f };
    int m_nWallDirection = 0;  // 0: +X, 1: -X, 2: +Z, 3: -Z

    // 엄폐물 및 장애물
    std::vector<GameObject*> m_vCoverObjects;
    std::vector<BoundingBox> m_vObstacles;

    // 브레스 원점 (보스 위치)
    XMFLOAT3 m_xmf3BreathOrigin = { 0.0f, 0.0f, 0.0f };

    // 애니메이션 플래그
    bool m_bBreathAnimStarted = false;

    // 파티클 이펙트 (GPU 유체 기반)
    class FluidSkillVFXManager* m_pFluidVFXManager = nullptr;
    int m_nFluidVFXId = -1;  // 브레스 유체 VFX ID
};
