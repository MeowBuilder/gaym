#include "stdafx.h"
#include "DropItemComponent.h"
#include "GameObject.h"
#include "TransformComponent.h"
#include <random>

DropItemComponent::DropItemComponent(GameObject* pOwner)
    : Component(pOwner)
{
    GenerateRandomRunes();
}

DropItemComponent::~DropItemComponent()
{
}

void DropItemComponent::Update(float deltaTime)
{
    if (!m_bIsActive) return;

    // Floating bob animation
    m_fBobTime += deltaTime * m_fBobSpeed;
    m_fBobOffset = sinf(m_fBobTime) * m_fBobAmplitude;

    // Apply bob to transform
    if (m_pOwner)
    {
        TransformComponent* pTransform = m_pOwner->GetTransform();
        if (pTransform)
        {
            XMFLOAT3 pos = pTransform->GetPosition();
            // Store base Y on first update
            static float baseY = pos.y;
            pTransform->SetPosition(pos.x, baseY + m_fBobOffset, pos.z);
        }
    }
}

void DropItemComponent::GenerateRandomRunes()
{
    // Random number generator
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, static_cast<int>(ActivationType::Count) - 1);

    // Generate 3 unique runes
    std::vector<ActivationType> available;
    for (int i = 0; i < static_cast<int>(ActivationType::Count); ++i)
    {
        available.push_back(static_cast<ActivationType>(i));
    }

    // Shuffle and pick first 3
    std::shuffle(available.begin(), available.end(), gen);

    for (int i = 0; i < 3; ++i)
    {
        m_RuneOptions[i] = available[i];
    }

    // Debug output
    const wchar_t* typeNames[] = { L"Instant", L"Charge", L"Channel", L"Place", L"Enhance" };
    wchar_t buffer[256];
    swprintf_s(buffer, L"[Drop] Generated runes: [1] %s, [2] %s, [3] %s\n",
        typeNames[static_cast<int>(m_RuneOptions[0])],
        typeNames[static_cast<int>(m_RuneOptions[1])],
        typeNames[static_cast<int>(m_RuneOptions[2])]);
    OutputDebugString(buffer);
}

ActivationType DropItemComponent::GetRuneOption(int index) const
{
    if (index < 0 || index >= 3)
        return ActivationType::Instant;
    return m_RuneOptions[index];
}
