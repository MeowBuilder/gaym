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
};
