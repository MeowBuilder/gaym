# Gaym Project Architecture Design Document
**Date:** 2026-01-11
**Topic:** Stage & Room Based Hierarchical System

## 1. 개요 (Overview)
본 프로젝트는 5개의 선형적인 스테이지로 구성되며, 각 스테이지는 순차적으로 진행되는 여러 개의 '방(Room)'으로 이루어진다.
효율적인 리소스 관리와 게임 로직 처리를 위해 기존의 평면적인 `Scene -> GameObject` 구조를 **계층적 구조**로 개편한다.

---

## 2. 핵심 계층 구조 (Core Hierarchy)

### 전체 구조도
```text
[CGameFramework] (Application Root)
 │
 ├── [CScene] (Current Stage Manager)
 │    │  * 담당: 현재 스테이지(예: 불의 신전) 전체 관리, 전역 리소스(카메라, 조명)
 │    │
 │    ├── [Player] (Global Entity)
 │    │      * 플레이어는 방에 종속되지 않고 스테이지 전체를 이동함.
 │    │
 │    ├── [CRoom 0] (Start Room)
 │    │    ├── [Static Geometry] (Walls, Floors - Octree/Optimized)
 │    │    ├── [Triggers] (Entry/Exit Event)
 │    │    └── [Enemies] (Lazy Loaded or Pooled)
 │    │
 │    ├── [CRoom 1] (Battle Room)
 │    │    ├── [Gate] (Locked Door)
 │    │    └── [Gimmicks] (Traps)
 │    │
 │    └── ... [CRoom N]
 │
 └── [UI/HUD]
```

---

## 3. 클래스별 상세 역할 (Class Responsibilities)

### A. CScene (Stage)
*   **역할:** 하나의 거대한 스테이지(레벨)를 의미한다.
*   **주요 데이터:**
    *   `m_pPlayer`: 플레이어 오브젝트 포인터.
    *   `m_vRooms`: 해당 스테이지를 구성하는 `CRoom` 객체들의 리스트.
    *   `m_pCurrentRoom`: 현재 플레이어가 위치한 방을 가리키는 포인터.
*   **주요 로직:**
    *   **Update:** 전체 오브젝트를 순회하지 않고, `m_pCurrentRoom`과 (필요하다면) 인접한 방의 `Update`만 호출하여 연산량을 줄인다.
    *   **Render:** 현재 카메라 시야(Frustum)에 있는 방과 오브젝트만 렌더링하도록 요청한다.

### B. CRoom (Logical Unit)
*   **역할:** 게임 플레이의 최소 단위. 전투 발생, 퍼즐 해결 등이 일어나는 공간.
*   **상태 (State):**
    *   `Inactive`: 플레이어가 멀리 있음. 적/기믹 업데이트 안 함.
    *   `Active`: 플레이어 진입. 적 생성/활성화, AI 동작 시작.
    *   `Cleared`: 모든 적 처치/조건 달성. 문이 열리고 적 생성 중단.
*   **구성 요소:**
    *   `BoundingBox`: 방의 영역 (플레이어 진입 감지용).
    *   `SpawnPoints`: 적들이 생성될 위치 정보.
    *   `m_vEnemies`: 현재 활성화된 적 리스트.
    *   `m_vProps`: 배경, 장식물, 함정 등.

---

## 4. 게임 플레이 루프 (Gameplay Loop)

1.  **진입 (Room Entry):**
    *   플레이어가 `Room N`의 `BoundingBox`와 충돌.
    *   `CScene`이 `CurrentRoom`을 갱신.
    *   `Room N`의 상태가 `Active`로 변경.
        *   입구 문이 닫힘.
        *   `SpawnPoints` 정보를 바탕으로 적(`Enemy`)들을 `CreateGameObject` 하거나 풀에서 가져옴.

2.  **전투 (Battle & Logic):**
    *   `CScene::Update` -> `CRoom::Update` 호출.
    *   방 내부의 적 AI 구동.
    *   기믹(함정 등) 동작.

3.  **클리어 (Room Clear):**
    *   적 사망 시 `CRoom`의 남은 적 카운트 감소.
    *   카운트가 0이 되면 `Cleared` 상태로 전환.
    *   다음 방으로 가는 문(Gate) 개방 애니메이션 및 충돌 해제.

---

## 5. 데이터 주도적 설계 (Data-Driven)
하드코딩을 피하기 위해 각 스테이지와 방의 정보는 파일(JSON/Binary/Text)로 정의한다.

**예시 데이터 포맷 (Stage1.map)**
```text
[STAGE_INFO]
Name: FireTemple
Skybox: Sky_Fire.dds

[ROOM_0]
Area: -50, 0, -50, 50, 20, 50 (Min/Max)
Type: Safe
StaticMesh: Room0_Floor.bin

[ROOM_1]
Area: 50, 0, -50, 150, 20, 50
Type: Battle
Enemy: Goblin, 3 (Count), SpawnPos(70, 0, 0)
Gate: Door_Iron, Pos(150, 0, 0)
```

## 6. 구현 단계 (Action Plan)
1.  **CRoom 클래스 정의:** `GameObject` 리스트와 상태를 관리하는 기본 클래스 작성.
2.  **CScene 리팩토링:** `m_vGameObjects`를 직접 관리하는 방식에서 `CRoom`을 통해 관리하는 방식으로 변경.
3.  **Trigger 구현:** 방 진입을 감지하는 AABB 충돌 처리.
4.  **Enemy Spawning:** 방 진입 시 적을 동적으로 생성하는 로직 구현.
