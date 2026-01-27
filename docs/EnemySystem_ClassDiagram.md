# 적 시스템 클래스 다이어그램

## 클래스 관계도

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              Scene                                          │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │ - m_pEnemySpawner: unique_ptr<EnemySpawner>                         │   │
│  │ - m_vRooms: vector<unique_ptr<CRoom>>                               │   │
│  │ - m_pPlayerGameObject: GameObject*                                   │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│         │                          │                                        │
│         │ owns                     │ owns                                   │
│         ▼                          ▼                                        │
│  ┌──────────────┐           ┌──────────────┐                               │
│  │ EnemySpawner │           │    CRoom     │                               │
│  └──────────────┘           └──────────────┘                               │
└─────────────────────────────────────────────────────────────────────────────┘

                           상세 클래스 구조
┌─────────────────────────────────────────────────────────────────────────────┐
│                                                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                         EnemySpawner                                 │   │
│  ├─────────────────────────────────────────────────────────────────────┤   │
│  │ - m_mapPresets: map<string, EnemySpawnData>                         │   │
│  │ - m_pDevice: ID3D12Device*                                          │   │
│  │ - m_pScene: Scene*                                                  │   │
│  │ - m_pShader: Shader*                                                │   │
│  ├─────────────────────────────────────────────────────────────────────┤   │
│  │ + Init(device, cmdList, scene, shader)                              │   │
│  │ + RegisterEnemyPreset(name, data)                                   │   │
│  │ + SpawnEnemy(room, preset, pos, target) : GameObject*               │   │
│  │ + SpawnTestEnemy(room, pos, target) : GameObject*                   │   │
│  │ + SpawnRoomEnemies(room, config, target)                            │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│         │                                                                   │
│         │ creates                                                           │
│         ▼                                                                   │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                         GameObject                                   │   │
│  │  ┌─────────────────────────────────────────────────────────────┐    │   │
│  │  │ Components:                                                  │    │   │
│  │  │  - TransformComponent                                        │    │   │
│  │  │  - EnemyComponent  ◄──────────────────────────────────┐     │    │   │
│  │  │  - ColliderComponent                                   │     │    │   │
│  │  │  - RenderComponent                                     │     │    │   │
│  │  └─────────────────────────────────────────────────────────────┘    │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                                      │                      │
│                                                      │ has                  │
│                                                      ▼                      │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                       EnemyComponent                                 │   │
│  ├─────────────────────────────────────────────────────────────────────┤   │
│  │ - m_eCurrentState: EnemyState                                       │   │
│  │ - m_Stats: EnemyStats                                               │   │
│  │ - m_pAttackBehavior: unique_ptr<IAttackBehavior>                    │   │
│  │ - m_pTarget: GameObject*                                            │   │
│  │ - m_pRoom: CRoom*                                                   │   │
│  │ - m_OnDeathCallback: function<void(EnemyComponent*)>                │   │
│  ├─────────────────────────────────────────────────────────────────────┤   │
│  │ + Update(deltaTime)                                                 │   │
│  │ + ChangeState(newState)                                             │   │
│  │ + TakeDamage(damage)                                                │   │
│  │ + SetTarget(target)                                                 │   │
│  │ + SetAttackBehavior(behavior)                                       │   │
│  │ + GetDistanceToTarget() : float                                     │   │
│  │ + FaceTarget()                                                      │   │
│  │ + MoveTowardsTarget(dt)                                             │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│         │                                                                   │
│         │ uses                                                              │
│         ▼                                                                   │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                    <<interface>>                                     │   │
│  │                    IAttackBehavior                                   │   │
│  ├─────────────────────────────────────────────────────────────────────┤   │
│  │ + Execute(enemy)                                                    │   │
│  │ + Update(dt, enemy)                                                 │   │
│  │ + IsFinished() : bool                                               │   │
│  │ + Reset()                                                           │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                          △                                                  │
│                          │ implements                                       │
│                          │                                                  │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                   MeleeAttackBehavior                                │   │
│  ├─────────────────────────────────────────────────────────────────────┤   │
│  │ - m_fDamage: float                                                  │   │
│  │ - m_fWindupTime: float                                              │   │
│  │ - m_fHitTime: float                                                 │   │
│  │ - m_fRecoveryTime: float                                            │   │
│  │ - m_fTimer: float                                                   │   │
│  │ - m_bHitDealt: bool                                                 │   │
│  │ - m_bFinished: bool                                                 │   │
│  ├─────────────────────────────────────────────────────────────────────┤   │
│  │ + Execute(enemy)                                                    │   │
│  │ + Update(dt, enemy)                                                 │   │
│  │ + IsFinished() : bool                                               │   │
│  │ + Reset()                                                           │   │
│  │ - DealDamage(enemy)                                                 │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘


                              CRoom 확장
┌─────────────────────────────────────────────────────────────────────────────┐
│                              CRoom                                          │
├─────────────────────────────────────────────────────────────────────────────┤
│ 기존 멤버:                                                                  │
│ - m_vGameObjects: vector<unique_ptr<GameObject>>                           │
│ - m_eState: RoomState                                                      │
│ - m_BoundingBox: BoundingBox                                               │
│                                                                             │
│ 새로 추가된 멤버:                                                           │
│ - m_vEnemies: vector<EnemyComponent*>        ◄── 적 추적용 (소유 X)        │
│ - m_nTotalEnemies: int                                                     │
│ - m_nDeadEnemies: int                                                      │
│ - m_SpawnConfig: RoomSpawnConfig                                           │
│ - m_pSpawner: EnemySpawner*                  ◄── Scene 소유                │
│ - m_pPlayerTarget: GameObject*              ◄── Scene 소유                 │
│ - m_bEnemiesSpawned: bool                                                  │
├─────────────────────────────────────────────────────────────────────────────┤
│ 새로 추가된 메서드:                                                         │
│ + SetSpawnConfig(config)                                                   │
│ + SetEnemySpawner(spawner)                                                 │
│ + SetPlayerTarget(player)                                                  │
│ + RegisterEnemy(enemy)                       ◄── EnemySpawner가 호출       │
│ + OnEnemyDeath(enemy)                        ◄── EnemyComponent 콜백       │
│ + GetAliveEnemyCount() : int                                               │
│ + SpawnEnemies()                             ◄── Active 상태 진입 시       │
└─────────────────────────────────────────────────────────────────────────────┘


                            데이터 구조체
┌─────────────────────────────────────────────────────────────────────────────┐
│                                                                             │
│  ┌────────────────────────┐      ┌────────────────────────────────────┐    │
│  │      EnemyStats        │      │         EnemySpawnData             │    │
│  ├────────────────────────┤      ├────────────────────────────────────┤    │
│  │ m_fMaxHP: float        │      │ m_strMeshPath: string              │    │
│  │ m_fCurrentHP: float    │◄─────│ m_strAnimationPath: string         │    │
│  │ m_fMoveSpeed: float    │      │ m_xmf3Scale: XMFLOAT3              │    │
│  │ m_fAttackRange: float  │      │ m_xmf4Color: XMFLOAT4              │    │
│  │ m_fAttackCooldown: float│     │ m_Stats: EnemyStats                │    │
│  │ m_fDetectionRange: float│     │ m_fnCreateAttack: function<...>    │    │
│  └────────────────────────┘      └────────────────────────────────────┘    │
│                                                                             │
│  ┌────────────────────────────────────────────────────────────────────┐    │
│  │                       RoomSpawnConfig                               │    │
│  ├────────────────────────────────────────────────────────────────────┤    │
│  │ m_vEnemySpawns: vector<pair<string, XMFLOAT3>>                      │    │
│  │                        │          │                                 │    │
│  │                  프리셋 이름    스폰 위치                            │    │
│  ├────────────────────────────────────────────────────────────────────┤    │
│  │ + AddSpawn(presetName, position)                                    │    │
│  │ + AddSpawn(presetName, x, y, z)                                     │    │
│  │ + Clear()                                                           │    │
│  └────────────────────────────────────────────────────────────────────┘    │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘


                         상태 전이 다이어그램
┌─────────────────────────────────────────────────────────────────────────────┐
│                                                                             │
│                              EnemyState FSM                                 │
│                                                                             │
│     ┌──────────────────────────────────────────────────────────────┐       │
│     │                                                              │       │
│     ▼                                                              │       │
│  ┌──────┐    타겟 존재    ┌───────┐                               │       │
│  │ Idle │ ──────────────▶ │ Chase │ ◄─────────────────────────────┤       │
│  └──────┘                 └───────┘                               │       │
│                               │                                    │       │
│                               │ 공격 범위 내                        │       │
│                               │ + 쿨다운 완료                       │       │
│                               ▼                                    │       │
│                          ┌────────┐    공격 완료                   │       │
│                          │ Attack │ ──────────────────────────────┘       │
│                          └────────┘                                        │
│                               │                                            │
│     ┌─────────────────────────┼─────────────────────────────┐              │
│     │                         │                             │              │
│     │ 피격 (HP > 0)           │                             │              │
│     ▼                         │                             │              │
│  ┌─────────┐    0.5초 경과    │                             │              │
│  │ Stagger │ ─────────────────┘                             │              │
│  └─────────┘                                                │              │
│     │                                                       │              │
│     │ HP <= 0                                               │ HP <= 0      │
│     ▼                                                       ▼              │
│  ┌──────┐ ◄─────────────────────────────────────────────────               │
│  │ Dead │                                                                  │
│  └──────┘                                                                  │
│     │                                                                      │
│     │ OnDeathCallback                                                      │
│     ▼                                                                      │
│  Room::OnEnemyDeath() → CheckClearCondition()                              │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘


                           실행 흐름도
┌─────────────────────────────────────────────────────────────────────────────┐
│                                                                             │
│  Scene::Init()                                                             │
│       │                                                                     │
│       ├──▶ EnemySpawner::Init()                                            │
│       │         │                                                           │
│       │         └──▶ RegisterEnemyPreset("TestEnemy", ...)                 │
│       │                                                                     │
│       ├──▶ RoomSpawnConfig 설정                                            │
│       │                                                                     │
│       └──▶ CRoom 설정                                                      │
│                 │                                                           │
│                 ├──▶ SetSpawnConfig()                                      │
│                 ├──▶ SetEnemySpawner()                                     │
│                 ├──▶ SetPlayerTarget()                                     │
│                 └──▶ SetState(Active)                                      │
│                                                                             │
│  ════════════════════════════════════════════════════════════════════════  │
│                                                                             │
│  Scene::Update() [매 프레임]                                               │
│       │                                                                     │
│       └──▶ CRoom::Update()                                                 │
│                 │                                                           │
│                 ├──▶ SpawnEnemies() [최초 1회]                             │
│                 │         │                                                 │
│                 │         └──▶ EnemySpawner::SpawnRoomEnemies()            │
│                 │                   │                                       │
│                 │                   ├──▶ CreateCubeEnemy()                 │
│                 │                   ├──▶ SetupEnemyComponents()            │
│                 │                   │         │                             │
│                 │                   │         ├──▶ AddComponent<Enemy>()   │
│                 │                   │         ├──▶ SetAttackBehavior()     │
│                 │                   │         └──▶ SetOnDeathCallback()    │
│                 │                   │                                       │
│                 │                   └──▶ RegisterEnemy()                   │
│                 │                                                           │
│                 ├──▶ GameObject::Update() [각 적]                          │
│                 │         │                                                 │
│                 │         └──▶ EnemyComponent::Update()                    │
│                 │                   │                                       │
│                 │                   └──▶ FSM 상태별 처리                   │
│                 │                                                           │
│                 └──▶ CheckClearCondition()                                 │
│                           │                                                 │
│                           └──▶ 모두 사망 시 SetState(Cleared)              │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

## 의존성 그래프

```
Scene
  │
  ├───────────────────────┐
  │                       │
  ▼                       ▼
EnemySpawner ◄────────── CRoom
  │                       │
  │                       │ tracks
  ▼                       ▼
GameObject ◄───────── EnemyComponent
  │                       │
  │ has                   │ uses
  ▼                       ▼
Components          IAttackBehavior
  │                       △
  ├─ Transform            │
  ├─ Collider             │
  ├─ Render         MeleeAttackBehavior
  └─ Enemy
```

## 파일 의존성

```
stdafx.h
    │
    ├──▶ EnemyComponent.h
    │         │
    │         ├──▶ Component.h
    │         └──▶ DirectXMath.h
    │
    ├──▶ IAttackBehavior.h (독립)
    │
    ├──▶ MeleeAttackBehavior.h
    │         │
    │         └──▶ IAttackBehavior.h
    │
    ├──▶ EnemySpawnData.h
    │         │
    │         ├──▶ stdafx.h
    │         └──▶ EnemyComponent.h (EnemyStats)
    │
    ├──▶ EnemySpawner.h
    │         │
    │         └──▶ EnemySpawnData.h
    │
    └──▶ Room.h
              │
              └──▶ EnemySpawnData.h (RoomSpawnConfig)
```
