# Dragon Flying Animation Issue

## 증상
- 지상 애니메이션 (Idle01, Walk, Flame Attack): 정상 작동
- 비행 애니메이션 (Fly Float, Fly Forward, Fly Flame Attack): 메쉬 깨짐 (정점이 위로 스파이크처럼 늘어남)

## 사용 가능한 애니메이션 클립
```
- Idle01, Idle02
- Walk, Run
- Basic Attack, Claw Attack, Flame Attack
- Fly Forward, Fly Float, Fly Glide, Fly Flame Attack
- Take Off, Land
- Scream, Defend, Get Hit, Sleep, Die
```

## 추측되는 원인

### 1. 본 이름 불일치
- 비행 애니메이션의 본 이름이 메쉬 계층 구조의 본 이름과 다를 수 있음
- AnimationComponent는 이름으로 본을 매칭하므로, 불일치 시 해당 본이 애니메이션되지 않음
- 애니메이션되지 않은 본에 가중치가 있는 정점들이 원점이나 기본 위치로 이동 → 스파이크 현상

### 2. 비행 애니메이션에서 누락된 본
- 지상 애니메이션에는 있지만 비행 애니메이션에는 키프레임 트랙이 없는 본이 있을 수 있음
- 해당 본의 트랜스폼이 초기화되지 않아 문제 발생

### 3. 루트 본 처리 문제
- 비행 애니메이션은 루트 본이 수직으로 이동하는 것을 가정할 수 있음
- 현재 시스템이 이를 적절히 처리하지 않을 수 있음

### 4. 본 계층 구조 차이
- 비행 애니메이션이 다른 본 계층 구조를 가정하고 익스포트되었을 수 있음

### 5. 스케일/회전 값 이상
- 비행 애니메이션 키프레임에 극단적인 스케일/회전 값이 있을 수 있음

## 디버깅 방법

### 1. 본 매칭 로그 추가
`AnimationComponent::Update()`에서 매칭되지 않는 본 이름 출력:
```cpp
for (const auto& track : m_pCurrentClip->m_vBoneTracks)
{
    auto it = m_mapBoneTransforms.find(track.m_strBoneName);
    if (it == m_mapBoneTransforms.end())
    {
        OutputDebugStringA(("Bone not found: " + track.m_strBoneName + "\n").c_str());
    }
}
```

### 2. 메쉬와 애니메이션의 본 이름 비교
- 메쉬 로드 시 본 이름 목록 출력
- 애니메이션 로드 시 트랙 본 이름 목록 출력
- 두 목록 비교하여 불일치 확인

### 3. 비행 애니메이션 첫 프레임만 적용 테스트
- 첫 프레임만 적용해서 정적 포즈가 정상인지 확인
- 정상이면 보간 문제, 비정상이면 키프레임 데이터 문제

## 현재 임시 해결책
- 비행 모드 활성화 (높이 8에서 부유)
- 지상 애니메이션 사용 (Idle01, Walk, Flame Attack)
- 드래곤이 날면서 걷는 모션 재생

## 관련 파일
- `EnemySpawner.cpp`: Dragon 프리셋 설정 (라인 154-185)
- `AnimationComponent.cpp`: 애니메이션 업데이트 및 본 캐시
- `Animation.cpp`: 애니메이션 로드
- `MeshLoader.cpp`: 메쉬 및 본 계층 로드

## 다음 작업
1. 위 디버깅 방법으로 본 매칭 상태 확인
2. 불일치 본 이름 수정 또는 매핑 테이블 추가
3. 비행 애니메이션 정상 작동 확인
