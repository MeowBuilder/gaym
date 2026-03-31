#include "stdafx.h"
#include "BossPhaseConfig.h"
#include <algorithm>

void BossPhaseConfig::AddPhase(const BossPhaseData& phase)
{
    m_vPhases.push_back(phase);

    // 체력 임계값 기준 내림차순 정렬 (높은 체력 → 낮은 체력)
    std::sort(m_vPhases.begin(), m_vPhases.end(),
        [](const BossPhaseData& a, const BossPhaseData& b) {
            return a.m_fHealthThreshold > b.m_fHealthThreshold;
        });
}

int BossPhaseConfig::GetPhaseIndexForHealth(float fHealthRatio) const
{
    // 체력 비율에 해당하는 페이즈 인덱스 반환
    // 예: 페이즈 0 (100-70%), 페이즈 1 (70-30%), 페이즈 2 (30-0%)
    // fHealthRatio = 0.5 → 페이즈 1 반환

    for (int i = 0; i < static_cast<int>(m_vPhases.size()); ++i)
    {
        // 현재 체력이 이 페이즈의 임계값 이하인지 확인
        // 마지막 페이즈까지 확인
        if (i == static_cast<int>(m_vPhases.size()) - 1)
        {
            return i;  // 마지막 페이즈
        }

        // 다음 페이즈의 임계값보다 높으면 현재 페이즈
        if (fHealthRatio > m_vPhases[i + 1].m_fHealthThreshold)
        {
            return i;
        }
    }

    return 0;  // 기본값: 첫 번째 페이즈
}
