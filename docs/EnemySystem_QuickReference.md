# 적 시스템 빠른 참조 가이드

## 새로운 적 타입 추가하기 (5단계)

### Step 1: 스폰 데이터 정의
```cpp
EnemySpawnData myEnemyData;
myEnemyData.m_xmf3Scale = XMFLOAT3(1.0f, 2.0f, 1.0f);      // 크기
myEnemyData.m_xmf4Color = XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f); // 색상 (녹색)
```

### Step 2: 스탯 설정
```cpp
myEnemyData.m_Stats.m_fMaxHP = 100.0f;
myEnemyData.m_Stats.m_fCurrentHP = 100.0f;
myEnemyData.m_Stats.m_fMoveSpeed = 5.0f;
myEnemyData.m_Stats.m_fAttackRange = 3.0f;
myEnemyData.m_Stats.m_fAttackCooldown = 2.0f;
```

### Step 3: 공격 패턴 설정
```cpp
myEnemyData.m_fnCreateAttack = []() {
    return std::make_unique<MeleeAttackBehavior>(
        15.0f,   // 데미지
        0.3f,    // 윈드업 시간
        0.2f,    // 타격 시간
        0.3f     // 회복 시간
    );
};
```

### Step 4: 프리셋 등록 (Scene::Init에서)
```cpp
m_pEnemySpawner->RegisterEnemyPreset("MyEnemy", myEnemyData);
```

### Step 5: 룸에 스폰 위치 추가
```cpp
RoomSpawnConfig config;
config.AddSpawn("MyEnemy", 10.0f, 0.0f, 5.0f);  // 위치 지정
pRoom->SetSpawnConfig(config);
```

---

## 자주 사용하는 코드 스니펫

### 적에게 데미지 주기
```cpp
EnemyComponent* pEnemy = pGameObject->GetComponent<EnemyComponent>();
if (pEnemy && !pEnemy->IsDead())
{
    pEnemy->TakeDamage(25.0f);
}
```

### 현재 룸의 적 상태 확인
```cpp
int alive = pRoom->GetAliveEnemyCount();
int total = pRoom->GetTotalEnemyCount();
bool isCleared = (pRoom->GetState() == RoomState::Cleared);
```

### 수동으로 적 스폰
```cpp
GameObject* pEnemy = m_pEnemySpawner->SpawnEnemy(
    pRoom,
    "TestEnemy",
    XMFLOAT3(x, y, z),
    pPlayerGameObject
);
```

### 적 상태 강제 변경
```cpp
pEnemy->ChangeState(EnemyState::Stagger);  // 경직 상태로
pEnemy->ChangeState(EnemyState::Dead);     // 즉사
```

---

## 파일별 역할 요약

| 파일 | 핵심 역할 |
|------|-----------|
| `EnemyComponent.h/cpp` | 적 AI 두뇌 (FSM) |
| `IAttackBehavior.h` | 공격 패턴 인터페이스 |
| `MeleeAttackBehavior.h/cpp` | 근접 공격 구현 |
| `EnemySpawnData.h` | 적 설정 데이터 |
| `EnemySpawner.h/cpp` | 적 생성 공장 |
| `Room.h/cpp` | 적 관리 & 클리어 판정 |

---

## FSM 상태 전이표

| 현재 상태 | 조건 | 다음 상태 |
|-----------|------|-----------|
| Idle | 타겟 존재 | Chase |
| Chase | 공격 범위 내 + 쿨다운 완료 | Attack |
| Chase | 타겟 없음 | Idle |
| Attack | 공격 완료 | Chase |
| Any | 피격 | Stagger |
| Stagger | 0.5초 경과 | Chase |
| Any | HP <= 0 | Dead |

---

## MeleeAttackBehavior 타이밍

```
Execute() 호출
     │
     ▼
┌─────────────────────────────────────────────────────┐
│  Windup (0.3s)  │  Hit (0.2s)  │  Recovery (0.3s)  │
│    준비 동작     │  데미지 판정  │    회복 동작      │
└─────────────────────────────────────────────────────┘
                       ↑
                 DealDamage() 호출
                       │
                       ▼
                 IsFinished() = true
```

---

## 디버그 로그 필터

Visual Studio Output 창에서 `[Enemy]`, `[Room]`, `[MeleeAttack]` 등으로 검색하면 관련 로그만 볼 수 있습니다.

```
[Enemy] State changed: Idle -> Chase
[Enemy] State changed: Chase -> Attack
[MeleeAttack] Attack started - windup phase
[MeleeAttack] HIT! Dealing 10.0 damage to player
[MeleeAttack] Attack finished
[Enemy] Took 50.0 damage, HP: 0.0/50.0
[Enemy] State changed: Attack -> Dead
[Enemy] Died!
[Room] Enemy died! (1/3 dead)
```

---

## 체크리스트: 새 룸에 적 추가

- [ ] 룸에 `SetEnemySpawner()` 호출
- [ ] 룸에 `SetPlayerTarget()` 호출
- [ ] `RoomSpawnConfig` 생성 및 스폰 위치 추가
- [ ] 룸에 `SetSpawnConfig()` 호출
- [ ] 룸을 `RoomState::Active`로 설정

---

## 자주 발생하는 문제

### Q: 적이 스폰되지 않음
**A:** 다음을 확인하세요:
1. `SetEnemySpawner()` 호출 여부
2. `SetSpawnConfig()`에 스폰 위치 추가 여부
3. 룸 상태가 `Active`인지 확인

### Q: 적이 움직이지 않음
**A:** `SetPlayerTarget()` 호출 여부 확인

### Q: 적이 공격하지 않음
**A:** `SetAttackBehavior()` 호출 여부 확인 (EnemySpawner 사용 시 자동 설정됨)

### Q: 룸이 클리어되지 않음
**A:** 모든 적이 `EnemyState::Dead` 상태인지 확인. `OnEnemyDeath()` 콜백이 정상 호출되는지 로그 확인.
