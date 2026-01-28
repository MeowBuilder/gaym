# 스킬 시스템 설계 문서

## 1. 개요

본 문서는 Rune Code 프로젝트의 스킬 시스템 구현을 위한 기술 설계 문서이다.
스킬 시스템은 캐릭터의 전투 능력을 정의하며, 룬 시스템과 연동되어 다양한 빌드를 가능하게 한다.

---

## 2. 스킬 슬롯 구성

각 캐릭터는 **4개의 스킬 슬롯**을 보유한다.

| 슬롯 | 입력 키 | 일반적 용도 |
|------|--------|-----------|
| Skill 1 | Q | 주력 공격/유틸리티 |
| Skill 2 | E | 보조 공격/이동기 |
| Skill 3 | R | 궁극기 (높은 쿨다운, 강력한 효과) |
| Skill 4 | 우클릭 | 보조 스킬/특수 기능 |

> **조작 체계**: 기본 공격은 좌클릭, 이동은 WASD

---

## 3. 스킬 속성 체계

### 3.1 원소 속성 (Element)

4가지 원소가 존재하며, 각 원소는 고유한 상태이상과 전투 특성을 가진다.

| 원소 | 색상 | 상태이상 | 전투 특성 |
|------|------|---------|----------|
| **물 (Water)** | 파랑 | 둔화, 빙결 | 군중제어, 서포트 |
| **불 (Fire)** | 빨강 | 화상 (DoT) | 광역 딜러, 폭발 |
| **땅 (Earth)** | 갈색 | 기절, 넉백 | 탱커, 보호막 |
| **바람 (Wind)** | 초록 | 밀어내기, 끌어당김 | 원거리 딜러, 기동성 |

### 3.2 발동 방식 키워드 (Activation Type)

스킬의 발동 방식을 정의하는 키워드:

| 키워드 | 설명 | 예시 |
|--------|------|------|
| **즉발 (Instant)** | 키 입력 즉시 발동 | 기본 투사체 |
| **차지 (Charge)** | 홀드 시 위력/범위 증가 | 차징 빔 |
| **캐스팅 (Casting)** | 시전 시간 필요 | 메테오 |
| **설치 (Summon)** | 지속형 오브젝트 생성 | 장판, 포탑 |
| **강화 (Buff)** | 자신/아군에게 버프 | 보호막, 속도 증가 |

---

## 4. 캐릭터별 스킬 구성

> **구현 우선순위**: 불 원소 캐릭터 먼저 구현. 상세 스킬셋은 차후 조정 예정.

### 4.1 불 원소 캐릭터 (광역 딜러) ★ 우선 구현

**역할**: 폭발/연소 기반의 광역 딜러

| 슬롯 | 스킬명 | 설명 |
|------|--------|------|
| Q | 화염탄 | 소형 폭발 투사체, 적중 시 화염 스택 부여 |
| E | 돌진 폭발 | 전방 짧은 돌진 후 폭발, 화염 스택당 추가 데미지 |
| R | 메테오 | 넓은 범위 마법진 → 캐스팅 후 거대한 폭발 |
| 우클릭 | 화염 분출 | 원형 화염 분출, 범위 내 적에게 큰 데미지 + 기절 |

### 4.2 물 원소 캐릭터 (서포터)

**역할**: 군중 제어, 감속 특화의 서포트

| 슬롯 | 스킬명 | 설명 |
|------|--------|------|
| Q | 물창 | 직선 관통 투사체, 적중 시 짧은 둔화 |
| E | 소용돌이 | 원형 설치형, 중심으로 적을 끌어들이며 둔화 |
| R | 해일 | 라인형 대파동, 적중 시 기절 + 받는 데미지 증가 |
| 우클릭 | 물길 | 직선형 물길 생성, 아군 이동속도 증가 + 보호막 |

### 4.3 땅 원소 캐릭터 (탱커)

**역할**: 보호막, 피해 감소 기반의 탱커

| 슬롯 | 스킬명 | 설명 |
|------|--------|------|
| Q | 바위 투척 | 바위 투사체, 적 적중 시 자신에게 보호막 생성 |
| E | 돌진 | 전방 돌진, 적중 적에게 보호막 비례 데미지 |
| R | 대지의 분노 | 주변 도발 + 땅 내리쳐 기절/데미지, 시전 중 피해 감소 |
| 우클릭 | 암석 갑옷 | 바위를 둘러 받는 피해 감소, 주변 적 둔화 |

### 4.4 바람 원소 캐릭터 (원거리 딜러)

**역할**: 빠른 연사 기반의 원거리 딜러

| 슬롯 | 스킬명 | 설명 |
|------|--------|------|
| Q | 부메랑 바람 | 직선 투사체 → 일정 시간 후 귀환 (2회 타격) |
| E | 돌풍 | 전방 바람으로 짧은 넉백 + 이동속도 감소 |
| R | 회오리 | 회오리 발사, 적을 끌어들이며 지속 데미지 |
| 우클릭 | 질풍 | 짧은 시간 이동속도 증가 + 적 투사체 회피 |

---

## 5. 룬 시스템 연동

### 5.1 룬 장착 구조

- 각 스킬에 **3~4개의 룬 슬롯** 존재
- 룬 장착 시 스킬의 속성/발동방식/수치가 변경됨
- 룬 조합에 따라 **VFX가 동적으로 변화**

### 5.2 룬 종류

총 **50종**의 룬 (상세 목록은 `RuneList.md` 참조)

| 분류 | 개수 | 설명 |
|------|------|------|
| 원소 강화 룬 | 20종 | 원소 속성 부여/강화 (물/불/땅/바람 각 5종) |
| 발동 방식 변경 룬 | 20종 | 스킬 동작 구조 변경 (즉발/차지/캐스팅/설치/강화 각 4종) |
| 레전더리 룬 | 10종 | 스킬 근본적 변경 (보스 스테이지 전용) |

### 5.3 룬 등급 시스템

각 룬은 **고정 등급**을 가지며, 해당 등급에서만 드랍됨:

| 등급 | 드랍 확률 | 기본 효과 수준 |
|------|----------|---------------|
| 노멀 | 50% | 기본 |
| 레어 | 30% | 중간 |
| 에픽 | 15% | 강력 |
| 유니크 | 4% | 매우 강력 + 특수효과 |
| 레전더리 | 1% | 스킬 변형 |

### 5.4 중복 획득 시 강화

같은 룬을 중복 획득하면 **효과가 영구 강화**됨:

| 등급 | 중복 획득당 강화량 |
|------|-------------------|
| 노멀 | +10% |
| 레어 | +8% |
| 에픽 | +6% |
| 유니크 | +5% |
| 레전더리 | +3% |

예시: 노멀 "불꽃" (불 속성 피해 +15%)
- 1회 획득: +15%
- 2회 획득: +16.5% (+10%)
- 3회 획득: +18.15% (+10%)
- ...

### 5.5 룬 조합 규칙

**원소 조합**: 서로 다른 원소 룬을 함께 장착 가능. 두 원소의 효과가 동시에 적용됨.
- 예: 불 룬 + 물 룬 = 화상 + 둔화 동시 적용
- 단, 해당 원소 피해가 없는 스킬은 해당 원소 강화 효과를 받지 않음

**원소 조합 시스템 (차후 확장 예정)**:
- 물+불 → 증기, 불+바람 → 화염 회오리 등

### 5.6 상호배타 규칙

**즉발(Instant) ↔ 차지(Charge)**: 서로 상충되는 키워드로 동시 장착 불가

그 외 조합은 모두 허용:
- 원소 키워드 간: 제한 없음 (불+물, 땅+바람 등 가능)
- 발동 방식 간: 즉발↔차지 외에는 제한 없음

---

## 6. 스킬 데이터 구조

### 6.1 SkillData 구조체

```cpp
struct SkillData
{
    // 기본 정보
    std::string skillId;           // 고유 ID
    std::string skillName;         // 스킬명
    SkillSlot slot;                // Q, E, R, RightClick

    // 속성
    ElementType baseElement;       // 기본 원소
    ActivationType activationType; // 발동 방식
    TargetType targetType;         // Single, Area, Self, Ally

    // 수치
    float baseDamage;              // 기본 데미지
    float cooldown;                // 쿨다운 (초)
    float range;                   // 사거리
    float radius;                  // 범위 (Area 타입)
    float castTime;                // 시전 시간
    float duration;                // 지속 시간 (설치/버프)

    // 룬 슬롯
    int maxRuneSlots;              // 최대 룬 슬롯 수 (3~4)
    std::vector<RuneSlot> runeSlots;

    // 상태이상
    StatusEffect statusEffect;     // 부여할 상태이상
    float statusDuration;          // 상태이상 지속시간
    float statusChance;            // 적용 확률

    // VFX 참조
    std::string vfxPrefabId;       // 기본 VFX
    std::string hitVfxId;          // 적중 VFX
};
```

### 6.2 열거형 정의

```cpp
enum class SkillSlot
{
    Q,
    E,
    R,
    RightClick
};

enum class ElementType
{
    None,
    Water,
    Fire,
    Earth,
    Wind
};

enum class ActivationType
{
    Instant,    // 즉발
    Charge,     // 차지
    Casting,    // 캐스팅
    Summon,     // 설치
    Buff        // 강화
};

enum class TargetType
{
    Single,     // 단일 대상
    Area,       // 범위
    Line,       // 직선
    Self,       // 자신
    Ally        // 아군
};
```

---

## 7. 스킬 컴포넌트 설계

### 7.1 클래스 구조

```
SkillComponent (Component 상속)
├── SkillSlotManager          // 4개 스킬 슬롯 관리
├── SkillExecutor             // 스킬 실행 로직
├── CooldownManager           // 쿨다운 관리
└── RuneModifierApplier       // 룬 효과 적용
```

### 7.2 SkillComponent 인터페이스

```cpp
class SkillComponent : public Component
{
public:
    // 스킬 등록/해제
    void SetSkill(SkillSlot slot, const SkillData& data);
    void RemoveSkill(SkillSlot slot);

    // 스킬 사용
    bool CanUseSkill(SkillSlot slot) const;
    void UseSkill(SkillSlot slot, const XMFLOAT3& targetPos);
    void UseSkill(SkillSlot slot, GameObject* target);

    // 차지 스킬
    void BeginCharge(SkillSlot slot);
    void ReleaseCharge(SkillSlot slot);
    float GetChargeProgress(SkillSlot slot) const;

    // 쿨다운
    float GetCooldownRemaining(SkillSlot slot) const;
    float GetCooldownProgress(SkillSlot slot) const;  // 0~1

    // 룬 연동
    void AttachRune(SkillSlot slot, int runeSlotIndex, const RuneData& rune);
    void DetachRune(SkillSlot slot, int runeSlotIndex);
    SkillData GetModifiedSkillData(SkillSlot slot) const;  // 룬 적용된 최종 데이터

private:
    std::array<SkillSlotData, 4> m_skills;
    std::array<float, 4> m_cooldowns;
    std::array<float, 4> m_chargeProgress;
};
```

---

## 8. VFX 연동 시스템

### 8.1 동적 VFX 변화

룬 조합에 따라 VFX가 동적으로 변경됨:

| 원소 조합 | VFX 변화 |
|----------|----------|
| 물 + 불 | 증기 파티클 추가 |
| 불 + 바람 | 화염 회오리 |
| 땅 + 물 | 진흙/늪 효과 |
| 바람 + 땅 | 모래 폭풍 |

### 8.2 VFX 레이어 구조

```
스킬 VFX
├── Base Layer      // 기본 스킬 형태
├── Element Layer   // 원소 속성 파티클
├── Rune Layer      // 룬 효과 추가 파티클
└── Impact Layer    // 적중/폭발 효과
```

---

## 9. 구현 우선순위

### Phase 1: 기본 구조
1. SkillData 구조체 정의
2. SkillComponent 기본 틀
3. 쿨다운 시스템
4. 단일 스킬 발동 테스트

### Phase 2: 스킬 타입별 구현
1. 투사체 스킬 (Projectile)
2. 범위 스킬 (Area)
3. 버프/디버프 스킬
4. 차지 스킬

### Phase 3: 룬 시스템 연동
1. 룬 데이터 구조
2. 룬 슬롯 시스템
3. 스킬 수치 변경 로직
4. VFX 동적 변경

### Phase 4: 캐릭터별 스킬셋
1. 불 원소 캐릭터 4스킬 구현 (우선)
2. 나머지 원소 캐릭터 순차 구현
3. 밸런스 조정
4. VFX 완성

---

## 10. 스테이지 구조

### 10.1 스테이지 구성

4개의 원소별 스테이지 + 보스 스테이지:

| 스테이지 | 원소 | 특징 |
|---------|------|------|
| Stage 1 | 물 | 둔화/빙결 적 다수 |
| Stage 2 | 불 | 화상/폭발 적 다수 |
| Stage 3 | 땅 | 고체력/보호막 적 다수 |
| Stage 4 | 바람 | 기동성/원거리 적 다수 |
| Boss Stage | 혼합 | 최종 보스 |

### 10.2 진행 구조

```
┌─────────────────────────────────────────────────────────────┐
│                      게임 진행 플로우                         │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│   [Stage 1] → [Stage 2] → [Stage 3] → [Stage 4]            │
│        ↑                                    │               │
│        └────────── 루프 (선택) ─────────────┘               │
│                                             │               │
│                                             ↓               │
│                                     [Boss Stage]            │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

- 4스테이지 클리어 후 **보스 스테이지 진입** 또는 **루프 반복** 선택
- 루프를 돌수록 적 강화, 보상 증가
- 예: 1→2→3→4→1→2→3→4→Boss (2루프 후 보스)

### 10.3 방(Room) 구성

각 스테이지는 **6~8개의 방**으로 구성:

| 방 타입 | 개수 | 설명 |
|--------|------|------|
| 일반 전투 | 4~5 | 일반 몬스터 처치 |
| 엘리트 전투 | 1~2 | 강화 몬스터 (보상 증가) |
| 보스 방 | 1 | 스테이지 보스 |
| (선택) 휴식/상점 | 0~1 | 회복/구매 |

---

## 11. 룬 드랍 시스템

### 11.1 선택형 드랍

방 클리어 시 **3개의 룬 중 1개 선택**:

```
┌─────────────────────────────────────────┐
│            룬을 선택하세요               │
├───────────┬───────────┬─────────────────┤
│  [불꽃]   │  [신속]   │    [집중]       │
│   노멀    │   레어    │     노멀        │
│  불 원소  │  즉발계열 │    차지계열     │
└───────────┴───────────┴─────────────────┘
      ↓           ↓            ↓
   선택 시      선택 시       선택 시
   획득         획득          획득

   * 선택하지 않은 룬은 소멸
```

### 11.2 중복 선택 시 강화

이미 보유한 룬 선택 시 **효과 영구 강화**:

```cpp
// 중복 룬 획득 처리
void OnRuneAcquired(const RuneData& newRune)
{
    RuneData* existing = FindOwnedRune(newRune.id);
    if (existing)
    {
        // 중복 → 효과 강화 (등급별 강화량)
        float bonus = GetStackBonus(existing->grade);  // 노멀 10%, 레어 8% 등
        existing->stackCount++;
        existing->effectMultiplier *= (1.0f + bonus);
    }
    else
    {
        // 신규 획득
        AddToInventory(newRune);
    }
}
```

### 11.3 드랍 풀 구성

**모든 스테이지 동일한 고정 드랍 테이블** (루프/스테이지 무관):

| 등급 | 드랍 확률 |
|------|----------|
| 노멀 | 50% |
| 레어 | 30% |
| 에픽 | 15% |
| 유니크 | 4% |
| 레전더리 | 1% |

- 각 룬은 고유 등급에서만 드랍
- 드랍 풀 축소 없음 (중복 획득도 강화로 가치 있음)
- 50종 룬이 등급별로 분배됨

### 11.4 룬 데이터 구조

```cpp
enum class RuneGrade
{
    Normal,
    Rare,
    Epic,
    Unique,
    Legendary  // 보스 전용, 등급 업 불가
};

struct RuneData
{
    std::string runeId;        // "F01", "I02", "L05" 등
    std::string runeName;      // "불꽃", "신속", "흡수"
    RuneCategory category;     // Element, Activation, Legendary
    RuneGrade grade;           // 고정 등급 (변경 안됨)

    // 중복 획득 강화
    int stackCount = 1;            // 획득 횟수
    float effectMultiplier = 1.0f; // 효과 배율

    // 원소 룬 전용
    ElementType element;       // Water, Fire, Earth, Wind

    // 발동 방식 룬 전용
    ActivationType activation; // Instant, Charge, Casting, Summon, Buff

    // 효과 수치
    float baseValue;           // 기본 수치
    float GetFinalValue() const { return baseValue * effectMultiplier; }

    // 등급별 중복 강화량
    static float GetStackBonus(RuneGrade g)
    {
        switch (g)
        {
            case RuneGrade::Normal:    return 0.10f;  // +10%
            case RuneGrade::Rare:      return 0.08f;  // +8%
            case RuneGrade::Epic:      return 0.06f;  // +6%
            case RuneGrade::Unique:    return 0.05f;  // +5%
            case RuneGrade::Legendary: return 0.03f;  // +3%
            default: return 0.0f;
        }
    }
};
```

---

## 12. 참고사항

- 스킬 데이터는 추후 JSON/Binary 파일로 외부화 예정
- 네트워크 동기화를 위해 스킬 ID 기반 통신 구조 필요
- 쿨다운/차지 상태는 UI와 실시간 연동 필요
- 룬 상세 목록은 `RuneList.md` 참조
