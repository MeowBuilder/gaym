# GAYM 프로젝트 메모리

## 프로젝트 개요
DirectX 12 기반 3D 게임 엔진 프로젝트

## 최근 구현 내용 (2026-01-27)

### 적 몬스터 시스템 구현

#### 새로 생성된 파일
| 파일 | 역할 |
|------|------|
| `EnemyComponent.h/cpp` | 적 AI 컴포넌트 (FSM 상태머신) |
| `IAttackBehavior.h` | 공격 행동 인터페이스 (전략 패턴) |
| `MeleeAttackBehavior.h/cpp` | 근접 공격 구현체 |
| `EnemySpawnData.h` | 적 스폰 데이터 + RoomSpawnConfig |
| `EnemySpawner.h/cpp` | 적 생성 관리자 (프리셋 시스템) |

#### 수정된 파일
| 파일 | 변경 내용 |
|------|-----------|
| `Room.h/cpp` | 적 관리 기능 추가 (RegisterEnemy, OnEnemyDeath, CheckClearCondition, SpawnEnemies) |
| `Scene.h/cpp` | EnemySpawner 통합, 테스트 룸 설정 |
| `gaym.vcxproj` | 새 소스/헤더 파일 등록 |

#### 문서 파일
- `docs/EnemySystem.md` - 전체 시스템 상세 설명
- `docs/EnemySystem_QuickReference.md` - 빠른 참조 가이드
- `docs/EnemySystem_ClassDiagram.md` - 클래스 다이어그램

---

## 현재 아키텍처

### 적 시스템 구조
```
Scene
  ├── EnemySpawner (프리셋 관리, 적 생성)
  └── CRoom
        ├── m_vEnemies (적 추적)
        ├── m_SpawnConfig (스폰 설정)
        └── GameObject
              └── EnemyComponent (FSM AI)
                    └── IAttackBehavior (공격 패턴)
```

### EnemyComponent FSM 상태
```
Idle → Chase → Attack → (반복)
         ↓
      Stagger (피격 시)
         ↓
       Dead (HP <= 0) → Room::OnEnemyDeath()
```

### 주요 클래스 관계
- `EnemySpawner`: 적 프리셋 등록 및 생성
- `EnemyComponent`: FSM 기반 AI (Idle, Chase, Attack, Stagger, Dead)
- `IAttackBehavior`: 공격 전략 인터페이스
- `MeleeAttackBehavior`: 근접 공격 (윈드업 → 타격 → 회복)
- `CRoom`: 적 스폰, 사망 추적, 클리어 판정

---

## 기존 컴포넌트 시스템

### Component 계층
```
Component (base)
  ├── TransformComponent
  ├── RenderComponent
  ├── ColliderComponent
  ├── PlayerComponent
  ├── AnimationComponent
  ├── RotatorComponent
  └── EnemyComponent (신규)
```

### 핵심 파일
- `Scene.h/cpp` - 씬 관리, 게임오브젝트/룸 소유
- `Room.h/cpp` - 룸 시스템, 적 관리
- `GameObject.h/cpp` - 게임오브젝트, 컴포넌트 컨테이너
- `MeshLoader.h/cpp` - 메쉬 로딩
- `CollisionManager.h/cpp` - 충돌 처리
- `CollisionLayer.h` - 충돌 레이어 정의

---

## 테스트 설정

현재 Scene::Init()에서 다음과 같이 테스트 적 3마리 스폰:
```cpp
RoomSpawnConfig spawnConfig;
spawnConfig.AddSpawn("TestEnemy", 10.0f, 0.0f, 0.0f);   // 오른쪽
spawnConfig.AddSpawn("TestEnemy", -10.0f, 0.0f, 0.0f);  // 왼쪽
spawnConfig.AddSpawn("TestEnemy", 0.0f, 0.0f, 15.0f);   // 앞쪽
```

TestEnemy 프리셋:
- 크기: 1x2x1 (빨간색 CubeMesh)
- HP: 50
- 이동속도: 4
- 공격 사거리: 3
- 공격 데미지: 10

---

## 향후 작업 예정

- [ ] 플레이어 체력 시스템 (PlayerComponent::TakeDamage)
- [ ] 원거리 공격 (RangedAttackBehavior)
- [ ] 보스 몬스터 (다중 페이즈)
- [ ] 적 애니메이션 연동
- [ ] 적 메쉬 로딩 (현재 CubeMesh만 지원)

---

## 빌드 정보

- Visual Studio 2022
- C++20 (stdcpplatest)
- DirectX 12
- Platform: x64
- 빌드 출력: `x64/Release/gaym.exe`
