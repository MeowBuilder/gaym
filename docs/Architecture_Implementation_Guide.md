# Gaym Project Architecture Implementation Guide
**Date:** 2026-01-19
**Version:** 1.0 (Room System Implemented)

## 1. 아키텍처 개요 (Architecture Overview)
이 프로젝트는 **계층적(Hierarchical) 관리 구조**를 따릅니다. 상위 객체는 하위 객체의 생명주기(Life-cycle)를 관리하고, 하위 객체는 상위 객체의 로직에 종속됩니다.

### 계층 구조 (Hierarchy)
```text
[CGameFramework] (App Root)
 │
 ├── [CScene] (Stage / Level)
 │    │  * 역할: 스테이지 전역 관리, 카메라, 조명, 플레이어
 │    │
 │    ├── [Player] (Global GameObject)
 │    │      * 씬에 직접 속함 (방 이동 시에도 유지됨)
 │    │
 │    ├── [CRoom 0] (Start Room)
 │    │    ├── [GameObject] (Terrain, Walls)
 │    │    └── [GameObject] (Enemies, Props)
 │    │
 │    ├── [CRoom 1] (Next Room)
 │    │    └── ...
 │    │
 │    └── [CurrentRoom Pointer] -> (Points to active CRoom)
```

---

## 2. 객체 생성 및 배치 (Object Creation & Placement)

### A. 전역 객체 생성 (플레이어 등)
플레이어처럼 방을 넘나드는 객체는 `CScene`이 직접 관리해야 합니다.

**코드 작성 위치:** `Scene::Init()`
```cpp
// 1. 방 관리 포인터를 잠시 해제 (중요!)
m_pCurrentRoom = nullptr; 

// 2. 객체 로드 (자동으로 Scene::m_vGameObjects에 추가됨)
GameObject* pPlayer = MeshLoader::LoadGeometryFromFile(..., "PlayerModel.bin");

// 3. 컴포넌트 부착 및 설정
pPlayer->AddComponent<PlayerComponent>();
m_pPlayerGameObject = pPlayer;
```

### B. 지역 객체 생성 (적, 지형, 장식물)
특정 방에 속하는 오브젝트는 해당 방이 활성화(`m_pCurrentRoom` 설정)된 상태에서 생성해야 합니다.

**코드 작성 위치:** `Scene::Init()` 또는 `Scene::LoadSceneFromFile()`
```cpp
// 1. 타겟 방 활성화 (또는 생성)
m_pCurrentRoom = m_vRooms[0].get(); 

// 2. 객체 생성 (자동으로 m_pCurrentRoom->m_vGameObjects에 추가됨)
GameObject* pEnemy = CreateGameObject(pDevice, pCommandList);

// 3. 위치 설정 (방의 로컬 좌표계가 아닌 월드 좌표계 사용 주의)
pEnemy->GetTransform()->SetPosition(100.0f, 0.0f, 50.0f);

// 4. 모델 로드 (MeshLoader 사용 시)
GameObject* pModel = MeshLoader::LoadGeometryFromFile(..., "Enemy.bin");
// 주의: MeshLoader는 내부적으로 CreateGameObject를 호출하므로, 
// 생성된 모델은 자동으로 현재 방에 등록됩니다.
```

---

## 3. 런타임 로직 흐름 (Runtime Logic Flow)

### Update 흐름
매 프레임 `GameFramework` -> `Scene::Update` 호출 시:

1.  **카메라 & 조명 업데이트:** 전역 렌더링 파라미터 갱신.
2.  **전역 오브젝트(Player) 업데이트:** 플레이어 입력 처리 및 이동.
3.  **현재 방(Current Room) 업데이트:**
    *   `m_pCurrentRoom`이 `Active` 상태인지 확인.
    *   방 내부의 모든 오브젝트(`Enemies`, `Props`) `Update` 호출.
    *   **최적화:** 다른 방(`Inactive`)의 오브젝트는 업데이트되지 않음.
4.  **방 전이(Transition) 체크:**
    *   `Scene`에서 플레이어 위치를 확인하여 방 변경 조건 검사.
    *   조건 충족 시 `m_pCurrentRoom` 포인터 변경.

### Render 흐름
`Scene::Render` 호출 시:

1.  **셰이더 바인딩:** PSO, RootSignature 설정.
2.  **렌더링 실행:**
    *   현재 구조에서는 `Shader` 클래스가 등록된 모든 `RenderComponent`를 순회하며 그립니다.
    *   **주의:** 현재는 모든 방의 오브젝트가 셰이더 리스트에 등록되어 있으므로 전체가 그려집니다. (추후 최적화 대상: 활성 방의 오브젝트만 그리도록 개선 예정)

---

## 4. 개발자 가이드 (How-to)

### Q1. 새로운 방(Room)을 추가하려면?
1.  `Scene::Init`에서 `CRoom` 인스턴스를 생성합니다.
2.  `m_vRooms` 벡터에 추가합니다.
3.  필요하다면 `BoundingBox`를 설정합니다.

```cpp
auto pNewRoom = std::make_unique<CRoom>();
pNewRoom->SetBoundingBox(BoundingBox(...));
m_vRooms.push_back(std::move(pNewRoom));
```

### Q2. 특정 방에 적을 배치하려면?
1.  `m_pCurrentRoom`을 해당 방으로 설정합니다.
2.  `CreateGameObject` 또는 `MeshLoader`를 호출합니다.

```cpp
// 2번 방에 배치하고 싶다면
m_pCurrentRoom = m_vRooms[1].get();
CreateGameObject(...); // 2번 방으로 들어감
```

### Q3. 방을 이동시키는 로직은 어디에?
`Scene::Update` 함수 내의 `// 2. Update Current Room` 주석 아래에 작성합니다.
```cpp
if (m_pCurrentRoom == m_vRooms[0].get() && PlayerPos.x > 100.0f) {
    m_pCurrentRoom->SetState(RoomState::Inactive);
    m_pCurrentRoom = m_vRooms[1].get();
    m_pCurrentRoom->SetState(RoomState::Active);
}
```
