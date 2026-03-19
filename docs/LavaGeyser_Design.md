# LavaGeyserComponent 설계 문서

## 개요
화염 맵에서 주기적으로 용암 장판이 터지는 맵 기믹

## 스펙 요약
| 항목 | 값 |
|------|-----|
| 경고 시간 | 1.5초 |
| 스폰 주기 | 5초 |
| 동시 생성 | 2-3개 |
| 범위 크기 | 4-5m (반지름) |
| 데미지 | 20 |
| 타겟 방식 | 가장 가까운 플레이어 위치 |

---

## 1. 컴포넌트 설계

### LavaGeyserComponent
용암 장판 하나를 관리하는 컴포넌트

```
상태 머신:
Idle → Warning (범위 표시) → Erupting (폭발/데미지) → Idle
         1.5초                    0.3초
```

**주요 멤버**
- `m_eState`: 현재 상태 (Idle, Warning, Erupting)
- `m_fTimer`: 상태 타이머
- `m_fRadius`: 폭발 반지름 (4.5m 기본값)
- `m_fDamage`: 데미지량 (20.0f)
- `m_vTargetPosition`: 폭발 위치
- `m_pIndicator`: 범위 표시용 GameObject

**주요 메서드**
- `Activate(XMFLOAT3 position)`: 지정 위치에서 경고 시작
- `Update(float dt)`: 상태별 업데이트
- `DealDamage()`: 범위 내 모든 플레이어에게 데미지

---

## 2. LavaGeyserManager
여러 장판을 관리하고 주기적으로 스폰하는 매니저

**주요 멤버**
- `m_vecGeysers`: LavaGeyserComponent 풀 (5개)
- `m_fSpawnTimer`: 스폰 주기 타이머 (5초)
- `m_nSpawnCount`: 한 번에 스폰할 개수 (2-3)
- `m_bActive`: 활성화 상태

**주요 메서드**
- `Update(float dt)`: 주기적 스폰 관리
- `SpawnGeysers()`: 플레이어 위치 기반으로 장판 활성화
- `FindNearestPlayers()`: 가장 가까운 플레이어들 탐색
- `SetActive(bool)`: Room 상태에 따라 on/off

---

## 3. 수정할 기존 파일

### Room.h / Room.cpp
- `m_pGeyserManager` 멤버 추가
- `OnActivate()`: 매니저 활성화
- `OnCleared()`: 매니저 비활성화
- `Update()`: 매니저 업데이트 호출

### Scene.h / Scene.cpp
- 플레이어 목록 조회 메서드 추가 (멀티플레이어 대비)
- `GetAllPlayers()`: vector<GameObject*> 반환

---

## 4. 파일 목록

### 생성 완료
```
gaym/
├── LavaGeyserComponent.h    ✅ (구현 완료)
├── LavaGeyserComponent.cpp  ✅ (구현 완료)
├── LavaGeyserManager.h      ✅ (구현 완료)
└── LavaGeyserManager.cpp    ✅ (구현 완료)
```

### 수정 완료
```
gaym/
├── Room.h                   ✅ (m_pGeyserManager 추가)
├── Room.cpp                 ✅ (Update, SetState, InitLavaGeyserManager 수정)
├── Scene.cpp                ✅ (LavaGeyserManager 렌더링 추가)
├── gaym.vcxproj             ✅ (새 파일 등록)
└── gaym.vcxproj.filters     ✅ (필터 등록)
```

---

## 5. 범위 표시 구현

기존 `EnemyComponent::ShowIndicators()` 패턴 재활용:
- RingMesh로 Circle 타입 GameObject 생성
- 위치/스케일 설정으로 범위 표시
- 주황색/빨간색 Emissive 머티리얼

---

## 6. Room 통합 상세

### Room.h 추가 내용
```cpp
// Forward declaration
class LavaGeyserManager;

// 멤버 변수 (protected)
std::unique_ptr<LavaGeyserManager> m_pGeyserManager;

// public 메서드
void SetLavaGeyserEnabled(bool bEnabled);
void InitLavaGeyserManager(ID3D12Device*, ID3D12GraphicsCommandList*, Shader*);
```

### Room.cpp Update() 수정
```cpp
if (m_eState == RoomState::Active)
{
    // 기존 코드...

    // Geyser 업데이트 추가
    if (m_pGeyserManager)
    {
        m_pGeyserManager->Update(deltaTime);
    }
}
```

### Room.cpp SetState() 수정
```cpp
case RoomState::Active:
    if (m_pGeyserManager)
        m_pGeyserManager->SetActive(true);
    break;

case RoomState::Cleared:
    if (m_pGeyserManager)
        m_pGeyserManager->SetActive(false);
    break;
```

---

## 7. 검증 방법

1. 게임 실행 후 화염 맵 Room 진입
2. 5초마다 2-3개 장판이 플레이어 근처에 생성되는지 확인
3. 1.5초 경고 후 폭발 발생 확인
4. 범위 내 있을 때 20 데미지 받는지 확인
5. Room 클리어 시 장판 생성 중단 확인

---

## 8. 참고 기존 코드

| 기능 | 파일 | 라인 |
|------|------|------|
| 범위 표시 | EnemyComponent.cpp | ShowIndicators() :362-431 |
| 데미지 처리 | PlayerComponent.cpp | TakeDamage() :141-150 |
| 타이머 패턴 | MeleeAttackBehavior.cpp | Update() :30-56 |
| Room 상태 | Room.h | RoomState enum |
| 인디케이터 생성 | EnemySpawner.cpp | CreateIndicatorObject() :537-575 |
