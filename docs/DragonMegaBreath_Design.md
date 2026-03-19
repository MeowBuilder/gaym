# 드래곤 보스 메가 브레스 기믹 설계 문서

## 개요
드래곤 보스가 대규모 브레스를 발동하면 플레이어가 맵에 배치된 엄폐물(바위/기둥) 뒤에 숨어야 하는 레이드 스타일 기믹

## 스펙 요약
| 항목 | 값 |
|------|-----|
| 경고 시간 | 3초 (차징) |
| 브레스 지속 | 2초 |
| 데미지 | 즉사 (HP 90% 이상) |
| 카메라 쉐이크 | 강도 0.5, 지속 2.5초 |
| 엄폐물 뒤 | 데미지 무효화 |
| 발동 조건 | HP 70%, 40% 도달 시 (페이즈 전환) |

---

## 1. 카메라 쉐이크 시스템

### Camera.h 수정

**추가할 멤버:**
```cpp
// Camera shake
bool m_bShaking = false;
float m_fShakeIntensity = 0.0f;
float m_fShakeDuration = 0.0f;
float m_fShakeTimer = 0.0f;
float m_fShakeFrequency = 25.0f;  // 진동 빈도
XMFLOAT3 m_vShakeOffset = { 0, 0, 0 };
```

**추가할 메서드:**
```cpp
void StartShake(float fIntensity, float fDuration);
void StopShake();
void UpdateShake(float dt);
```

### Camera.cpp 수정

**UpdateShake() 구현:**
- 사인파 기반 랜덤 오프셋 생성
- X, Y 축 방향으로 카메라 위치 흔들림
- 시간 경과에 따라 intensity 감쇠

**UpdateViewMatrix() 수정:**
- m_vShakeOffset을 카메라 위치에 적용

---

## 2. MegaBreathAttackBehavior

### 새 파일: MegaBreathAttackBehavior.h / .cpp

**상태 머신:**
```
Idle → Charging (3초) → Breathing (2초) → Recovery (1초) → Idle
```

**Phase 상세:**

| Phase | 시간 | 동작 |
|-------|-----|------|
| Charging | 3초 | 드래곤 날개 펴고 포효, 경고 영역 표시, 카메라 미세 진동 |
| Breathing | 2초 | 전방위 브레스, 카메라 강한 쉐이크, 데미지 판정 |
| Recovery | 1초 | 드래곤 피로 상태, 플레이어 공격 찬스 |

**주요 멤버:**
```cpp
float m_fChargeTime = 3.0f;
float m_fBreathTime = 2.0f;
float m_fRecoveryTime = 1.0f;
float m_fDamage = 999.0f;  // 즉사급
bool m_bDamageDealt = false;

// 경고 인디케이터
GameObject* m_pWarningIndicator = nullptr;  // 전방위 원형 경고
```

**DealDamage() 로직:**
```cpp
1. 모든 플레이어 순회
2. 각 플레이어에 대해 IsPlayerBehindCover() 체크
3. 엄폐 중이 아니면 데미지 적용
```

---

## 3. 엄폐물(Cover) 시스템

### 새 파일: CoverComponent.h / .cpp

**역할:** 오브젝트를 엄폐물로 지정

**주요 멤버:**
```cpp
float m_fCoverRadius = 3.0f;   // 엄폐 유효 반경
float m_fCoverHeight = 5.0f;   // 엄폐 높이
bool m_bDestructible = false;  // 파괴 가능 여부
```

### Line of Sight 체크 로직

**IsPlayerBehindCover() 함수:**
```cpp
1. 보스 위치 → 플레이어 위치 방향 벡터 계산
2. 씬 내 모든 CoverComponent 순회
3. 각 엄폐물에 대해:
   - 보스-플레이어 직선과 엄폐물 원기둥의 교차 판정
   - 플레이어가 엄폐물 뒤(보스 반대편)에 있는지 확인
4. 하나라도 가리면 true 반환
```

**간소화된 판정 방식:**
```
보스 → 엄폐물 → 플레이어 순서로 일직선상에 있고,
플레이어-엄폐물 거리 < 엄폐반경 이면 "숨음" 판정
```

---

## 4. 경고 연출 시스템

### 차징 단계 연출

1. **시각적 경고:**
   - 드래곤 주변 거대한 빨간 원형 인디케이터 (범위 = 맵 전체)
   - "안전지대 표시" - 엄폐물 뒤쪽에 초록색 영역 (선택)

2. **드래곤 애니메이션:**
   - "Charge_Breath" 클립 (날개 펴고 입 벌리기)
   - 입에서 화염 파티클 차오름

3. **카메라 연출:**
   - 미세한 진동 시작 (intensity: 0.1)
   - 줌아웃하여 전체 필드 보여주기 (선택)

4. **UI 경고:**
   - 화면 가장자리 빨간색 비네팅
   - "숨어!" 텍스트 표시 (선택)

### 브레스 단계 연출

1. **카메라 쉐이크:**
   - 강한 쉐이크 (intensity: 0.5~0.8)
   - 지속시간: 브레스 시간 + 0.5초

2. **파티클 효과:**
   - 전방위 화염 파티클
   - 바닥 균열/용암 분출 이펙트

3. **화면 효과:**
   - 화면 전체 오렌지/빨간 틴트
   - 히트 디스토션 (열기 왜곡) - 포스트 프로세싱 (선택)

---

## 5. Room/Scene 통합

### BossRoom 클래스 (Room 상속)

**추가 멤버:**
```cpp
std::vector<GameObject*> m_vCoverObjects;  // 엄폐물 목록
EnemyComponent* m_pBoss = nullptr;
int m_nCurrentPhase = 1;  // 보스 페이즈 (1, 2, 3)
```

**Update() 확장:**
```cpp
// 보스 HP 체크 → 페이즈 전환
if (bossHP <= 70% && m_nCurrentPhase == 1) {
    TriggerMegaBreath();
    m_nCurrentPhase = 2;
}
if (bossHP <= 40% && m_nCurrentPhase == 2) {
    TriggerMegaBreath();
    m_nCurrentPhase = 3;
}
```

### 맵 데이터

보스 맵 JSON에 엄폐물 배치 정보 추가:
```json
{
  "covers": [
    { "position": [10, 0, 10], "radius": 3.0 },
    { "position": [-10, 0, 10], "radius": 3.0 },
    { "position": [0, 0, -15], "radius": 4.0 }
  ]
}
```

---

## 6. 새로 생성할 파일

```
gaym/
├── MegaBreathAttackBehavior.h
├── MegaBreathAttackBehavior.cpp
├── CoverComponent.h
├── CoverComponent.cpp
├── BossRoom.h (선택 - Room 상속)
└── BossRoom.cpp
```

---

## 7. 수정할 기존 파일

| 파일 | 수정 내용 |
|------|-----------|
| Camera.h | 쉐이크 멤버/메서드 추가 |
| Camera.cpp | UpdateShake(), StartShake() 구현 |
| EnemyComponent.h | TriggerMegaBreath() 메서드 추가 |
| EnemyComponent.cpp | 페이즈 전환 로직 |
| EnemySpawner.cpp | Dragon 프리셋에 MegaBreath 설정 |
| Scene.cpp | 카메라 쉐이크 연동 |

---

## 8. 구현 순서

1. **카메라 쉐이크 시스템**
   - Camera 클래스 확장
   - 테스트용 키 바인딩 (디버그)

2. **CoverComponent**
   - 기본 구조 및 Line of Sight 체크
   - 테스트용 큐브 엄폐물 배치

3. **MegaBreathAttackBehavior**
   - 상태 머신 기본 구조
   - 차징 → 브레스 → 리커버리 플로우

4. **경고 인디케이터**
   - 전방위 원형 경고 영역
   - 안전지대 표시 (선택)

5. **데미지 판정**
   - IsPlayerBehindCover() 구현
   - 엄폐 실패 시 데미지 적용

6. **연출 폴리싱**
   - 파티클 효과
   - 화면 효과 (틴트/비네팅)
   - 사운드 (선택)

---

## 9. 참고 기존 코드

| 기능 | 파일 | 참고 |
|------|------|------|
| 브레스 공격 | BreathAttackBehavior.cpp | Phase 패턴, 투사체 발사 |
| 보스 인트로 | EnemyComponent.cpp:491 | UpdateBossIntro() 상태머신 |
| 인디케이터 생성 | EnemySpawner.cpp:537 | CreateIndicatorObject() |
| 카메라 업데이트 | Camera.cpp | UpdateViewMatrix() |
| 파티클 시스템 | ParticleSystem.cpp | 화염 이펙트 참고 |

---

## 10. 검증 체크리스트

- [ ] 보스 HP 70% 도달 시 메가 브레스 발동
- [ ] 3초 차징 중 경고 영역 표시
- [ ] 카메라 쉐이크 정상 작동
- [ ] 엄폐물 뒤에서 데미지 회피 확인
- [ ] 노출 시 즉사급 데미지 확인
- [ ] 브레스 후 1초 리커버리 타임
- [ ] HP 40%에서 2차 메가 브레스 발동

---

## 11. 확장 고려사항

- **파괴 가능 엄폐물**: 2차 브레스 때 일부 엄폐물 파괴
- **이동하는 안전지대**: 브레스 중 안전지대가 회전
- **다중 플레이어**: 각 플레이어별 엄폐 판정
- **난이도 조절**: 차징 시간, 엄폐물 개수 변경

---

## 12. 타임라인 예시

```
[0.0초] 보스 HP 70% 도달 - 메가 브레스 트리거
        → 드래곤 포효 애니메이션 시작
        → 경고 인디케이터 표시
        → 카메라 미세 진동 (0.1)

[1.5초] 경고 절반 경과
        → 화면 가장자리 빨간 틴트 시작
        → 드래곤 입에서 화염 파티클 차오름

[3.0초] 차징 완료 - 브레스 발동
        → 카메라 강한 쉐이크 (0.5)
        → 데미지 판정 (엄폐 체크)
        → 전방위 화염 파티클

[5.0초] 브레스 종료 - 리커버리
        → 드래곤 피로 애니메이션
        → 카메라 쉐이크 감쇠

[6.0초] 리커버리 종료 - 일반 전투 재개
```
