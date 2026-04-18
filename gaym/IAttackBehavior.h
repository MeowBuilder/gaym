#pragma once

class EnemyComponent;

class IAttackBehavior
{
public:
    virtual ~IAttackBehavior() = default;

    // Start the attack
    virtual void Execute(EnemyComponent* pEnemy) = 0;

    // Update the attack (called every frame while attacking)
    virtual void Update(float dt, EnemyComponent* pEnemy) = 0;

    // Check if the attack is finished
    virtual bool IsFinished() const = 0;

    // Reset the attack state for reuse
    virtual void Reset() = 0;

    // 이 공격에 맞는 애니메이션 클립 이름. 빈 문자열이면 m_strAttackClip 기본값 사용
    virtual const char* GetAnimClipName() const { return ""; }

    // 공격 시작(windup)부터 실제 데미지 타이밍까지의 시간(초).
    // 인디케이터 fill 진행도 계산에 사용. 0 반환 시 즉시 차오름/기본 1.0s 사용
    virtual float GetTimeToHit() const { return 0.0f; }

    // 지면 AoE 인디케이터(빨간 원)를 보여줄지 여부. 투사체/발사형은 false로 표시 억제
    virtual bool ShouldShowHitZone() const { return true; }
};
