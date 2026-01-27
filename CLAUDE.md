# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 프로젝트 개요
DirectX 12 기반 3D 게임 엔진 프로젝트 (Windows)

## 빌드 명령어

```bash
# Visual Studio 2022 Developer Command Prompt에서 실행
msbuild gaym/gaym.sln /p:Configuration=Release /p:Platform=x64

# 또는 Visual Studio에서 gaym/gaym.sln 열어서 빌드
```

- **솔루션 파일**: `gaym/gaym.sln`
- **프로젝트 파일**: `gaym/gaym.vcxproj`
- **빌드 출력**: `x64/Release/gaym.exe`
- **C++ 표준**: C++20 (`/std:c++latest`)
- **플랫폼**: x64

## 아키텍처

### 핵심 구조
```
Dx12App (Application Root - 싱글톤)
  ├── Scene (현재 스테이지 관리)
  │     ├── Player (전역 엔티티)
  │     ├── CRoom[] (방 단위 논리 구역)
  │     │     └── GameObject[] (방 내 오브젝트)
  │     ├── CollisionManager (충돌 처리)
  │     └── EnemySpawner (적 프리셋 관리)
  └── InputSystem (입력 처리)
```

### 컴포넌트 시스템
GameObject는 컴포넌트 컨테이너. 주요 컴포넌트:

| Component | 역할 |
|-----------|------|
| `TransformComponent` | 위치/회전/스케일 |
| `RenderComponent` | 렌더링 |
| `ColliderComponent` | 충돌 |
| `PlayerComponent` | 플레이어 로직 |
| `AnimationComponent` | 애니메이션 |
| `EnemyComponent` | 적 AI (FSM) |

컴포넌트 추가/조회:
```cpp
auto* enemy = gameObject->AddComponent<EnemyComponent>(target, 50.f, 4.f, 3.f); // HP, speed, range
auto* transform = gameObject->GetComponent<TransformComponent>();
```

### Room 시스템
방 상태: `Inactive` → `Active` (플레이어 진입) → `Cleared` (모든 적 처치)

Room은 적 스폰 및 클리어 판정을 관리:
```cpp
room->SpawnEnemies(pDevice, pCommandList, pScene, spawner, config);
room->OnEnemyDeath(enemy);  // 사망 시 호출
room->CheckClearCondition(); // 클리어 판정
```

### 적 시스템
- `EnemySpawner`: 적 프리셋 등록 및 생성
- `EnemyComponent`: FSM (Idle → Chase → Attack, Stagger, Dead)
- `IAttackBehavior`: 공격 전략 인터페이스
- `MeleeAttackBehavior`: 근접 공격 구현

적 생성 예시:
```cpp
EnemyPreset preset;
preset.name = "Goblin";
preset.hp = 50.f;
preset.speed = 4.f;
preset.attackRange = 3.f;
spawner->RegisterPreset(preset);
spawner->SpawnEnemy("Goblin", position, device, cmdList);
```

### 충돌 레이어
`CollisionLayer.h`에 정의:
- `Player`, `Enemy`, `PlayerAttack`, `EnemyAttack`, `Environment`, `Trigger`

### 주요 파일 위치
- 엔트리포인트: `gaym/gaym.cpp`
- DX12 초기화: `gaym/Dx12App.cpp`
- 씬 관리: `gaym/Scene.cpp`
- 셰이더: `gaym/shaders.hlsl`

## 상세 문서
- `docs/Architecture_Design.md` - Stage/Room 계층 구조 설계
- `docs/EnemySystem.md` - 적 시스템 상세 설명
- `docs/EnemySystem_QuickReference.md` - 적 시스템 빠른 참조

## 향후 작업 예정
- [ ] 플레이어 체력 시스템 (PlayerComponent::TakeDamage)
- [ ] 원거리 공격 (RangedAttackBehavior)
- [ ] 보스 몬스터 (다중 페이즈)
- [ ] 적 애니메이션/메쉬 연동
