# 적 몬스터 시스템 (Enemy System)

## 개요

플레이어가 룸에 진입하면 적들이 스폰되어 전투를 벌이는 시스템입니다.
FSM(유한 상태 기계) 기반의 AI와 전략 패턴을 활용한 공격 시스템으로 구성되어 있습니다.

### 주요 특징
- **FSM 기반 AI**: Idle → Chase → Attack → Stagger → Dead 상태 전이
- **전략 패턴 공격**: `IAttackBehavior` 인터페이스로 다양한 공격 패턴 확장 가능
- **프리셋 시스템**: `EnemySpawner`를 통한 적 타입 사전 등록 및 재사용
- **룸 연동**: 룸 활성화 시 자동 스폰, 모든 적 처치 시 클리어 판정

---

## 파일 구조

```
gaym/
├── EnemyComponent.h        # 적 컴포넌트 (FSM + Stats)
├── EnemyComponent.cpp
├── IAttackBehavior.h       # 공격 행동 인터페이스
├── MeleeAttackBehavior.h   # 근접 공격 구현체
├── MeleeAttackBehavior.cpp
├── EnemySpawnData.h        # 스폰 데이터 구조체
├── EnemySpawner.h          # 적 생성 관리자
├── EnemySpawner.cpp
├── Room.h                  # (수정됨) 적 관리 기능 추가
└── Room.cpp
```

---

## 클래스 상세 설명

### 1. EnemyComponent

적 게임오브젝트에 부착되는 핵심 컴포넌트입니다.

#### 상태 (EnemyState)

```cpp
enum class EnemyState
{
    Idle,      // 대기 상태 - 타겟이 설정되면 즉시 Chase로 전환
    Chase,     // 추적 상태 - 타겟을 향해 이동
    Attack,    // 공격 상태 - 공격 범위 내에서 공격 실행
    Stagger,   // 경직 상태 - 피격 시 0.5초간 행동 불가
    Dead       // 사망 상태 - 더 이상 상태 전환 불가
};
```

#### 스탯 (EnemyStats)

```cpp
struct EnemyStats
{
    float m_fMaxHP = 100.0f;           // 최대 체력
    float m_fCurrentHP = 100.0f;       // 현재 체력
    float m_fMoveSpeed = 5.0f;         // 이동 속도 (units/sec)
    float m_fAttackRange = 2.0f;       // 공격 사거리
    float m_fAttackCooldown = 1.0f;    // 공격 쿨다운 (초)
    float m_fDetectionRange = 50.0f;   // 감지 범위 (현재 미사용)
};
```

#### 주요 메서드

| 메서드 | 설명 |
|--------|------|
| `SetTarget(GameObject*)` | 추적할 타겟(플레이어) 설정 |
| `SetStats(EnemyStats&)` | 적 스탯 설정 |
| `TakeDamage(float)` | 데미지 처리 (HP 감소, Stagger/Dead 전환) |
| `SetAttackBehavior(unique_ptr<IAttackBehavior>)` | 공격 패턴 설정 |
| `SetOnDeathCallback(function<void(EnemyComponent*)>)` | 사망 시 콜백 등록 |
| `GetDistanceToTarget()` | 타겟과의 거리 계산 |
| `FaceTarget()` | 타겟을 바라보도록 회전 |
| `MoveTowardsTarget(float dt)` | 타겟 방향으로 이동 |

#### 사용 예시

```cpp
// 기본 사용법 (EnemySpawner가 자동으로 처리)
auto* pEnemy = pGameObject->AddComponent<EnemyComponent>();
pEnemy->SetTarget(pPlayerGameObject);

EnemyStats stats;
stats.m_fMaxHP = 50.0f;
stats.m_fMoveSpeed = 4.0f;
stats.m_fAttackRange = 3.0f;
pEnemy->SetStats(stats);

pEnemy->SetAttackBehavior(std::make_unique<MeleeAttackBehavior>(10.0f));
```

---

### 2. IAttackBehavior (인터페이스)

공격 행동을 정의하는 전략 패턴 인터페이스입니다.

```cpp
class IAttackBehavior
{
public:
    virtual ~IAttackBehavior() = default;

    // 공격 시작
    virtual void Execute(EnemyComponent* pEnemy) = 0;

    // 공격 업데이트 (매 프레임 호출)
    virtual void Update(float dt, EnemyComponent* pEnemy) = 0;

    // 공격 완료 여부
    virtual bool IsFinished() const = 0;

    // 상태 초기화
    virtual void Reset() = 0;
};
```

#### 새로운 공격 패턴 추가 방법

```cpp
// 예시: 원거리 공격 패턴
class RangedAttackBehavior : public IAttackBehavior
{
public:
    void Execute(EnemyComponent* pEnemy) override
    {
        Reset();
        // 투사체 생성 준비
    }

    void Update(float dt, EnemyComponent* pEnemy) override
    {
        m_fTimer += dt;
        if (m_fTimer >= m_fFireTime && !m_bFired)
        {
            // 투사체 발사
            m_bFired = true;
        }
        if (m_fTimer >= m_fTotalDuration)
        {
            m_bFinished = true;
        }
    }

    bool IsFinished() const override { return m_bFinished; }
    void Reset() override { m_fTimer = 0; m_bFired = false; m_bFinished = false; }

private:
    float m_fFireTime = 0.3f;
    float m_fTotalDuration = 0.8f;
    float m_fTimer = 0.0f;
    bool m_bFired = false;
    bool m_bFinished = false;
};
```

---

### 3. MeleeAttackBehavior

근접 공격 구현체입니다. 윈드업 → 타격 → 회복 3단계로 진행됩니다.

#### 생성자 파라미터

```cpp
MeleeAttackBehavior(
    float fDamage = 10.0f,        // 데미지
    float fWindupTime = 0.3f,     // 선딜레이 (준비 시간)
    float fHitTime = 0.5f,        // 타격 판정 시간
    float fRecoveryTime = 0.2f    // 후딜레이 (회복 시간)
);
```

#### 타임라인

```
|-- Windup --|-- Hit --|-- Recovery --|
0.0s        0.3s      0.8s          1.0s
             ↑
          데미지 판정
```

#### 설정 메서드

```cpp
void SetDamage(float fDamage);
void SetWindupTime(float fTime);
void SetHitTime(float fTime);
void SetRecoveryTime(float fTime);
void SetHitRange(float fRange);  // 타격 시 거리 체크용
```

---

### 4. EnemySpawnData

적 스폰에 필요한 데이터를 담는 구조체입니다.

```cpp
struct EnemySpawnData
{
    // 비주얼
    std::string m_strMeshPath;           // 메쉬 경로 (빈 문자열 = CubeMesh 사용)
    std::string m_strAnimationPath;      // 애니메이션 경로 (빈 문자열 = 애니메이션 없음)
    XMFLOAT3 m_xmf3Scale = {1,1,1};      // 크기
    XMFLOAT4 m_xmf4Color = {1,0,0,1};    // 색상 (RGBA)

    // 스탯
    EnemyStats m_Stats;

    // 공격 패턴 팩토리
    std::function<std::unique_ptr<IAttackBehavior>()> m_fnCreateAttack;
};
```

#### RoomSpawnConfig

룸에 스폰할 적 목록을 정의합니다.

```cpp
struct RoomSpawnConfig
{
    // (프리셋 이름, 위치) 쌍의 목록
    std::vector<std::pair<std::string, XMFLOAT3>> m_vEnemySpawns;

    void AddSpawn(const std::string& presetName, const XMFLOAT3& position);
    void AddSpawn(const std::string& presetName, float x, float y, float z);
    void Clear();
};
```

---

### 5. EnemySpawner

적 생성을 관리하는 클래스입니다.

#### 초기화

```cpp
void Init(
    ID3D12Device* pDevice,
    ID3D12GraphicsCommandList* pCommandList,
    Scene* pScene,
    Shader* pShader
);
```

#### 프리셋 등록

```cpp
// 새로운 적 타입 등록
EnemySpawnData goblinData;
goblinData.m_xmf3Scale = XMFLOAT3(0.8f, 1.5f, 0.8f);
goblinData.m_xmf4Color = XMFLOAT4(0.2f, 0.8f, 0.2f, 1.0f);  // 녹색
goblinData.m_Stats.m_fMaxHP = 30.0f;
goblinData.m_Stats.m_fMoveSpeed = 6.0f;  // 빠름
goblinData.m_Stats.m_fAttackRange = 2.0f;
goblinData.m_fnCreateAttack = []() {
    return std::make_unique<MeleeAttackBehavior>(5.0f, 0.2f, 0.1f, 0.2f);
};

m_pEnemySpawner->RegisterEnemyPreset("Goblin", goblinData);
```

#### 적 스폰

```cpp
// 단일 적 스폰
GameObject* pEnemy = m_pEnemySpawner->SpawnEnemy(
    pRoom,                          // 소속 룸
    "Goblin",                       // 프리셋 이름
    XMFLOAT3(10.0f, 0.0f, 5.0f),   // 위치
    pPlayerGameObject               // 타겟
);

// 테스트용 기본 적 스폰
GameObject* pTestEnemy = m_pEnemySpawner->SpawnTestEnemy(
    pRoom,
    XMFLOAT3(0.0f, 0.0f, 10.0f),
    pPlayerGameObject
);

// 룸 설정에 따라 일괄 스폰
m_pEnemySpawner->SpawnRoomEnemies(pRoom, spawnConfig, pPlayerGameObject);
```

#### 기본 프리셋 (TestEnemy)

초기화 시 자동 등록되는 테스트용 프리셋:

| 속성 | 값 |
|------|-----|
| 크기 | 1.0 x 2.0 x 1.0 |
| 색상 | 빨간색 (1, 0.2, 0.2) |
| HP | 50 |
| 이동속도 | 4.0 |
| 공격 사거리 | 3.0 |
| 공격 쿨다운 | 1.5초 |
| 공격 데미지 | 10 |

---

### 6. CRoom (확장된 기능)

#### 새로 추가된 멤버

```cpp
// 적 관리
std::vector<EnemyComponent*> m_vEnemies;  // 등록된 적 목록
int m_nTotalEnemies = 0;                   // 총 적 수
int m_nDeadEnemies = 0;                    // 사망한 적 수
RoomSpawnConfig m_SpawnConfig;             // 스폰 설정
EnemySpawner* m_pSpawner = nullptr;        // 스포너 참조
GameObject* m_pPlayerTarget = nullptr;     // 플레이어 참조
bool m_bEnemiesSpawned = false;            // 스폰 완료 여부
```

#### 새로 추가된 메서드

| 메서드 | 설명 |
|--------|------|
| `SetSpawnConfig(RoomSpawnConfig&)` | 스폰 설정 적용 |
| `SetEnemySpawner(EnemySpawner*)` | 스포너 설정 |
| `SetPlayerTarget(GameObject*)` | 플레이어 타겟 설정 |
| `RegisterEnemy(EnemyComponent*)` | 적 등록 (스포너가 자동 호출) |
| `OnEnemyDeath(EnemyComponent*)` | 적 사망 콜백 |
| `GetAliveEnemyCount()` | 생존 적 수 반환 |
| `SpawnEnemies()` | 적 스폰 실행 |

#### 상태 흐름

```
Inactive ──(플레이어 진입)──▶ Active ──(모든 적 처치)──▶ Cleared
                                │
                                ▼
                         SpawnEnemies() 호출
                                │
                                ▼
                         적 추적 시작
```

---

## 통합 사용 예시

### Scene에서 적 시스템 설정

```cpp
void Scene::Init(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList)
{
    // ... 기존 초기화 코드 ...

    // 1. 룸 생성
    auto pRoom = std::make_unique<CRoom>();
    pRoom->SetBoundingBox(BoundingBox(XMFLOAT3(0, 0, 0), XMFLOAT3(50, 50, 50)));
    m_vRooms.push_back(std::move(pRoom));
    m_pCurrentRoom = m_vRooms[0].get();

    // 2. EnemySpawner 초기화
    m_pEnemySpawner = std::make_unique<EnemySpawner>();
    m_pEnemySpawner->Init(pDevice, pCommandList, this, pShader.get());

    // 3. 커스텀 적 타입 등록 (선택사항)
    EnemySpawnData bossData;
    bossData.m_xmf3Scale = XMFLOAT3(2.0f, 3.0f, 2.0f);
    bossData.m_xmf4Color = XMFLOAT4(0.5f, 0.0f, 0.5f, 1.0f);  // 보라색
    bossData.m_Stats.m_fMaxHP = 500.0f;
    bossData.m_Stats.m_fMoveSpeed = 2.0f;
    bossData.m_Stats.m_fAttackRange = 5.0f;
    bossData.m_fnCreateAttack = []() {
        return std::make_unique<MeleeAttackBehavior>(50.0f, 0.8f, 0.3f, 0.5f);
    };
    m_pEnemySpawner->RegisterEnemyPreset("Boss", bossData);

    // 4. 룸 스폰 설정
    RoomSpawnConfig spawnConfig;
    spawnConfig.AddSpawn("TestEnemy", 10.0f, 0.0f, 0.0f);
    spawnConfig.AddSpawn("TestEnemy", -10.0f, 0.0f, 0.0f);
    spawnConfig.AddSpawn("Boss", 0.0f, 0.0f, 20.0f);

    // 5. 룸에 설정 적용
    m_pCurrentRoom->SetSpawnConfig(spawnConfig);
    m_pCurrentRoom->SetEnemySpawner(m_pEnemySpawner.get());
    m_pCurrentRoom->SetPlayerTarget(m_pPlayerGameObject);

    // 6. 룸 활성화 (적 자동 스폰)
    m_pCurrentRoom->SetState(RoomState::Active);
}
```

### 적에게 데미지 주기 (플레이어 공격 시)

```cpp
void OnPlayerAttack(ColliderComponent* pHitCollider)
{
    GameObject* pHitObject = pHitCollider->GetOwner();
    EnemyComponent* pEnemy = pHitObject->GetComponent<EnemyComponent>();

    if (pEnemy && !pEnemy->IsDead())
    {
        pEnemy->TakeDamage(25.0f);  // 25 데미지
    }
}
```

### 룸 클리어 이벤트 처리

```cpp
void CRoom::SetState(RoomState state)
{
    // ... 기존 코드 ...

    switch (m_eState)
    {
    case RoomState::Cleared:
        // 클리어 보상 지급
        SpawnReward();
        // 다음 룸으로 가는 문 열기
        OpenExitDoor();
        break;
    }
}
```

---

## 디버그 로그

시스템은 `OutputDebugString`을 통해 다음 로그를 출력합니다:

| 로그 | 설명 |
|------|------|
| `[Enemy] State changed: X -> Y` | 적 상태 전환 |
| `[Enemy] Took X damage, HP: Y/Z` | 적 피격 |
| `[Enemy] Died!` | 적 사망 |
| `[MeleeAttack] Attack started` | 근접 공격 시작 |
| `[MeleeAttack] HIT! Dealing X damage` | 근접 공격 명중 |
| `[Room] Spawning X enemies...` | 적 스폰 시작 |
| `[Room] Enemy died! (X/Y dead)` | 적 사망 카운트 |
| `[Room] Room cleared!` | 룸 클리어 |

Visual Studio의 **Output** 창에서 확인할 수 있습니다.

---

## 향후 확장 가이드

### 1. 원거리 공격 추가

```cpp
// RangedAttackBehavior.h 생성
class RangedAttackBehavior : public IAttackBehavior { ... };

// 프리셋 등록
archerData.m_fnCreateAttack = []() {
    return std::make_unique<RangedAttackBehavior>(15.0f, 20.0f); // 데미지, 사거리
};
```

### 2. 여러 페이즈 보스

```cpp
class BossAttackBehavior : public IAttackBehavior
{
    int m_nPhase = 1;

    void Update(float dt, EnemyComponent* pEnemy) override
    {
        float hpPercent = pEnemy->GetStats().m_fCurrentHP / pEnemy->GetStats().m_fMaxHP;

        if (hpPercent < 0.5f && m_nPhase == 1)
        {
            m_nPhase = 2;
            // 2페이즈 패턴으로 전환
        }
    }
};
```

### 3. 애니메이션 연동

```cpp
// EnemyComponent에 애니메이션 상태 연동 추가
void EnemyComponent::ChangeState(EnemyState newState)
{
    // ... 기존 코드 ...

    AnimationComponent* pAnim = m_pOwner->GetComponent<AnimationComponent>();
    if (pAnim)
    {
        switch (newState)
        {
        case EnemyState::Chase:
            pAnim->Play("Walk");
            break;
        case EnemyState::Attack:
            pAnim->Play("Attack");
            break;
        case EnemyState::Dead:
            pAnim->Play("Death");
            break;
        }
    }
}
```

### 4. 플레이어 체력 시스템 연동

`MeleeAttackBehavior::DealDamage()`에서 TODO 주석 부분 구현:

```cpp
void MeleeAttackBehavior::DealDamage(EnemyComponent* pEnemy)
{
    // ... 거리 체크 ...

    PlayerComponent* pPlayer = pTarget->GetComponent<PlayerComponent>();
    if (pPlayer)
    {
        pPlayer->TakeDamage(m_fDamage);  // PlayerComponent에 TakeDamage 추가 필요
    }
}
```

---

## 주의사항

1. **적 GameObject 소유권**: 적은 `CRoom`의 `m_vGameObjects`에 소유됩니다. 직접 delete하지 마세요.

2. **스포너 수명**: `EnemySpawner`는 `Scene`이 소유합니다. 룸에는 포인터만 전달됩니다.

3. **콜라이더 레이어**: 적은 `CollisionLayer::Enemy`로 설정됩니다. 충돌 마스크 설정에 주의하세요.

4. **메쉬 참조 카운트**: `CubeMesh`는 생성 후 `AddRef()`를 호출해야 합니다.

---

## 변경 이력

| 날짜 | 작성자 | 내용 |
|------|--------|------|
| 2026-01-27 | - | 초기 구현 (FSM, MeleeAttack, Spawner, Room 연동) |
