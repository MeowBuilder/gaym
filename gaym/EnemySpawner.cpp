#include "stdafx.h"
#include "EnemySpawner.h"
#include "GameObject.h"
#include "TransformComponent.h"
#include "EnemyComponent.h"
#include "ColliderComponent.h"
#include "RenderComponent.h"
#include "AnimationComponent.h"
#include "CollisionLayer.h"
#include "MeleeAttackBehavior.h"
#include "RushAoEAttackBehavior.h"
#include "RushFrontAttackBehavior.h"
#include "RangedAttackBehavior.h"
#include "BreathAttackBehavior.h"
#include "FlyingBarrageAttackBehavior.h"
#include "FlyingStrafeAttackBehavior.h"
#include "DiveBombAttackBehavior.h"
#include "FlyingCircleAttackBehavior.h"
#include "FlyingSweepAttackBehavior.h"
#include "TailSweepAttackBehavior.h"
#include "JumpSlamAttackBehavior.h"
#include "ComboAttackBehavior.h"
#include "MegaBreathAttackBehavior.h"
#include "BossPhaseConfig.h"
#include "BossPhaseController.h"
#include "Room.h"
#include "Scene.h"
#include "ProjectileManager.h"
#include "Shader.h"
#include "Mesh.h"
#include "MeshLoader.h"

EnemySpawner::EnemySpawner()
{
}

EnemySpawner::~EnemySpawner()
{
}

void EnemySpawner::Init(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList, Scene* pScene, Shader* pShader)
{
    m_pDevice = pDevice;
    m_pCommandList = pCommandList;
    m_pScene = pScene;
    m_pShader = pShader;

    // Register default test enemy preset
    EnemySpawnData testEnemy;
    testEnemy.m_xmf3Scale = XMFLOAT3(1.0f, 2.0f, 1.0f);  // Human-like proportions
    testEnemy.m_xmf4Color = XMFLOAT4(1.0f, 0.2f, 0.2f, 1.0f);  // Red
    testEnemy.m_Stats.m_fMaxHP = 50.0f;
    testEnemy.m_Stats.m_fCurrentHP = 50.0f;
    testEnemy.m_Stats.m_fMoveSpeed = 4.0f;
    testEnemy.m_Stats.m_fAttackRange = 3.0f;
    testEnemy.m_Stats.m_fAttackCooldown = 1.5f;
    testEnemy.m_IndicatorConfig.m_eType = IndicatorType::Circle;
    testEnemy.m_IndicatorConfig.m_fHitRadius = 3.0f;
    testEnemy.m_fnCreateAttack = []() {
        return std::make_unique<MeleeAttackBehavior>(10.0f, 0.3f, 0.2f, 0.3f);
    };

    RegisterEnemyPreset("TestEnemy", testEnemy);

    // Shared animation config for Elemental enemies
    EnemyAnimationConfig elementalAnim;
    elementalAnim.m_strIdleClip    = "idle";
    elementalAnim.m_strChaseClip   = "Run_Forward";
    elementalAnim.m_strAttackClip  = "Combat_Unarmed_Attack";
    elementalAnim.m_strStaggerClip = "Combat_Stun";
    elementalAnim.m_strDeathClip   = "Death";

    // Register AirElemental preset (Melee - light blue air)
    EnemySpawnData airElemental;
    airElemental.m_strMeshPath      = "Assets/Enemies/Elementals/AirElemental_Bl/AirElemental_Bl.bin";
    airElemental.m_strAnimationPath = "Assets/Enemies/Elementals/AirElemental_Bl/AirElemental_Bl_Anim.bin";
    airElemental.m_strTexturePath   = "Assets/Enemies/Elementals/AirElemental_Bl/Textures/T_AirElemental_Body_Bl_D.png";
    airElemental.m_xmf3Scale = XMFLOAT3(2.0f, 2.0f, 2.0f);
    airElemental.m_xmf4Color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

    airElemental.m_Stats.m_fMaxHP          = 80.0f;
    airElemental.m_Stats.m_fCurrentHP      = 80.0f;
    airElemental.m_Stats.m_fMoveSpeed      = 5.0f;
    airElemental.m_Stats.m_fAttackRange    = 4.0f;
    airElemental.m_Stats.m_fAttackCooldown = 2.0f;

    airElemental.m_AnimConfig = elementalAnim;

    airElemental.m_IndicatorConfig.m_eType      = IndicatorType::Circle;
    airElemental.m_IndicatorConfig.m_fHitRadius = 4.0f;
    airElemental.m_fnCreateAttack = []() {
        return std::make_unique<MeleeAttackBehavior>(15.0f, 0.4f, 0.2f, 0.4f);
    };

    RegisterEnemyPreset("AirElemental", airElemental);

    // Register RushAoEEnemy preset (Rush + 360 AoE - FireGolem_Rd)
    EnemySpawnData rushAoE;
    rushAoE.m_strMeshPath      = "Assets/Enemies/Elementals/FireGolem_Rd/FireGolem_Rd.bin";
    rushAoE.m_strAnimationPath = "Assets/Enemies/Elementals/FireGolem_Rd/FireGolem_Rd_Anim.bin";
    rushAoE.m_strTexturePath   = "Assets/Enemies/Elementals/FireGolem_Rd/Textures/T_FireGolem_Rd_D.png";
    rushAoE.m_xmf3Scale = XMFLOAT3(2.0f, 2.0f, 2.0f);
    rushAoE.m_xmf4Color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

    rushAoE.m_Stats.m_fMaxHP          = 100.0f;
    rushAoE.m_Stats.m_fCurrentHP      = 100.0f;
    rushAoE.m_Stats.m_fMoveSpeed      = 5.0f;
    rushAoE.m_Stats.m_fAttackRange    = 20.0f;
    rushAoE.m_Stats.m_fAttackCooldown = 3.0f;

    rushAoE.m_AnimConfig = elementalAnim;

    // rushSpeed=15, rushDuration=1.2 → rushDistance=18
    rushAoE.m_IndicatorConfig.m_eType         = IndicatorType::RushCircle;
    rushAoE.m_IndicatorConfig.m_fRushDistance = 18.0f;
    rushAoE.m_IndicatorConfig.m_fHitRadius    = 5.0f;
    rushAoE.m_fnCreateAttack = []() {
        return std::make_unique<RushAoEAttackBehavior>(15.0f, 15.0f, 1.2f, 0.3f, 0.2f, 0.3f, 5.0f);
    };

    RegisterEnemyPreset("RushAoEEnemy", rushAoE);

    // Register RushFrontEnemy preset (Rush + cone - EarthElemental_Gn)
    EnemySpawnData rushFront;
    rushFront.m_strMeshPath      = "Assets/Enemies/Elementals/EarthElemental_Gn/EarthElemental_Gn.bin";
    rushFront.m_strAnimationPath = "Assets/Enemies/Elementals/EarthElemental_Gn/EarthElemental_Gn_Anim.bin";
    rushFront.m_strTexturePath   = "Assets/Enemies/Elementals/EarthElemental_Gn/Textures/T_EarthElemental_Gn_D.png";
    rushFront.m_xmf3Scale = XMFLOAT3(2.0f, 2.0f, 2.0f);
    rushFront.m_xmf4Color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

    rushFront.m_Stats.m_fMaxHP          = 80.0f;
    rushFront.m_Stats.m_fCurrentHP      = 80.0f;
    rushFront.m_Stats.m_fMoveSpeed      = 5.0f;
    rushFront.m_Stats.m_fAttackRange    = 18.0f;
    rushFront.m_Stats.m_fAttackCooldown = 2.5f;

    rushFront.m_AnimConfig = elementalAnim;

    // rushSpeed=18, rushDuration=1.0 → rushDistance=18
    rushFront.m_IndicatorConfig.m_eType         = IndicatorType::RushCone;
    rushFront.m_IndicatorConfig.m_fRushDistance = 18.0f;
    rushFront.m_IndicatorConfig.m_fHitRadius    = 4.0f;
    rushFront.m_IndicatorConfig.m_fConeAngle    = 90.0f;
    rushFront.m_fnCreateAttack = []() {
        return std::make_unique<RushFrontAttackBehavior>(20.0f, 18.0f, 1.0f, 0.2f, 0.2f, 0.3f, 4.0f, 90.0f);
    };

    RegisterEnemyPreset("RushFrontEnemy", rushFront);

    // Register RangedEnemy preset (Projectile - StormElemental_Bl)
    EnemySpawnData ranged;
    ranged.m_strMeshPath      = "Assets/Enemies/Elementals/StormElemental_Bl/StormElemental_Bl.bin";
    ranged.m_strAnimationPath = "Assets/Enemies/Elementals/StormElemental_Bl/StormElemental_Bl_Anim.bin";
    ranged.m_strTexturePath   = "Assets/Enemies/Elementals/StormElemental_Bl/Textures/T_StormElemental_Bl_D.png";
    ranged.m_xmf3Scale = XMFLOAT3(2.0f, 2.0f, 2.0f);
    ranged.m_xmf4Color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

    ranged.m_Stats.m_fMaxHP          = 60.0f;
    ranged.m_Stats.m_fCurrentHP      = 60.0f;
    ranged.m_Stats.m_fMoveSpeed      = 3.0f;
    ranged.m_Stats.m_fAttackRange    = 30.0f;
    ranged.m_Stats.m_fAttackCooldown = 2.0f;

    ranged.m_AnimConfig = elementalAnim;

    ProjectileManager* pProjMgr = pScene->GetProjectileManager();
    ranged.m_fnCreateAttack = [pProjMgr]() {
        return std::make_unique<RangedAttackBehavior>(pProjMgr, 10.0f, 20.0f, 0.5f, 0.1f, 0.5f);
    };

    RegisterEnemyPreset("RangedEnemy", ranged);

    // Register Dragon Boss preset (Flying)
    EnemySpawnData dragon;
    dragon.m_strMeshPath = "Assets/Enemies/Dragon/Red.bin";
    dragon.m_strAnimationPath = "Assets/Enemies/Dragon/Red_Anim.bin";
    dragon.m_strTexturePath = "Assets/Enemies/Dragon/Textures/RedHP.png";
    dragon.m_xmf3Scale = XMFLOAT3(3.0f, 3.0f, 3.0f);  // 더 크게
    dragon.m_xmf4Color = XMFLOAT4(1.0f, 0.3f, 0.1f, 1.0f);
    dragon.m_Stats.m_fMaxHP = 800.0f;           // HP 대폭 상향
    dragon.m_Stats.m_fCurrentHP = 800.0f;
    dragon.m_Stats.m_fMoveSpeed = 10.0f;        // 이동속도 상향
    dragon.m_Stats.m_fAttackRange = 40.0f;      // 공격 사거리 상향 (브레스 사거리)
    dragon.m_Stats.m_fAttackCooldown = 1.5f;    // 공격 쿨다운 감소
    dragon.m_Stats.m_fLongRangeThreshold = 35.0f;   // 원거리 기준
    dragon.m_Stats.m_fMidRangeThreshold = 18.0f;    // 중거리 기준

    // Flying mode disabled - boss intro handles the entrance
    dragon.m_bIsFlying = false;
    dragon.m_fFlyHeight = 0.0f;

    // Boss settings - immune to stagger, has special attack
    dragon.m_bIsBoss = true;
    dragon.m_fSpecialAttackCooldown = 4.0f;    // 특수 공격 쿨다운 감소
    dragon.m_fFlyingAttackCooldown = 8.0f;     // 비행 공격 쿨다운 감소
    dragon.m_nSpecialAttackChance = 45;        // 특수 공격 확률 증가
    dragon.m_nFlyingAttackChance = 50;         // 비행 공격 확률 증가

    // Ground combat animations
    dragon.m_AnimConfig.m_strIdleClip = "Idle01";
    dragon.m_AnimConfig.m_strChaseClip = "Walk";
    dragon.m_AnimConfig.m_strAttackClip = "Flame Attack";
    dragon.m_AnimConfig.m_strStaggerClip = "Get Hit";
    dragon.m_AnimConfig.m_strDeathClip = "Die";

    dragon.m_IndicatorConfig.m_eType = IndicatorType::Circle;
    dragon.m_IndicatorConfig.m_fHitRadius = 15.0f;

    // Normal attack: Breath attack (reduced projectile count for performance)
    dragon.m_fnCreateAttack = [pProjMgr]() {
        // scale 3.0 (뭉친 클러스터 크기 up)
        return std::make_unique<BreathAttackBehavior>(pProjMgr, 32.0f, 38.0f, 5, 50.0f, 0.4f, 0.8f, 0.3f, 1.0f, 3.0f);
    };

    // Special attack (fallback if no phase controller, reduced wave count)
    dragon.m_fnCreateSpecialAttack = [pProjMgr]() {
        return std::make_unique<FlyingBarrageAttackBehavior>(
            pProjMgr, 26.0f, 18.0f, 12, 4, 0.35f, 16.0f, 0.8f, 0.8f);
    };

    // Boss Phase Configuration - 3 phases with varied attack patterns
    dragon.m_fnCreateBossPhaseConfig = [pProjMgr]() {
        auto pConfig = std::make_unique<BossPhaseConfig>();

        // ============ Phase 1 (100% - 70% HP): Ground Combat ============
        BossPhaseData phase1;
        phase1.m_fHealthThreshold = 1.0f;
        phase1.m_fSpeedMultiplier = 1.0f;
        phase1.m_fAttackSpeedMultiplier = 1.0f;
        phase1.m_nSpecialAttackChance = 35;
        phase1.m_bCanFly = false;

        // Primary: Breath attack (reduced projectiles for performance)
        phase1.m_fnPrimaryAttack = [pProjMgr]() {
            return std::make_unique<BreathAttackBehavior>(pProjMgr, 30.0f, 35.0f, 4, 45.0f, 0.4f, 0.8f, 0.3f);
        };

        // Special: Randomly choose between tail sweep and claw combo
        phase1.m_fnSpecialAttack = []() -> std::unique_ptr<IAttackBehavior> {
            int choice = rand() % 3;
            if (choice == 0) {
                // Tail sweep - wide arc behind (상향된 데미지)
                return std::make_unique<TailSweepAttackBehavior>(35.0f, 0.35f, 0.25f, 0.35f, 8.0f, 180.0f, true);
            } else if (choice == 1) {
                // Jump slam (상향된 데미지)
                return std::make_unique<JumpSlamAttackBehavior>(45.0f, 10.0f, 0.45f, 7.0f, 0.25f, 0.4f, true);
            } else {
                // Light combo (3-hit)
                return std::unique_ptr<IAttackBehavior>(ComboAttackBehavior::CreateLightCombo());
            }
        };

        pConfig->AddPhase(phase1);

        // ============ Phase 2 (70% - 35% HP): Aerial Assault ============
        BossPhaseData phase2;
        phase2.m_fHealthThreshold = 0.7f;
        phase2.m_fSpeedMultiplier = 1.3f;     // 속도 증가
        phase2.m_fAttackSpeedMultiplier = 0.85f;
        phase2.m_nSpecialAttackChance = 45;
        phase2.m_nFlyingAttackChance = 55;    // 비행 공격 확률 증가
        phase2.m_bCanFly = true;

        // Primary: Faster breath (reduced projectiles for performance)
        phase2.m_fnPrimaryAttack = [pProjMgr]() {
            return std::make_unique<BreathAttackBehavior>(pProjMgr, 35.0f, 38.0f, 5, 50.0f, 0.35f, 0.75f, 0.25f);
        };

        // Special: Ground attacks (상향된 데미지)
        phase2.m_fnSpecialAttack = []() -> std::unique_ptr<IAttackBehavior> {
            int choice = rand() % 2;
            if (choice == 0) {
                return std::make_unique<JumpSlamAttackBehavior>(50.0f, 12.0f, 0.45f, 8.0f, 0.2f, 0.35f, true);
            } else {
                return std::unique_ptr<IAttackBehavior>(ComboAttackBehavior::CreateHeavyCombo());
            }
        };

        // Flying: Strafe or Circle attack (reduced projectiles for performance)
        phase2.m_fnFlyingAttack = [pProjMgr]() -> std::unique_ptr<IAttackBehavior> {
            int choice = rand() % 2;
            if (choice == 0) {
                // Strafe - side movement while shooting at player
                return std::make_unique<FlyingStrafeAttackBehavior>(
                    pProjMgr, 24.0f, 22.0f, 18.0f, 22.0f, 0.15f, 3, 12.0f, 0.4f, 0.4f);
            } else {
                // Circle - orbit around player (shorter, faster)
                return std::make_unique<FlyingCircleAttackBehavior>(
                    pProjMgr, 22.0f, 22.0f, 18.0f, 100.0f, 280.0f, 0.18f, 3, 12.0f, 0.4f, 0.4f);
            }
        };

        // Phase 2 transition: Mega Breath attack
        phase2.m_fnTransitionAttack = []() {
            return std::make_unique<MegaBreathAttackBehavior>(
                15.0f,  // 틱당 데미지
                0.2f,   // 틱 간격
                20.0f,  // 이동 속도
                2.0f,   // 벽 이동 시간
                2.0f,   // 준비 시간
                4.0f,   // 브레스 지속
                1.0f,   // 회복 시간
                3.0f    // 엄폐물 크기
            );
        };
        phase2.m_bHasTransitionAttack = true;
        phase2.m_bInvincibleDuringTransition = true;
        phase2.m_fTransitionDuration = 0.0f;  // MegaBreath handles its own timing

        pConfig->AddPhase(phase2);

        // ============ Phase 3 (35% - 0% HP): Fury Mode ============
        BossPhaseData phase3;
        phase3.m_fHealthThreshold = 0.35f;
        phase3.m_fSpeedMultiplier = 1.6f;     // 더 빠르게
        phase3.m_fAttackSpeedMultiplier = 0.7f;
        phase3.m_nSpecialAttackChance = 55;
        phase3.m_nFlyingAttackChance = 65;    // 비행 공격 확률 대폭 증가
        phase3.m_bCanFly = true;

        // Primary: Rapid breath (reduced projectiles for performance)
        phase3.m_fnPrimaryAttack = [pProjMgr]() {
            return std::make_unique<BreathAttackBehavior>(pProjMgr, 42.0f, 42.0f, 6, 55.0f, 0.25f, 0.65f, 0.2f);
        };

        // Special: Fury combo or double jump slam (상향된 데미지)
        phase3.m_fnSpecialAttack = []() -> std::unique_ptr<IAttackBehavior> {
            int choice = rand() % 2;
            if (choice == 0) {
                return std::unique_ptr<IAttackBehavior>(ComboAttackBehavior::CreateFuryCombo());
            } else {
                return std::make_unique<JumpSlamAttackBehavior>(60.0f, 14.0f, 0.4f, 9.0f, 0.18f, 0.3f, true);
            }
        };

        // Flying: All aerial attacks available (reduced projectiles for performance)
        phase3.m_fnFlyingAttack = [pProjMgr]() -> std::unique_ptr<IAttackBehavior> {
            int choice = rand() % 4;
            switch (choice) {
            case 0:
                // Dive bomb - dive at player while shooting
                return std::make_unique<DiveBombAttackBehavior>(
                    pProjMgr, 30.0f, 32.0f, 36.0f, 40.0f, 7.0f, 4, 0.1f, 18.0f, 0.4f, 0.25f);
            case 1:
                // Sweep - side-to-side fire while moving
                return std::make_unique<FlyingSweepAttackBehavior>(
                    pProjMgr, 26.0f, 28.0f, 18.0f, 28.0f, 100.0f, 200.0f, 0.08f, 2, 10.0f, 0.35f, 0.35f);
            case 2:
                // Barrage - fewer but faster waves
                return std::make_unique<FlyingBarrageAttackBehavior>(
                    pProjMgr, 28.0f, 20.0f, 10, 4, 0.3f, 16.0f, 0.5f, 0.5f);
            default:
                // Fast strafe
                return std::make_unique<FlyingStrafeAttackBehavior>(
                    pProjMgr, 26.0f, 26.0f, 22.0f, 25.0f, 0.12f, 3, 12.0f, 0.35f, 0.35f);
            }
        };

        // Phase 3 transition: Stronger Mega Breath attack
        phase3.m_fnTransitionAttack = []() {
            return std::make_unique<MegaBreathAttackBehavior>(
                25.0f,  // 틱당 데미지 (더 강함)
                0.15f,  // 틱 간격 (더 빠름)
                25.0f,  // 이동 속도
                1.5f,   // 벽 이동 시간 (더 빠름)
                1.5f,   // 준비 시간 (더 빠름)
                5.0f,   // 브레스 지속 (더 길게)
                0.8f,   // 회복 시간
                3.5f    // 엄폐물 크기
            );
        };
        phase3.m_bHasTransitionAttack = true;
        phase3.m_bInvincibleDuringTransition = true;
        phase3.m_fTransitionDuration = 0.0f;  // MegaBreath handles its own timing

        pConfig->AddPhase(phase3);

        return pConfig;
    };

    RegisterEnemyPreset("Dragon", dragon);

    // Register Kraken Boss preset (Water stage boss — 느릿 + 넓은 범위 + 4패턴)
    EnemySpawnData kraken;
    kraken.m_strMeshPath      = "Assets/Enemies/Kraken/KRAKEN.bin";
    kraken.m_strAnimationPath = "Assets/Enemies/Kraken/KRAKEN_Anim.bin";
    kraken.m_strTexturePath   = "Assets/Enemies/Kraken/Textures/Tex_KRAKEN_BODY_BaseColor.png";
    kraken.m_xmf3Scale = XMFLOAT3(3.0f, 3.0f, 3.0f);
    kraken.m_xmf4Color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    kraken.m_fColliderXZMultiplier = 0.8f;   // 거대 몸체에 맞춰 XZ 피격 반경 확대

    kraken.m_Stats.m_fMaxHP              = 1000.0f;
    kraken.m_Stats.m_fCurrentHP          = 1000.0f;
    kraken.m_Stats.m_fMoveSpeed          = 5.0f;
    kraken.m_Stats.m_fAttackRange        = 30.0f;   // Breath 가 기본이라 사정거리 확보
    kraken.m_Stats.m_fAttackCooldown     = 1.6f;    // 빠른 견제 발사 텀
    kraken.m_Stats.m_fLongRangeThreshold = 40.0f;
    kraken.m_Stats.m_fMidRangeThreshold  = 18.0f;

    kraken.m_bIsBoss = true;
    kraken.m_fSpecialAttackCooldown = 4.5f;
    kraken.m_nSpecialAttackChance   = 70;     // 쿨 끝나면 70% 확률 특수기 (나머지는 Breath 지속)
    // 사각형 판정이 보스 중심(z=0)부터 전방으로 뻗어나가므로 offset 0 으로
    kraken.m_fAttackOriginForwardOffset = 0.0f;

    kraken.m_AnimConfig.m_strIdleClip    = "Idle";
    kraken.m_AnimConfig.m_strChaseClip   = "Walk";
    kraken.m_AnimConfig.m_strAttackClip  = "Attack_Forward_RM";   // 기본 = 잉크 발사 애니
    kraken.m_AnimConfig.m_strStaggerClip = "Hit";
    kraken.m_AnimConfig.m_strDeathClip   = "Death";

    // 전방 직사각형 인디케이터 — 촉수가 앞으로 휘두르는 과장된 범위
    kraken.m_IndicatorConfig.m_eType      = IndicatorType::ForwardBox;
    kraken.m_IndicatorConfig.m_fHitRadius = 14.0f;   // 반폭 (총 너비 28u)
    kraken.m_IndicatorConfig.m_fHitLength = 30.0f;   // 전방 30u — 과장된 촉수 휩쓸기 범위

    // ── 기본 공격 = 작은 잉크 투사체 다수 지속 발사 (견제기 역할) ────────────────
    //   · projectileCount 10 (많이)
    //   · projectileScale 1.0 (기본 작게) — 변주 시 0.55~1.9 배로 다양화
    //   · bVariedProjectiles=true 로 크기/속도/각도/데미지/발사 위치 모두 랜덤 변주
    kraken.m_fnCreateAttack = [pProjMgr]() {
        return std::make_unique<BreathAttackBehavior>(
            pProjMgr,
            7.0f,     // dmgPerHit (평균 — ±30% 변주)
            34.0f,    // projectileSpeed (평균 — 0.75~1.45 배)
            10,       // projectileCount (많이)
            55.0f,    // spreadAngle (넓게 뿌림)
            0.4f,     // windup (빠름)
            1.1f,     // breath duration
            0.2f,     // recovery
            0.6f,     // projectileRadius (평균)
            1.0f,     // projectileScale 기본 — 변주로 0.55~1.9 배
            ElementType::Water,
            "Attack_Forward_RM",
            true);    // ★ varied projectiles: 크기/속도/각/발사 위치 랜덤
    };

    // ── 특수기 팩토리: 3종 랜덤 (TailSweep / HeavyCombo / 360 탄막) ───────────
    kraken.m_fnCreateSpecialAttack = [pProjMgr]() -> std::unique_ptr<IAttackBehavior> {
        int roll = rand() % 100;
        if (roll < 45)
        {
            // 광역 휩쓸기 — 앞쪽 사각형 (14×30 확장)
            return std::make_unique<TailSweepAttackBehavior>(
                55.0f,   // dmg
                0.8f,    // windup
                0.5f,    // sweep duration
                0.7f,    // recovery
                14.0f,   // hitRange (미사용 — rect 모드)
                180.0f,  // sweepArc (미사용)
                false,
                "Sweep_Attack",
                14.0f,   // rectWidthHalf — 반폭 14 (총 28u)
                30.0f);  // rectLength — 전방 30u
        }
        else if (roll < 75)
        {
            // 3연타 필살 콤보 — 사각형 판정 (14×30)
            std::vector<ComboAttackBehavior::ComboHit> hits;
            ComboAttackBehavior::ComboHit h;
            h.strAnimation = "Sweep_Smash_Attack_3_HIt_Combo";
            h.fHitRange      = 14.0f;
            h.fConeAngle     = 160.0f;
            h.fRectWidthHalf = 14.0f;
            h.fRectLength    = 30.0f;
            h.bTrackTarget = true;
            h.fDamage = 30.0f; h.fWindupTime = 0.7f; h.fHitTime = 0.2f; h.fRecoveryTime = 0.6f;
            hits.push_back(h);
            h.bTrackTarget = false;
            h.fDamage = 55.0f; h.fWindupTime = 0.8f; h.fHitTime = 0.2f; h.fRecoveryTime = 0.5f;
            hits.push_back(h);
            h.fDamage = 85.0f; h.fWindupTime = 0.9f; h.fHitTime = 0.3f; h.fRecoveryTime = 0.6f;
            hits.push_back(h);
            return std::make_unique<ComboAttackBehavior>(hits);
        }
        else
        {
            // 360° 탄막 — 뒤에 숨은 플레이어 견제 + 혼란스러운 다양한 투사체
            //   spread 360°로 전 방향 스프레이, count 많음, 변주 활성
            //   지면 AoE 가 아니니 인디케이터 억제됨 (false 전달)
            auto pBehavior = std::make_unique<BreathAttackBehavior>(
                pProjMgr,
                10.0f,    // dmg/hit
                28.0f,    // speed (평균)
                16,       // count (많음)
                360.0f,   // spread — 전 방향
                0.9f,     // windup (좀 더 길게 — 몸을 웅크리는 느낌)
                1.6f,     // duration (계속 뿜음)
                0.4f,     // recovery
                0.8f,     // radius
                1.1f,     // scale 기본
                ElementType::Water,
                "Unreal Take",  // 포효 동시 분사 — 몸을 벌리며 뿜는 느낌
                true);    // varied
            return pBehavior;
        }
    };

    RegisterEnemyPreset("Kraken", kraken);

    // Register Golem Boss preset (Earth stage boss - stationary colossus like Mordeum)
    EnemySpawnData golem;
    golem.m_strMeshPath      = "Assets/Enemies/golem/Golem01_Generic_prefab.bin";
    golem.m_strAnimationPath = "Assets/Enemies/golem/Golem01_Generic_prefab_Anim.bin";
    golem.m_strTexturePath   = "Assets/Enemies/golem/Textures/chr_04_Golem_alb.png";
    golem.m_xmf3Scale = XMFLOAT3(8.0f, 8.0f, 8.0f);  // 거대 보스
    golem.m_xmf4Color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

    golem.m_Stats.m_fMaxHP              = 2000.0f;
    golem.m_Stats.m_fCurrentHP          = 2000.0f;
    golem.m_Stats.m_fMoveSpeed          = 1.0f;   // 거의 안 움직임
    golem.m_Stats.m_fAttackRange        = 22.0f;  // 큰 체구에 맞는 넓은 범위
    golem.m_Stats.m_fAttackCooldown     = 3.0f;   // 느린 공격
    golem.m_Stats.m_fLongRangeThreshold = 40.0f;
    golem.m_Stats.m_fMidRangeThreshold  = 22.0f;

    golem.m_bIsBoss = true;
    golem.m_fSpecialAttackCooldown = 8.0f;
    golem.m_nSpecialAttackChance   = 50;

    golem.m_AnimConfig.m_strIdleClip    = "Golem_battle_stand_ge";
    golem.m_AnimConfig.m_strChaseClip   = "Golem_battle_walk_ge";
    golem.m_AnimConfig.m_strAttackClip  = "Golem_battle_attack01_ge";
    golem.m_AnimConfig.m_strStaggerClip = "Golem_battle_damage_ge";
    golem.m_AnimConfig.m_strDeathClip   = "Golem_battle_die_ge";

    golem.m_IndicatorConfig.m_eType      = IndicatorType::Circle;
    golem.m_IndicatorConfig.m_fHitRadius = 18.0f;

    // 광역 내려찍기 - 범위 넓고 데미지 강함
    golem.m_fnCreateAttack = []() {
        return std::make_unique<JumpSlamAttackBehavior>(80.0f, 0.0f, 0.6f, 12.0f, 0.3f, 0.5f, true);
    };
    // 특수: 전방 360도 지진 스텝
    golem.m_fnCreateSpecialAttack = []() {
        return std::make_unique<TailSweepAttackBehavior>(100.0f, 0.5f, 0.4f, 0.6f, 15.0f, 360.0f, true);
    };

    RegisterEnemyPreset("Golem", golem);

    // Register Demon Boss preset (Grass/Wind stage boss - agile dark druid)
    EnemySpawnData demon;
    demon.m_strMeshPath      = "Assets/Enemies/demon/Demon.bin";
    demon.m_strAnimationPath = "Assets/Enemies/demon/Demon_Anim.bin";
    demon.m_strTexturePath   = "Assets/Enemies/demon/Textures/_Albedo.png";
    demon.m_xmf3Scale = XMFLOAT3(3.5f, 3.5f, 3.5f);
    demon.m_xmf4Color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

    demon.m_Stats.m_fMaxHP              = 1500.0f;
    demon.m_Stats.m_fCurrentHP          = 1500.0f;
    demon.m_Stats.m_fMoveSpeed          = 9.0f;   // 빠르고 공격적
    demon.m_Stats.m_fAttackRange        = 12.0f;
    demon.m_Stats.m_fAttackCooldown     = 1.5f;
    demon.m_Stats.m_fLongRangeThreshold = 30.0f;
    demon.m_Stats.m_fMidRangeThreshold  = 15.0f;

    demon.m_bIsBoss = true;
    demon.m_fSpecialAttackCooldown = 5.0f;
    demon.m_nSpecialAttackChance   = 45;

    demon.m_AnimConfig.m_strIdleClip    = "Idle1";
    demon.m_AnimConfig.m_strChaseClip   = "Run";
    demon.m_AnimConfig.m_strAttackClip  = "attack1";
    demon.m_AnimConfig.m_strStaggerClip = "gethit1";
    demon.m_AnimConfig.m_strDeathClip   = "Death1";

    demon.m_IndicatorConfig.m_eType      = IndicatorType::Circle;
    demon.m_IndicatorConfig.m_fHitRadius = 10.0f;

    demon.m_fnCreateAttack = []() {
        return std::make_unique<MeleeAttackBehavior>(50.0f, 0.3f, 0.2f, 0.4f);
    };
    demon.m_fnCreateSpecialAttack = [pProjMgr]() {
        return std::make_unique<RushFrontAttackBehavior>(70.0f, 20.0f, 1.0f, 0.2f, 0.2f, 0.3f, 6.0f, 80.0f);
    };

    RegisterEnemyPreset("Demon", demon);

    // Register Blue Dragon preset (Water boss Phase 1)
    EnemySpawnData blueDragon;
    blueDragon.m_strMeshPath      = "Assets/Enemies/Dragon_blue/Blue.bin";
    blueDragon.m_strAnimationPath = "Assets/Enemies/Dragon_blue/Blue_Anim.bin";
    blueDragon.m_strTexturePath   = "Assets/Enemies/Dragon_blue/Textures/BlueHP.png";
    blueDragon.m_xmf3Scale = XMFLOAT3(3.0f, 3.0f, 3.0f);
    blueDragon.m_xmf4Color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    blueDragon.m_fColliderXZMultiplier = 1.0f;   // 뚱뚱한 몸집에 맞게 피격 판정 확대 (기본 0.3 → 1.0)

    blueDragon.m_Stats.m_fMaxHP              = 80.0f;
    blueDragon.m_Stats.m_fCurrentHP          = 80.0f;
    blueDragon.m_Stats.m_fMoveSpeed          = 9.0f;
    blueDragon.m_Stats.m_fAttackRange        = 35.0f;
    blueDragon.m_Stats.m_fAttackCooldown     = 2.2f;  // 기본 공격 텀 넉넉하게
    blueDragon.m_Stats.m_fLongRangeThreshold = 30.0f;
    blueDragon.m_Stats.m_fMidRangeThreshold  = 15.0f;

    blueDragon.m_bIsFlying = false;
    blueDragon.m_fFlyHeight = 0.0f;
    blueDragon.m_bIsBoss = true;
    blueDragon.m_fSpecialAttackCooldown = 3.0f;   // 특수기 자주 나오게
    blueDragon.m_nSpecialAttackChance   = 60;     // 쿨다운 끝나면 60% 확률로 특수기
    blueDragon.m_nFlyingAttackChance    = 0;       // 페이즈 컨트롤러 없으면 작동 안 함

    blueDragon.m_AnimConfig.m_strIdleClip    = "Idle";
    blueDragon.m_AnimConfig.m_strChaseClip   = "Walk";
    blueDragon.m_AnimConfig.m_strAttackClip  = "Fireball Shoot";  // 브레스 전용 애니 (Basic Attack은 근접 물기)
    blueDragon.m_AnimConfig.m_strStaggerClip = "Get Hit";
    blueDragon.m_AnimConfig.m_strDeathClip   = "Die";
    blueDragon.m_AnimConfig.m_bLoopAttack    = true;  // 행동 지속 시간 동안 공격 포즈 유지

    blueDragon.m_IndicatorConfig.m_eType      = IndicatorType::Circle;
    blueDragon.m_IndicatorConfig.m_fHitRadius = 14.0f;

    // 기본 공격: 레드 드래곤과 완전 동일 파라미터, 색상만 Water
    blueDragon.m_fnCreateAttack = [pProjMgr]() {
        return std::make_unique<BreathAttackBehavior>(
            pProjMgr, 32.0f, 38.0f, 5, 50.0f, 0.4f, 0.8f, 0.3f, 1.0f, 3.0f, ElementType::Water);
    };

    // 특수 공격: 뚱뚱한 몸집에 맞는 묵직하고 느린 패턴들
    blueDragon.m_fnCreateSpecialAttack = []() -> std::unique_ptr<IAttackBehavior> {
        int choice = rand() % 4;
        switch (choice) {
        case 0:
            // 꼬리 휩쓸기 - 크고 느린 호, 긴 선딜
            return std::make_unique<TailSweepAttackBehavior>(32.0f, 0.65f, 0.45f, 0.6f, 10.0f, 200.0f, true);
        case 1:
            // 점프 슬램 - 낮고 느린 점프, 무거운 착지. 도약 높이 줄이고 착지 후딜 길게
            return std::make_unique<JumpSlamAttackBehavior>(42.0f, 6.0f, 0.7f, 8.0f, 0.5f, 0.7f, true);
        case 2:
            // 3연타 - 묵직하고 간격 넓게 (LightCombo 파라미터 오버라이드 불가하므로 HeavyCombo 사용)
            return std::unique_ptr<IAttackBehavior>(ComboAttackBehavior::CreateHeavyCombo());
        default:
            // 느릿한 돌진 - 짧은 거리, 낮은 속도
            return std::make_unique<RushFrontAttackBehavior>(40.0f, 10.0f, 1.0f, 0.35f, 0.35f, 0.5f, 6.0f, 60.0f);
        }
    };

    RegisterEnemyPreset("BlueDragon", blueDragon);

    // Create shared meshes for attack indicators
    // m_pRingMesh = 얇은 테두리 링 (공격 범위 윤곽) — 공격 내내 고정 표시
    m_pRingMesh = new RingMesh(pDevice, pCommandList, 1.0f, 0.96f, 48);   // 0.88 → 0.96 (더 얇게)
    m_pRingMesh->AddRef();
    // m_pDiscMesh = 꽉 찬 원판 (공격 타이밍에 맞춰 차오르는 fill)
    m_pDiscMesh = new RingMesh(pDevice, pCommandList, 1.0f, 0.0f, 48);
    m_pDiscMesh->AddRef();
    m_pLineMesh = new LineMesh(pDevice, pCommandList, 0.4f);
    m_pLineMesh->AddRef();

    m_pFanMesh = new FanMesh(pDevice, pCommandList, 90.0f, 24);
    m_pFanMesh->AddRef();

    // ForwardBox 전방 직사각형용 flat cube — 단위 크기, 실제 크기는 Transform 스케일로
    m_pBoxMesh = new CubeMesh(pDevice, pCommandList, 1.0f, 0.02f, 1.0f);
    m_pBoxMesh->AddRef();

    OutputDebugString(L"[EnemySpawner] Initialized with default presets\n");
}

void EnemySpawner::RegisterEnemyPreset(const std::string& name, const EnemySpawnData& data)
{
    m_mapPresets[name] = data;

    wchar_t buffer[128];
    swprintf_s(buffer, L"[EnemySpawner] Registered preset: %hs\n", name.c_str());
    OutputDebugString(buffer);
}

bool EnemySpawner::HasPreset(const std::string& name) const
{
    return m_mapPresets.find(name) != m_mapPresets.end();
}

GameObject* EnemySpawner::SpawnEnemy(CRoom* pRoom, const std::string& preset, const XMFLOAT3& position, GameObject* pTarget)
{
    auto it = m_mapPresets.find(preset);
    if (it == m_mapPresets.end())
    {
        wchar_t buffer[128];
        swprintf_s(buffer, L"[EnemySpawner] Preset not found: %hs, using TestEnemy\n", preset.c_str());
        OutputDebugString(buffer);

        // Fallback to test enemy
        return SpawnTestEnemy(pRoom, position, pTarget);
    }

    const EnemySpawnData& data = it->second;

    // Create enemy game object
    GameObject* pEnemy = nullptr;

    if (data.m_strMeshPath.empty())
    {
        // Use CubeMesh
        pEnemy = CreateCubeEnemy(pRoom, position, data.m_xmf3Scale, data.m_xmf4Color);
    }
    else
    {
        // Load mesh from file
        pEnemy = CreateMeshEnemy(pRoom, position, data);
    }

    if (pEnemy)
    {
        SetupEnemyComponents(pEnemy, data, pRoom, pTarget);
    }

    return pEnemy;
}

GameObject* EnemySpawner::SpawnTestEnemy(CRoom* pRoom, const XMFLOAT3& position, GameObject* pTarget)
{
    return SpawnEnemy(pRoom, "TestEnemy", position, pTarget);
}

void EnemySpawner::SpawnRoomEnemies(CRoom* pRoom, const RoomSpawnConfig& config, GameObject* pTarget)
{
    if (!pRoom) return;

    for (const auto& spawn : config.m_vEnemySpawns)
    {
        SpawnEnemy(pRoom, spawn.first, spawn.second, pTarget);
    }

    wchar_t buffer[128];
    swprintf_s(buffer, L"[EnemySpawner] Spawned %zu enemies in room\n", config.m_vEnemySpawns.size());
    OutputDebugString(buffer);
}

GameObject* EnemySpawner::CreateCubeEnemy(CRoom* pRoom, const XMFLOAT3& position, const XMFLOAT3& scale, const XMFLOAT4& color)
{
    if (!m_pDevice || !m_pCommandList || !m_pScene) return nullptr;

    // Create game object via Scene (handles descriptor allocation)
    GameObject* pEnemy = m_pScene->CreateGameObject(m_pDevice, m_pCommandList);
    if (!pEnemy) return nullptr;

    // Set position and scale
    TransformComponent* pTransform = pEnemy->GetTransform();
    if (pTransform)
    {
        pTransform->SetPosition(position);
        pTransform->SetScale(scale);
    }

    // Create cube mesh (1x1x1 base, scaled by transform)
    CubeMesh* pCubeMesh = new CubeMesh(m_pDevice, m_pCommandList, 2.0f, 2.0f, 2.0f);
    pCubeMesh->AddRef();
    pEnemy->SetMesh(pCubeMesh);

    // Set material (red color)
    MATERIAL material;
    material.m_cAmbient = XMFLOAT4(color.x * 0.3f, color.y * 0.3f, color.z * 0.3f, 1.0f);
    material.m_cDiffuse = color;
    material.m_cSpecular = XMFLOAT4(0.5f, 0.5f, 0.5f, 32.0f);
    material.m_cEmissive = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
    pEnemy->SetMaterial(material);

    // Add RenderComponent
    if (m_pShader)
    {
        auto* pRenderComp = pEnemy->AddComponent<RenderComponent>();
        pRenderComp->SetMesh(pCubeMesh);
        m_pShader->AddRenderComponent(pRenderComp);
    }

    // Add ColliderComponent (reduced size to avoid getting stuck on terrain)
    auto* pCollider = pEnemy->AddComponent<ColliderComponent>();
    pCollider->SetExtents(scale.x * 0.5f, scale.y, scale.z * 0.5f);  // Half extents, narrower XZ
    pCollider->SetCenter(0.0f, scale.y, 0.0f);  // Center at mid-height
    pCollider->SetLayer(CollisionLayer::Enemy);
    pCollider->SetCollisionMask(CollisionMask::Enemy);

    wchar_t buffer[128];
    swprintf_s(buffer, L"[EnemySpawner] Created cube enemy at (%.1f, %.1f, %.1f)\n",
        position.x, position.y, position.z);
    OutputDebugString(buffer);

    return pEnemy;
}

void EnemySpawner::SetupEnemyComponents(GameObject* pEnemy, const EnemySpawnData& data, CRoom* pRoom, GameObject* pTarget)
{
    if (!pEnemy) return;

    // Add EnemyComponent
    auto* pEnemyComp = pEnemy->AddComponent<EnemyComponent>();
    pEnemyComp->SetStats(data.m_Stats);
    pEnemyComp->SetTarget(pTarget);
    pEnemyComp->SetRoom(pRoom);
    pEnemyComp->SetAttackOriginForwardOffset(data.m_fAttackOriginForwardOffset);

    // Set flying mode if enabled
    if (data.m_bIsFlying)
    {
        pEnemyComp->SetFlying(true, data.m_fFlyHeight);
    }

    // Create attack behavior (+ store factory for per-use recreation)
    if (data.m_fnCreateAttack)
    {
        pEnemyComp->SetAttackBehavior(data.m_fnCreateAttack());
        pEnemyComp->SetAttackFactory(data.m_fnCreateAttack);
    }
    else
    {
        // Default melee attack
        pEnemyComp->SetAttackBehavior(std::make_unique<MeleeAttackBehavior>());
    }

    // Boss settings
    if (data.m_bIsBoss)
    {
        pEnemyComp->SetBoss(true);
        pEnemyComp->SetSpecialAttackCooldown(data.m_fSpecialAttackCooldown);
        pEnemyComp->SetSpecialAttackChance(data.m_nSpecialAttackChance);
        pEnemyComp->SetFlyingAttackCooldown(data.m_fFlyingAttackCooldown);
        pEnemyComp->SetFlyingAttackChance(data.m_nFlyingAttackChance);

        // Create special attack behavior + store factory for per-use recreation
        if (data.m_fnCreateSpecialAttack)
        {
            pEnemyComp->SetSpecialAttackBehavior(data.m_fnCreateSpecialAttack());
            pEnemyComp->SetSpecialAttackFactory(data.m_fnCreateSpecialAttack);
            OutputDebugString(L"[EnemySpawner] Boss special attack behavior set\n");
        }

        // Setup boss phase controller if config factory is provided
        if (data.m_fnCreateBossPhaseConfig)
        {
            auto pPhaseConfig = data.m_fnCreateBossPhaseConfig();
            if (pPhaseConfig)
            {
                auto pPhaseController = std::make_unique<BossPhaseController>(pEnemyComp);
                pPhaseController->SetPhaseConfig(std::move(pPhaseConfig));
                pEnemyComp->SetBossPhaseController(std::move(pPhaseController));
                OutputDebugString(L"[EnemySpawner] Boss phase controller set\n");
            }
        }
    }

    // Set death callback to notify room
    pEnemyComp->SetOnDeathCallback([pRoom](EnemyComponent* pDeadEnemy) {
        if (pRoom)
        {
            pRoom->OnEnemyDeath(pDeadEnemy);
        }
    });

    // Register enemy with room
    if (pRoom)
    {
        pRoom->RegisterEnemy(pEnemyComp);
    }

    // Connect AnimationComponent if present
    auto* pAnimComp = pEnemy->GetComponent<AnimationComponent>();
    if (pAnimComp)
    {
        pEnemyComp->SetAnimationComponent(pAnimComp);
        pEnemyComp->SetAnimationConfig(data.m_AnimConfig);

        // Apply random time offset to desync animations between enemies
        float fRandomOffset = (float)(rand() % 1000) / 100.0f;  // 0.0 ~ 10.0 seconds
        pAnimComp->SetTimeOffset(fRandomOffset);

        pAnimComp->Play(data.m_AnimConfig.m_strIdleClip, data.m_AnimConfig.m_bLoopIdle);
    }

    // Create attack indicators
    if (data.m_IndicatorConfig.m_eType != IndicatorType::None && pRoom)
    {
        SetupAttackIndicators(pEnemy, pEnemyComp, data.m_IndicatorConfig, pRoom);
    }

    OutputDebugString(L"[EnemySpawner] Setup enemy components complete\n");
}

GameObject* EnemySpawner::CreateMeshEnemy(CRoom* pRoom, const XMFLOAT3& position, const EnemySpawnData& data)
{
    if (!m_pDevice || !m_pCommandList || !m_pScene) return nullptr;

    // Temporarily set current room to place enemy in room
    CRoom* pPrevRoom = m_pScene->GetCurrentRoom();
    m_pScene->SetCurrentRoom(pRoom);

    // Load mesh from file
    GameObject* pEnemy = MeshLoader::LoadGeometryFromFile(m_pScene, m_pDevice, m_pCommandList, NULL, data.m_strMeshPath.c_str());

    // Restore previous room
    m_pScene->SetCurrentRoom(pPrevRoom);

    if (!pEnemy)
    {
        wchar_t buffer[256];
        swprintf_s(buffer, L"[EnemySpawner] Failed to load mesh: %hs\n", data.m_strMeshPath.c_str());
        OutputDebugString(buffer);
        return nullptr;
    }

    // Set position and scale
    TransformComponent* pTransform = pEnemy->GetTransform();
    if (pTransform)
    {
        pTransform->SetPosition(position);
        pTransform->SetScale(data.m_xmf3Scale);
    }

    // Add AnimationComponent and load animation
    if (!data.m_strAnimationPath.empty())
    {
        auto* pAnimComp = pEnemy->AddComponent<AnimationComponent>();
        pAnimComp->Init(m_pDevice, m_pCommandList);
        pAnimComp->LoadAnimation(data.m_strAnimationPath.c_str());
    }

    // Add RenderComponents to hierarchy
    AddRenderComponentsToHierarchy(pEnemy);

    // Load texture if specified
    if (!data.m_strTexturePath.empty())
    {
        LoadTextureToHierarchy(pEnemy, data.m_strTexturePath);
    }
    else
    {
        // Apply color tint to all meshes in hierarchy (only if no texture)
        ApplyColorToHierarchy(pEnemy, data.m_xmf4Color);
    }

    // Add ColliderComponent (reduced size to avoid getting stuck on terrain)
    auto* pCollider = pEnemy->AddComponent<ColliderComponent>();
    float colliderScale = data.m_xmf3Scale.x;
    if (data.m_xmf3Scale.y > colliderScale) colliderScale = data.m_xmf3Scale.y;
    if (data.m_xmf3Scale.z > colliderScale) colliderScale = data.m_xmf3Scale.z;
    float xzMult = (data.m_fColliderXZMultiplier > 0.0f) ? data.m_fColliderXZMultiplier : 0.3f;
    pCollider->SetExtents(colliderScale * xzMult, colliderScale * 1.0f, colliderScale * xzMult);
    pCollider->SetCenter(0.0f, colliderScale * 1.0f, 0.0f);
    pCollider->SetLayer(CollisionLayer::Enemy);
    pCollider->SetCollisionMask(CollisionMask::Enemy);

    wchar_t buffer[256];
    swprintf_s(buffer, L"[EnemySpawner] Created mesh enemy at (%.1f, %.1f, %.1f) from %hs\n",
        position.x, position.y, position.z, data.m_strMeshPath.c_str());
    OutputDebugString(buffer);

    return pEnemy;
}

void EnemySpawner::AddRenderComponentsToHierarchy(GameObject* pGameObject)
{
    if (!pGameObject || !m_pShader) return;

    // Skip Unity export helper nodes (BlobShadow, shadow projectors, etc.)
    {
        const char* name = pGameObject->m_pstrFrameName;
        bool bSkip = false;
        if (name)
        {
            // Case-insensitive substring check for known Unity helper node names
            std::string sName(name);
            for (char& c : sName) c = (char)tolower((unsigned char)c);
            if (sName == "bs" ||
                sName.find("shadow") != std::string::npos ||
                sName.find("blobshadow") != std::string::npos ||
                sName.find("projector") != std::string::npos)
            {
                bSkip = true;
                wchar_t buf[128];
                swprintf_s(buf, L"[EnemySpawner] Skipped helper node: %hs\n", name);
                OutputDebugString(buf);
            }
        }
        if (bSkip)
        {
            // Still traverse siblings (don't traverse children of skipped node)
            if (pGameObject->m_pSibling)
                AddRenderComponentsToHierarchy(pGameObject->m_pSibling);
            return;
        }
    }

    if (pGameObject->GetMesh())
    {
        auto* pRenderComp = pGameObject->AddComponent<RenderComponent>();
        pRenderComp->SetMesh(pGameObject->GetMesh());
        pRenderComp->SetCastsShadow(true);  // Enemies cast shadows
        m_pShader->AddRenderComponent(pRenderComp);

        wchar_t buffer[128];
        swprintf_s(buffer, L"[EnemySpawner] Added RenderComponent to: %hs\n", pGameObject->m_pstrFrameName);
        OutputDebugString(buffer);
    }

    if (pGameObject->m_pChild)
    {
        AddRenderComponentsToHierarchy(pGameObject->m_pChild);
    }
    if (pGameObject->m_pSibling)
    {
        AddRenderComponentsToHierarchy(pGameObject->m_pSibling);
    }
}

void EnemySpawner::ApplyColorToHierarchy(GameObject* pGameObject, const XMFLOAT4& color)
{
    if (!pGameObject) return;

    MATERIAL material;
    material.m_cAmbient = XMFLOAT4(color.x * 0.3f, color.y * 0.3f, color.z * 0.3f, 1.0f);
    material.m_cDiffuse = color;
    material.m_cSpecular = XMFLOAT4(0.5f, 0.5f, 0.5f, 32.0f);
    material.m_cEmissive = XMFLOAT4(color.x * 0.1f, color.y * 0.1f, color.z * 0.1f, 1.0f);
    pGameObject->SetMaterial(material);

    if (pGameObject->m_pChild)
    {
        ApplyColorToHierarchy(pGameObject->m_pChild, color);
    }
    if (pGameObject->m_pSibling)
    {
        ApplyColorToHierarchy(pGameObject->m_pSibling, color);
    }
}

void EnemySpawner::LoadTextureToHierarchy(GameObject* pGameObject, const std::string& texturePath)
{
    if (!pGameObject || !m_pDevice || !m_pCommandList || !m_pScene) return;

    // Load texture for objects with mesh
    if (pGameObject->GetMesh())
    {
        pGameObject->SetTextureName(texturePath.c_str());

        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
        D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle;
        m_pScene->AllocateDescriptor(&cpuHandle, &gpuHandle);

        pGameObject->LoadTexture(m_pDevice, m_pCommandList, cpuHandle);
        pGameObject->SetSrvGpuDescriptorHandle(gpuHandle);

        // Set white material to show texture properly
        MATERIAL material;
        material.m_cAmbient = XMFLOAT4(0.3f, 0.3f, 0.3f, 1.0f);
        material.m_cDiffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        material.m_cSpecular = XMFLOAT4(0.3f, 0.3f, 0.3f, 32.0f);
        material.m_cEmissive = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
        pGameObject->SetMaterial(material);

        wchar_t buffer[256];
        swprintf_s(buffer, L"[EnemySpawner] Loaded texture for: %hs\n", pGameObject->m_pstrFrameName);
        OutputDebugString(buffer);
    }

    if (pGameObject->m_pChild)
    {
        LoadTextureToHierarchy(pGameObject->m_pChild, texturePath);
    }
    if (pGameObject->m_pSibling)
    {
        LoadTextureToHierarchy(pGameObject->m_pSibling, texturePath);
    }
}

GameObject* EnemySpawner::CreateIndicatorObject(CRoom* pRoom, Mesh* pMesh)
{
    if (!m_pDevice || !m_pCommandList || !m_pScene || !pMesh || !m_pShader) return nullptr;

    CRoom* pPrevRoom = m_pScene->GetCurrentRoom();
    m_pScene->SetCurrentRoom(pRoom);

    GameObject* pIndicator = m_pScene->CreateGameObject(m_pDevice, m_pCommandList);

    m_pScene->SetCurrentRoom(pPrevRoom);

    if (!pIndicator) return nullptr;

    // Start hidden (below ground)
    TransformComponent* pTransform = pIndicator->GetTransform();
    if (pTransform)
    {
        pTransform->SetPosition(0.0f, -1000.0f, 0.0f);
    }

    // Set mesh
    pMesh->AddRef();
    pIndicator->SetMesh(pMesh);

    // 위협적인 붉은 발광 바닥 (로스트아크 텔레그래프 느낌)
    MATERIAL redMaterial;
    redMaterial.m_cAmbient  = XMFLOAT4(0.5f, 0.05f, 0.02f, 1.0f);
    redMaterial.m_cDiffuse  = XMFLOAT4(1.0f, 0.15f, 0.1f,  1.0f);
    redMaterial.m_cSpecular = XMFLOAT4(0.0f, 0.0f,  0.0f,  1.0f);
    redMaterial.m_cEmissive = XMFLOAT4(1.6f, 0.25f, 0.1f,  1.0f);  // 강한 붉은 발광
    pIndicator->SetMaterial(redMaterial);

    // Add render component — 오버레이 플래그로 depth 무시 + 맨 위에 렌더
    auto* pRenderComp = pIndicator->AddComponent<RenderComponent>();
    pRenderComp->SetMesh(pMesh);
    pRenderComp->SetOverlay(true);
    m_pShader->AddRenderComponent(pRenderComp);

    // FIX: Room이 Inactive일 때 Room::Update가 early return하여 인디케이터의
    // GameObject::Update가 호출되지 않아 CBV가 ZeroMemory 상태로 렌더링되는 버그 방지.
    // 생성 직후 Transform과 CB를 한 번 강제 동기화.
    pIndicator->GetTransform()->Update(0.0f);
    pIndicator->Update(0.0f);

    return pIndicator;
}

void EnemySpawner::SetupAttackIndicators(GameObject* pEnemy, EnemyComponent* pEnemyComp,
                                          const AttackIndicatorConfig& config, CRoom* pRoom)
{
    pEnemyComp->SetIndicatorConfig(config);

    if (config.m_eType == IndicatorType::Circle)
    {
        // 테두리 링 (고정 크기, 공격 범위 윤곽)
        GameObject* pBorder = CreateIndicatorObject(pRoom, m_pRingMesh);
        if (pBorder)
        {
            pEnemyComp->SetHitZoneIndicator(pBorder);
        }
        // 내부 fill 원판 (windup 동안 0→1 차오름)
        GameObject* pFill = CreateIndicatorObject(pRoom, m_pDiscMesh);
        if (pFill)
        {
            pEnemyComp->SetHitZoneFillIndicator(pFill);
        }
    }
    else if (config.m_eType == IndicatorType::ForwardBox)
    {
        // 전방 직사각형: 외곽 flat box (border 역할 — 살짝 큼) + 내부 fill (차오름)
        GameObject* pBorder = CreateIndicatorObject(pRoom, m_pBoxMesh);
        if (pBorder)
        {
            pEnemyComp->SetHitZoneIndicator(pBorder);
        }
        GameObject* pFill = CreateIndicatorObject(pRoom, m_pBoxMesh);
        if (pFill)
        {
            pEnemyComp->SetHitZoneFillIndicator(pFill);
        }
    }
    else if (config.m_eType == IndicatorType::RushCircle)
    {
        // Rush + 360 AoE: line + ring at destination
        GameObject* pLine = CreateIndicatorObject(pRoom, m_pLineMesh);
        if (pLine)
        {
            pEnemyComp->SetRushLineIndicator(pLine);
        }

        GameObject* pHitZone = CreateIndicatorObject(pRoom, m_pRingMesh);
        if (pHitZone)
        {
            pEnemyComp->SetHitZoneIndicator(pHitZone);
        }
    }
    else if (config.m_eType == IndicatorType::RushCone)
    {
        // Rush + cone: line + fan at destination
        GameObject* pLine = CreateIndicatorObject(pRoom, m_pLineMesh);
        if (pLine)
        {
            pEnemyComp->SetRushLineIndicator(pLine);
        }

        GameObject* pHitZone = CreateIndicatorObject(pRoom, m_pFanMesh);
        if (pHitZone)
        {
            pEnemyComp->SetHitZoneIndicator(pHitZone);
        }
    }
}
