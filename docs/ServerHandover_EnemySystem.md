# 적/보스 시스템 레퍼런스 — 서버 이관용

> DirectX 12 게임 클라이언트 (`gaym/gaym/`) 에서 현재 돌고 있는 적/보스 시스템 전수조사.
> 파일 경로와 라인 번호 기준으로 서버 작업자가 바로 코드 열람 가능.
> 모든 상태·AI·공격·투사체는 **현재 100% 클라 권위** — 이걸 서버로 옮기는 게 목표.

---

## 0. 핵심 요약 (TL;DR)

- **적 = `GameObject` + `EnemyComponent`**. FSM(Idle/Chase/Attack/Stagger/Dead)을 `EnemyComponent::Update()` 내부 틱에서 스스로 돌림.
- **스폰** = `EnemySpawner`가 preset 기반으로 GameObject 생성 + 컴포넌트 부착 + `CRoom::RegisterEnemy()` 호출.
- **공격** = `IAttackBehavior` 인터페이스 15+ 종 구현체. 보스는 거리 기반(근/중/원거리)으로 랜덤 선택.
- **보스 페이즈** = `BossPhaseController`가 HP% 임계값에서 공격 교체 + 속도 배율 + 전환 공격(MegaBreath 등) 실행.
- **투사체** = `ProjectileManager` (클라 로컬 풀). 자체 충돌검사로 `TakeDamage` 호출.
- **어그로** = `ThreatTable` — 데미지 누적 + 거리 가중, 0.5초마다 타겟 재선정.
- **방 클리어** = `CRoom::m_nDeadEnemies == m_nTotalEnemies` → 포탈 + 드랍 스폰.

---

## 1. 핵심 적 클래스: EnemyComponent

**파일**: `gaym/EnemyComponent.h` (285줄), `gaym/EnemyComponent.cpp` (1079줄)

### 1.1 상태 머신

`EnemyComponent.h:49-56` — `enum class EnemyState`:
- `Idle` — 타겟 없음, 대기
- `Chase` — 타겟 추격 또는 거리 유지
- `Attack` — 공격 behavior 실행 중
- `Stagger` — 피격 경직 (0.5초). **보스는 제외**
- `Dead` — 사망 (2.0초 린저 후 삭제)

**상태 전환**: `EnemyComponent.cpp:131-220` `ChangeState()` — 호출처:
- `UpdateIdle/Chase/Attack/Stagger()`의 각 조건 분기
- `TakeDamage()` (HP<=0일 때 Dead, 아니면 Stagger)
- 외부(Scene) — `L` 디버그 키로 강제 Attack 주입

**애니메이션 연동**: `ChangeState` 내부에서 `AnimationComponent::CrossFade()` 호출.

### 1.2 Stats 구조체

`EnemyComponent.h:68-81` `EnemyStats`:
```cpp
struct EnemyStats {
    float m_fMaxHP = 100.0f;
    float m_fCurrentHP = 100.0f;
    float m_fMoveSpeed = 5.0f;
    float m_fAttackRange = 2.0f;
    float m_fAttackCooldown = 1.0f;
    float m_fDetectionRange = 50.0f;

    // 보스 거리 기반 공격 선택 임계값
    float m_fLongRangeThreshold = 30.0f;   // ≥30: 플라잉/브레스
    float m_fMidRangeThreshold  = 15.0f;   // ≥15: 스페셜
    //     <15: 근접/콤보
};
```

### 1.3 데미지 / 사망

**`TakeDamage(float fDamage, bool bTriggerStagger = true)`** — `EnemyComponent.cpp:222-268`
- HP 차감 → Stagger(보스 제외) → HitFlash(0.15초) → DamageNumber 스폰(+2.0Y)
- `m_bInvincible == true`면 전부 블록
- `BossPhaseController::OnHealthChanged()` 호출 (페이즈 전환 트리거)
- HP ≤ 0 → `ChangeState(Dead)`

**`IsDead()`** — `EnemyComponent.h:113`: `m_eCurrentState == Dead`

**사망 콜백**: `SetOnDeathCallback(std::function<void(EnemyComponent*)>)` `EnemyComponent.h:159-160`
- `EnemyComponent::Die()` `EnemyComponent.cpp:875-889` 에서 호출
- 일반적으로 `CRoom::OnEnemyDeath()` 연결 (`Scene.cpp:2966-2976`)

### 1.4 HitFlash & DamageNumber

- HitFlash: `m_fHitFlashTimer`(0.15초) → `GameObject::SetHitFlash(float)` 매 프레임 갱신
- DamageNumber: `DamageNumberManager::Get().AddNumber(pos, damage)` — 부유 텍스트 1.0초

### 1.5 Threat(어그로) 시스템

**멤버**: `ThreatTable m_ThreatTable` `EnemyComponent.h:204`

**메서드** (`EnemyComponent.cpp:1011-1060`):
- `RegisterAllPlayers(const std::vector<GameObject*>&)` — 전투 시작 시 플레이어 일괄 등록
- `AddThreat(player, amount)` / `ReduceThreat(player, amount)`
- `ReevaluateTarget()` — 0.5초 주기로 최고 어그로 타겟 재선정

**사용**: `UpdateChase()` `EnemyComponent.cpp:437-657` 에서 거리·공격 선택에 활용

### 1.6 보스 전용 플래그

`EnemyComponent.h:218-226`:
- `bool m_bIsBoss` — 스태거 면역, 특수 매커니즘
- `bool m_bInvincible` — 특정 공격 중 전체 데미지 무시
- `bool m_bUsingSpecialAttack`, `m_bUsingFlyingAttack` — 실행 중 플래그

### 1.7 보스 페이즈 연동

- **멤버**: `std::unique_ptr<BossPhaseController> m_pPhaseController` `EnemyComponent.h:225`
- 페이즈 전환 트리거: `TakeDamage` 내부에서 `OnHealthChanged(currentHP, maxHP)` 호출 `EnemyComponent.cpp:252-256`
- `CanUseFlyingAttack()` `EnemyComponent.cpp:1071-1078` — 현재 페이즈의 `CanFly()` 체크

---

## 2. 스폰 시스템

**파일**:
- `gaym/EnemySpawner.h` / `.cpp` (600+줄)
- `gaym/EnemySpawnData.h` (85줄) — preset 구조체 정의

### 2.1 EnemySpawnData 구조체

`EnemySpawnData.h:11-62`:
```cpp
struct EnemySpawnData {
    // 시각
    std::string m_strMeshPath;          // 비어있으면 CubeMesh 사용
    std::string m_strAnimationPath;
    std::string m_strTexturePath;
    XMFLOAT3 m_xmf3Scale = {1,1,1};
    XMFLOAT4 m_xmf4Color = {1,0,0,1};

    EnemyStats m_Stats;

    // 공격 팩토리 (매 사용마다 새 인스턴스)
    std::function<std::unique_ptr<IAttackBehavior>()> m_fnCreateAttack;
    std::function<std::unique_ptr<IAttackBehavior>()> m_fnCreateSpecialAttack;

    EnemyAnimationConfig m_AnimConfig;       // Idle/Walk/Attack/Death 클립명
    AttackIndicatorConfig m_IndicatorConfig; // 선딜 표시(원/콘)

    // 비행
    bool m_bIsFlying = false;
    float m_fFlyHeight = 15.0f;

    float m_fColliderXZMultiplier = 0.0f;    // 0=자동

    // 보스
    bool m_bIsBoss = false;
    float m_fSpecialAttackCooldown = 10.0f;
    float m_fFlyingAttackCooldown  = 12.0f;
    int   m_nSpecialAttackChance   = 30;     // 0~100
    int   m_nFlyingAttackChance    = 30;

    std::function<std::unique_ptr<BossPhaseConfig>()> m_fnCreateBossPhaseConfig;
};
```

### 2.2 Preset 등록 — `EnemySpawner::Init()`

`EnemySpawner.cpp:41-571` — 게임 시작 시 모든 preset 등록.

**등록된 preset 이름**:
- `"TestEnemy"` / `"AirElemental"` / `"RushAoEEnemy"` / `"RushFrontEnemy"` / `"RangedEnemy"` — 잡몹
- `"Dragon"` — Red Dragon (Fire 보스)
- `"BlueDragon"` — Water 인트로 보스
- `"Kraken"` — Water 보스
- `"Golem"` — Earth 보스
- `"Demon"` — Grass 보스

### 2.3 스폰 실행

`EnemySpawner::SpawnEnemy(CRoom*, const std::string& preset, const XMFLOAT3& pos, GameObject* pTarget)` `EnemySpawner.cpp:587+`:
1. `m_mapPresets[preset]` 조회
2. `CreateMeshEnemy` / `CreateCubeEnemy` 로 GameObject 생성
3. `SetupEnemyComponents()` 호출 — `EnemyComponent` / `AnimationComponent` / `ColliderComponent` / `RenderComponent` 부착
4. `SetupAttackIndicators()` — 필요 시 선딜 인디케이터 mesh 부착
5. **`pRoom->RegisterEnemy(pEnemyComp)`** — 방에 등록
6. Attack behavior 팩토리 주입
7. 보스면 `BossPhaseController` 생성·부착

**핵심**: preset 이름만 알면 서버가 "Spawn X at Y" 명령을 내릴 수 있다. 실제 mesh/animation 에셋은 클라 소유로 유지.

---

## 3. 공격 Behavior

### 3.1 IAttackBehavior 인터페이스

`IAttackBehavior.h:5-24`:
```cpp
class IAttackBehavior {
public:
    virtual void Execute(EnemyComponent* pEnemy) = 0;   // 공격 시작
    virtual void Update(float dt, EnemyComponent* pEnemy) = 0;
    virtual bool IsFinished() const = 0;
    virtual void Reset() = 0;
    virtual const char* GetAnimClipName() const { return ""; }
};
```

### 3.2 구현체 목록

| 클래스 | 파일 | 동작 | 투사체 |
|---|---|---|:-:|
| MeleeAttackBehavior | MeleeAttackBehavior.h | 근접 콘 데미지 (0.3 windup, 0.5 hit, 0.2 recovery) | ✗ |
| RangedAttackBehavior | RangedAttackBehavior.h | 단일 투사체 | ✓ |
| BreathAttackBehavior | BreathAttackBehavior.h | 상공에서 5~24발 원뿔 확산 | ✓ |
| FireballBehavior | FireballBehavior.h | (플레이어 스킬 전용) | ✓ |
| MegaBreathAttackBehavior | MegaBreathAttackBehavior.h | 페이즈 전환 — 벽으로 날아가 화면 커버 지속 데미지 | ✓(에어리어) |
| RushAoEAttackBehavior | RushAoEAttackBehavior.h | 돌진 후 360° AoE 폭발 (1.2s 돌진, 5.0 반경) | ✗ |
| RushFrontAttackBehavior | RushFrontAttackBehavior.h | 돌진하며 전방 콘 데미지 (18u/s, 90°) | ✗ |
| DiveBombAttackBehavior | DiveBombAttackBehavior.h | 상승 후 급강하 + 전방 투사체 확산 | ✓ |
| ComboAttackBehavior | ComboAttackBehavior.h | 2~5회 연속 근접, 각 타마다 타겟 재조준 | ✗ |
| TailSweepAttackBehavior | TailSweepAttackBehavior.h | 180~360° 꼬리 휘두르기 (0.4 w / 0.3 sweep / 0.4 r) | ✗ |
| JumpSlamAttackBehavior | JumpSlamAttackBehavior.h | 타겟 위치로 점프 후 착지 AoE (10u 높이, 7u 반경) | ✗ |
| FlyingBarrageAttackBehavior | FlyingBarrageAttackBehavior.h | 상공 무적, 12+파 × 24발 원형 탄막 | ✓(288+발) |
| FlyingCircleAttackBehavior | FlyingCircleAttackBehavior.h | 플레이어 궤도 회전(540°) + 3발 샷 | ✓ |
| FlyingStrafeAttackBehavior | FlyingStrafeAttackBehavior.h | 수직 스트레이프 후 버스트 | ✓ |
| FlyingSweepAttackBehavior | FlyingSweepAttackBehavior.h | 직선 비행 중 탄막 사이드 스윕(120° 아크) | ✓ |
| WaveSlashBehavior | WaveSlashBehavior.h | (플레이어 Q 스킬) | — |

### 3.3 보스 공격 선택 로직

`EnemyComponent.cpp:437-657` `UpdateChase()`:
- **보스**: 거리 기반
  - ≥30u (LongRange): 70% 플라잉 공격 → primary
  - 15~30u (MidRange): special 50% → flying 30~50% → primary
  - <15u (Close): special 30% → flying 15% → primary
- **일반**: 범위 내 = attack, 아니면 chase
- **쿨다운**: `m_fAttackCooldownTimer / m_fSpecialCooldownTimer / m_fFlyingCooldownTimer`

---

## 4. 보스 페이즈 시스템

**파일**:
- `gaym/BossPhaseController.h` / `.cpp` (150+줄)
- `gaym/BossPhaseConfig.h` / `.cpp` (67줄)

### 4.1 BossPhaseData 구조체

`BossPhaseConfig.h:11-43`:
```cpp
struct BossPhaseData {
    float m_fHealthThreshold = 1.0f;   // 0~1 (0.7 = 70% HP)

    std::function<std::unique_ptr<IAttackBehavior>()> m_fnPrimaryAttack;
    std::function<std::unique_ptr<IAttackBehavior>()> m_fnSpecialAttack;
    std::function<std::unique_ptr<IAttackBehavior>()> m_fnFlyingAttack;
    std::function<std::unique_ptr<IAttackBehavior>()> m_fnTransitionAttack;

    bool m_bCanFly = false;
    bool m_bHasTransitionAttack = false;

    float m_fSpeedMultiplier = 1.0f;
    float m_fAttackSpeedMultiplier = 1.0f;
    int   m_nSpecialAttackChance = 30;
    int   m_nFlyingAttackChance  = 0;

    bool  m_bInvincibleDuringTransition = false;
    float m_fTransitionDuration = 2.0f;
    std::string m_strTransitionAnimation = "Scream";

    bool m_bSpawnAdds = false;
    std::vector<std::pair<std::string, XMFLOAT3>> m_vAddSpawns;
};
```

### 4.2 전환 흐름

`BossPhaseController.h:11-17` — 전환 상태:
```cpp
enum class PhaseTransitionState {
    None, TransitionAttack, TransitionAnim, TransitionDone
};
```

`BossPhaseController::OnHealthChanged(currentHP, maxHP)` `BossPhaseController.cpp:97-119`:
1. HP% 계산
2. 임계값 교차 시 `TransitionToPhase(newPhase)`
3. Transition attack 실행 → Transition anim 재생 → 새 페이즈 공격 세트 활성

### 4.3 Red Dragon 현재 설정 (페이즈 3단계) `EnemySpawner.cpp:222-379`

- **P1 (100~70%)**: 지상, Breath(30)/TailSweep(35)/JumpSlam(45)/LightCombo(3hit), 플라잉 X
- **P2 (70~35%)**: 공중전, 1.3× 속도, FastBreath(35)/JumpSlam(50)/HeavyCombo(2hit), Strafe/Circle(22), **MegaBreath 전환(15dmg/tick × 4s)**, `FlyingChance=55`
- **P3 (35~0%)**: 광폭화, 1.6× 속도, RapidBreath(42, 6발)/FuryCombo(5hit)/JumpSlam(60), Dive/Sweep/Barrage/Strafe(26~30), **강화 MegaBreath(25dmg/tick × 5s, 무적)**, `FlyingChance=65`

### 4.4 기타 보스 (단일 페이즈)

- **Kraken** `EnemySpawner.cpp:383-420`: 1000 HP, Melee + RushAoE
- **Golem** `EnemySpawner.cpp:422-460`: 2000 HP, 8× 스케일, 이동속도 1 (사실상 고정), JumpSlam + 360° TailSweep
- **Demon** `EnemySpawner.cpp:462-498`: 1500 HP, Melee + RushFront 콘
- **BlueDragon** `EnemySpawner.cpp:500-559`: 80 HP, Water 인트로용, Breath(water)/TailSweep/JumpSlam/Combo/RushFront

---

## 5. 투사체 시스템

**파일**: `gaym/ProjectileManager.h` / `.cpp`, `gaym/Projectile.h`

### 5.1 ProjectileManager

`ProjectileManager.h:35-112`:
- `Init(Scene*, ID3D12Device*, ID3D12GraphicsCommandList*, CDescriptorHeap*, UINT startDescIdx)`
- `SpawnProjectile(...)` — 파라미터 편의 오버로드 제공
- `Update(dt)` — 이동, 충돌, 데미지 일괄 처리
- `Render(cmdList)`

### 5.2 Projectile 구조체

`Projectile.h:9-62`:
```cpp
struct Projectile {
    XMFLOAT3 position;
    XMFLOAT3 direction;
    float speed = 30.0f;

    float damage = 10.0f;
    float radius = 0.5f;
    float explosionRadius = 0.0f;  // 0=단일, >0=AoE
    ElementType element = ElementType::None;

    GameObject* owner = nullptr;   // 발사자
    bool isPlayerProjectile = true;

    float maxDistance = 100.0f;
    float distanceTraveled = 0.0f;
    bool isActive = true;

    float scale = 1.0f;
    float chargeRatio = 0.0f;
    int fluidVFXId = -1;
    RuneCombo runeCombo;
    bool wasHit = false;
};
```

### 5.3 충돌/데미지

`ProjectileManager.cpp`:
- `CheckProjectileCollisions()` — 구체 vs 적/플레이어
- 단일: `ApplyDamage(proj, pEnemy)` → `pEnemy->TakeDamage(dmg, false)` (스태거 X)
- AoE: `ApplyAoEDamage(proj, impactPoint)` — `explosionRadius` 내 전원
- 맞거나 maxDistance 도달 시 `isActive=false`

---

## 6. Threat(어그로) 시스템

**파일**: `gaym/ThreatSystem.h` / `.cpp`, `gaym/ThreatConstants.h`

### 6.1 ThreatTable API

`ThreatSystem.h:15-46`:
```cpp
class ThreatTable {
public:
    void AddThreat(GameObject* player, float amount);
    void ReduceThreat(GameObject* player, float amount);
    void SetThreat(GameObject* player, float amount);
    float GetThreat(GameObject* player) const;

    GameObject* GetHighestThreatTarget(GameObject* current = nullptr) const;
    void Update(float dt, const XMFLOAT3& enemyPos);

    void RegisterPlayer(GameObject* player, float initial = 10.0f);
    void RemovePlayer(GameObject* player);
    void CleanupDeadPlayers();
    void Clear();
};
```

### 6.2 상수 `ThreatConstants.h`

- `DAMAGE_THREAT_MULTIPLIER = 1.0` (1 dmg = 1 threat)
- `SKILL_BASE_THREAT = 5.0`
- `HEAL_THREAT_MULTIPLIER = 0.5`
- `THREAT_DECAY_DISTANCE = 30`, `DECAY_RATE = 5/s`
- `THREAT_GAIN_DISTANCE = 10`, `GAIN_RATE = 2/s`
- `TARGET_REEVALUATION_INTERVAL = 0.5s`
- `CURRENT_TARGET_BONUS = 1.1×` (타겟 변경 억제)
- `INITIAL_THREAT = 10`

---

## 7. Room 레벨 적 관리

**파일**: `gaym/Room.h` / `.cpp`

### 7.1 필드

`Room.h:19-107`:
- `std::vector<EnemyComponent*> m_vEnemies`
- `int m_nTotalEnemies`, `int m_nDeadEnemies`

### 7.2 메서드

- `RegisterEnemy(EnemyComponent*)` `Room.cpp:157-167` — 스폰 시점 호출
- `OnEnemyDeath(EnemyComponent*)` `Room.cpp:169-178` — 사망 콜백에서 호출, `m_nDeadEnemies++`
- `CheckClearCondition()` `Room.cpp:142-155` — `Active + m_nDeadEnemies >= m_nTotalEnemies`이면 `Cleared`로 전환 후 `SpawnDropItem() + SpawnPortalCube()`
- `SpawnEnemies()` `Room.cpp:180+` — `Active` 진입 시 1회, `m_SpawnConfig.m_vEnemySpawns` 순회

### 7.3 방 상태

- `Inactive` — 업데이트·렌더 X
- `Active` — 적 스폰, 클리어 판정, 오브젝트 업데이트
- `Cleared` — 전원 사망, 드랍+포탈 존재

---

## 8. Scene 레벨 관리

### 8.1 사망 콜백 연결

`Scene.cpp:2966-2976` — Dragon 예시:
```cpp
pDragonEnemy->SetOnDeathCallback([this, pRoom](EnemyComponent* pDead) {
    if (pRoom) pRoom->OnEnemyDeath(pDead);
    // 씬별 추가 로직 (ex. 페이즈2 스폰)
});
```

### 8.2 삭제 처리

`EnemyComponent.cpp:725-786` `UpdateDead()`:
- 일반: 2.0초 린저 후 `Scene::MarkForDeletion(m_pOwner)`
- 보스: 8초+ 사체 유지
- Scene의 삭제 스위프가 정리

### 8.3 스킬 타격 경로

1. 스킬 시전 → `ProjectileManager::SpawnProjectile()`
2. `Update()` 내부 충돌검사 → `ApplyDamage()` → `EnemyComponent::TakeDamage()`
3. AoE이면 반경 내 전원

---

## 9. VFX / 애니메이션 사이드 이펙트

서버가 권위가 되어도 **브로드캐스트 필요한 이벤트**:

| 이벤트 | 시각 효과 | 비고 |
|---|---|---|
| 피격 | HitFlash 0.15s 붉게 | `SetHitFlash(1.0)` → 페이드 |
| 데미지 누적 | 부유 숫자(+2.0Y, 1.0s 상승) | `DamageNumberManager` |
| 상태 전환 | 애니메이션 크로스페이드 | `AnimationComponent::CrossFade` |
| 사망 | Death 클립 + 린저 | 서버가 broadcast |
| 스페셜 공격 | Element별 색상 투사체 | Fire/Water/Earth/Grass |
| MegaBreath | GPU 유체 이펙트 | `FluidSkillVFXManager` 클라 소유 |

---

## 10. 보스 / 적 데이터 요약표

### 보스

| 이름 | preset | HP | 스케일 | 속도 | 주요 공격 | 페이즈 |
|---|---|---:|---:|---:|---|:-:|
| **Red Dragon** | `"Dragon"` | 800 | 3.0× | 10 | Breath/TailSweep/JumpSlam/Combo(3종)/Flying(4종)/MegaBreath | 3 |
| **Blue Dragon** | `"BlueDragon"` | 80 | 3.0× | 9 | Breath(water)/TailSweep/JumpSlam/Combo/RushFront | 1(인트로) |
| **Kraken** | `"Kraken"` | 1000 | 3.0× | 6 | Melee + RushAoE | 1 |
| **Golem** | `"Golem"` | 2000 | 8.0× | 1 | JumpSlam(점프 X) + 360° TailSweep | 1 |
| **Demon** | `"Demon"` | 1500 | 3.5× | 9 | Melee + RushFront 콘 | 1 |

### 일반 적

| 이름 | preset | HP | 공격 | 사거리 | 속도 |
|---|---|---:|---|---:|---:|
| Test | `"TestEnemy"` | 50 | Melee(10) | 3 | 4 |
| AirElemental | `"AirElemental"` | 80 | Melee(15) | 4 | 5 |
| RushAoE | `"RushAoEEnemy"` | 100 | Rush+360°(15) | 20 | 5 |
| RushFront | `"RushFrontEnemy"` | 80 | Rush+콘(20) | 18 | 5 |
| Ranged | `"RangedEnemy"` | 60 | 투사체(10) | 30 | 3 |

---

## 11. 기타 관련 파일

- `CollisionManager.cpp/.h` — 플레이어-적 거리 검사
- `AnimationComponent.cpp/.h` — 클립 재생·블렌드
- `DamageNumberManager.cpp/.h` — 부유 데미지 숫자 풀
- `DropItemComponent.cpp/.h` — 방 클리어 시 랜덤 룬 3개
- `FluidSkillVFXManager.cpp/.h` — GPU 유체(MegaBreath)
- `VFXLibrary.cpp/.h` — 이펙트 에셋 라이브러리

---

## 12. 상태 흐름도

```
[Spawn via EnemySpawner::SpawnEnemy]
        ↓
     Idle (타겟 없음)
        ↓ [Threat 시스템이 타겟 선정]
     Chase (거리 기반 분기)
     ├─ 범위 안: Attack 진입
     └─ 범위 밖: 이동 + 타겟 페이싱
        ↓
     Attack (behavior 실행)
     ├─ behavior.Update()
     ├─ IsFinished() → 쿨다운 리셋
     └─ Chase 복귀

     [TakeDamage() 호출 시]
     ├─ HP > 0:
     │  └─ Stagger(0.5s, 보스 제외) → Chase 복귀
     └─ HP ≤ 0:
        └─ Dead (애니메이션 → 린저 → 삭제)

     [보스 HP% 임계값 교차 시]
     └─ BossPhaseController::OnHealthChanged
        └─ TransitionAttack(MegaBreath 등) → TransitionAnim → 새 페이즈 stats 적용
```

---

## 13. 서버 이관 시 권장 우선순위

### Phase 1 — 스폰/위치/사망 동기화 (최소 기능)
- 서버가 enemyId 발급
- 새 패킷: `S_ENEMY_SPAWN(id, presetName, pos)`, `S_ENEMY_MOVE(id, pos, rot, state)`, `S_ENEMY_DEATH(id)`
- 클라는 받은 상태로 GameObject 동기화. **AI·공격은 여전히 로컬 실행** (중간단계)
- 결과: 모든 클라가 같은 적을 같은 자리에서 봄, 누가 죽이든 모두 사라짐

### Phase 2 — 데미지/HP 서버 권위
- `C_ENEMY_HIT(enemyId, damage, skillType)` — 클라가 타격 보고
- 서버가 HP 감산 → `S_ENEMY_HP(id, current)` 브로드캐스트
- 서버가 0 감지 시 `S_ENEMY_DEATH` → **보스 클리어 판정도 서버에서 자동**
- 결과: 데미지 크레딧 공정, `C_BOSS_CLEAR` 패킷 불필요

### Phase 3 — AI 서버 이관 (한 번에 X, 행동별 분할)
- 3a. Idle/Chase FSM + Threat 테이블 서버화 → target 서버가 지정
- 3b. Melee attack 이관
- 3c. Ranged + Fireball + 투사체 서버 시뮬레이션
- 3d. 보스 공격 한 종씩(WaveSlash → FireBeam → Meteor → ...) 이관
- 3e. BossPhaseController 이관

### Phase 4 — 레이턴시 대응
- 서버 틱 고정(예: 30Hz), 클라 보간
- 플레이어 공격 즉시 이펙트 + 서버 확인 후 데미지 반영(예측·보정)

---

## 14. 서버 쪽 결정 필요 사항

1. **서버 틱 레이트** — 현재 서버는 이벤트 드리븐(패킷 도착 시에만 처리). AI 이관하려면 **고정 틱(예: 30Hz) 루프 필요**
2. **Enemy ID** — 64bit uint, 방 단위 유니크
3. **맵 콜리전** — AI가 벽 피해 이동하려면 서버도 터레인 알아야 함. 초기엔 "벽 없음 가정" 단순화 가능
4. **보스 컷신 타이밍** (Dragon 인트로, Kraken 등장 등) — 서버가 조율? 클라 로컬?
5. **예측/보정 정책** — 플레이어 공격 latency 보정 범위
6. **에셋 메타데이터 경계** — 서버는 preset 이름만, 클라가 mesh/anim/texture 로드 유지

---

**작성일**: 2026-04-18
**작성 목적**: 적/보스 시스템 서버 이관 Phase 1 착수 전 현황 공유
