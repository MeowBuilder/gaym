#pragma once

namespace ThreatConstants
{
    // 데미지 위협도
    constexpr float DAMAGE_THREAT_MULTIPLIER = 1.0f;     // 데미지 1당 위협도 1
    constexpr float SKILL_BASE_THREAT = 5.0f;            // 스킬 사용 기본 위협도

    // 힐 위협도
    constexpr float HEAL_THREAT_MULTIPLIER = 0.5f;       // 힐량의 50%
    constexpr float OVERHEAL_THREAT_MULTIPLIER = 0.25f;  // 오버힐은 25%

    // 거리 기반 위협도 변화
    constexpr float THREAT_DECAY_DISTANCE = 30.0f;       // 이 거리 이상이면 감소
    constexpr float THREAT_DECAY_RATE = 5.0f;            // 초당 감소량
    constexpr float THREAT_GAIN_DISTANCE = 10.0f;        // 이 거리 이하면 증가
    constexpr float THREAT_GAIN_RATE = 2.0f;             // 초당 증가량

    // 타겟 재평가
    constexpr float TARGET_REEVALUATION_INTERVAL = 0.5f; // 0.5초마다 타겟 재평가
    constexpr float CURRENT_TARGET_BONUS = 1.1f;         // 현재 타겟은 10% 보너스 (빈번한 타겟 전환 방지)

    // 초기 위협도
    constexpr float INITIAL_THREAT = 10.0f;              // 플레이어 등록 시 초기 위협도
}
