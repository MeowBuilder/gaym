#include "stdafx.h"
#include "SkillComponent.h"
#include "ISkillBehavior.h"
#include "SkillData.h"
#include "InputSystem.h"
#include "Camera.h"
#include "GameObject.h"
#include "TransformComponent.h"

SkillComponent::SkillComponent(GameObject* pOwner)
    : Component(pOwner)
{
    // Initialize all cooldowns to 0 (ready)
    m_CooldownTimers.fill(0.0f);

    // Initialize all skill states to Ready
    m_SkillStates.fill(SkillState::Ready);
}

SkillComponent::~SkillComponent()
{
}

void SkillComponent::Update(float deltaTime)
{
    // Update cooldown timers
    for (size_t i = 0; i < static_cast<size_t>(SkillSlot::Count); ++i)
    {
        if (m_CooldownTimers[i] > 0.0f)
        {
            m_CooldownTimers[i] -= deltaTime;
            if (m_CooldownTimers[i] <= 0.0f)
            {
                m_CooldownTimers[i] = 0.0f;
                if (m_SkillStates[i] == SkillState::Cooldown)
                {
                    m_SkillStates[i] = SkillState::Ready;
                }
            }
        }
    }

    // Update active skill if any
    if (m_ActiveSkillSlot != SkillSlot::Count)
    {
        size_t slotIndex = static_cast<size_t>(m_ActiveSkillSlot);
        if (m_Skills[slotIndex])
        {
            m_Skills[slotIndex]->Update(deltaTime);

            // Check if skill finished
            if (m_Skills[slotIndex]->IsFinished())
            {
                // Start cooldown
                float cooldown = m_Skills[slotIndex]->GetSkillData().cooldown;
                m_CooldownTimers[slotIndex] = cooldown;
                m_SkillStates[slotIndex] = SkillState::Cooldown;
                m_Skills[slotIndex]->Reset();
                m_ActiveSkillSlot = SkillSlot::Count;
            }
        }
    }
}

void SkillComponent::ProcessSkillInput(InputSystem* pInputSystem, CCamera* pCamera)
{
    if (!pInputSystem || !pCamera) return;

    // Don't process new skill input if a skill is currently active
    if (m_ActiveSkillSlot != SkillSlot::Count) return;

    // Calculate target position once for all skills
    DirectX::XMFLOAT3 targetPos = CalculateTargetPosition(pInputSystem, pCamera);

    // Check each skill slot for input
    for (size_t i = 0; i < static_cast<size_t>(SkillSlot::Count); ++i)
    {
        SkillSlot slot = static_cast<SkillSlot>(i);
        if (IsSkillKeyPressed(slot, pInputSystem))
        {
            TryUseSkill(slot, targetPos);
            break;  // Only use one skill per frame
        }
    }
}

void SkillComponent::EquipSkill(SkillSlot slot, std::unique_ptr<ISkillBehavior> pBehavior)
{
    size_t index = static_cast<size_t>(slot);
    if (index < m_Skills.size())
    {
        m_Skills[index] = std::move(pBehavior);
        m_CooldownTimers[index] = 0.0f;
        m_SkillStates[index] = SkillState::Ready;
    }
}

void SkillComponent::UnequipSkill(SkillSlot slot)
{
    size_t index = static_cast<size_t>(slot);
    if (index < m_Skills.size())
    {
        m_Skills[index].reset();
        m_CooldownTimers[index] = 0.0f;
        m_SkillStates[index] = SkillState::Ready;
    }
}

ISkillBehavior* SkillComponent::GetSkill(SkillSlot slot) const
{
    size_t index = static_cast<size_t>(slot);
    if (index < m_Skills.size())
    {
        return m_Skills[index].get();
    }
    return nullptr;
}

bool SkillComponent::IsSkillReady(SkillSlot slot) const
{
    size_t index = static_cast<size_t>(slot);
    if (index >= m_Skills.size() || !m_Skills[index])
    {
        return false;
    }
    return m_SkillStates[index] == SkillState::Ready;
}

float SkillComponent::GetCooldownRemaining(SkillSlot slot) const
{
    size_t index = static_cast<size_t>(slot);
    if (index < m_CooldownTimers.size())
    {
        return m_CooldownTimers[index];
    }
    return 0.0f;
}

float SkillComponent::GetCooldownProgress(SkillSlot slot) const
{
    size_t index = static_cast<size_t>(slot);
    if (index >= m_Skills.size() || !m_Skills[index])
    {
        return 1.0f;
    }

    float cooldown = m_Skills[index]->GetSkillData().cooldown;
    if (cooldown <= 0.0f)
    {
        return 1.0f;
    }

    float remaining = m_CooldownTimers[index];
    return 1.0f - (remaining / cooldown);
}

bool SkillComponent::TryUseSkill(SkillSlot slot, const DirectX::XMFLOAT3& targetPosition)
{
    size_t index = static_cast<size_t>(slot);

    // Check if skill exists and is ready
    if (index >= m_Skills.size() || !m_Skills[index])
    {
        return false;
    }

    if (m_SkillStates[index] != SkillState::Ready)
    {
        return false;
    }

    // Execute the skill
    m_Skills[index]->Execute(m_pOwner, targetPosition);
    m_SkillStates[index] = SkillState::Casting;
    m_ActiveSkillSlot = slot;

    return true;
}

DirectX::XMFLOAT3 SkillComponent::CalculateTargetPosition(InputSystem* pInputSystem, CCamera* pCamera) const
{
    using namespace DirectX;

    // Get mouse position in screen space
    XMFLOAT2 mousePos = pInputSystem->GetMousePosition();

    // Convert to Normalized Device Coordinates (NDC)
    float ndcX = (2.0f * mousePos.x / kWindowWidth) - 1.0f;
    float ndcY = 1.0f - (2.0f * mousePos.y / kWindowHeight);

    // Unproject from NDC to World Space to form a ray
    XMMATRIX viewMatrix = XMLoadFloat4x4(&pCamera->GetViewMatrix());
    XMMATRIX projMatrix = XMLoadFloat4x4(&pCamera->GetProjectionMatrix());
    XMMATRIX viewProjMatrix = viewMatrix * projMatrix;
    XMMATRIX invViewProjMatrix = XMMatrixInverse(nullptr, viewProjMatrix);

    XMVECTOR rayOrigin = XMVector3TransformCoord(XMVectorSet(ndcX, ndcY, 0.0f, 1.0f), invViewProjMatrix);
    XMVECTOR rayEnd = XMVector3TransformCoord(XMVectorSet(ndcX, ndcY, 1.0f, 1.0f), invViewProjMatrix);
    XMVECTOR rayDir = XMVector3Normalize(rayEnd - rayOrigin);

    // Define the ground plane (Y=0)
    XMVECTOR groundPlane = XMPlaneFromPointNormal(XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f), XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));

    // Find intersection of the ray and the ground plane
    XMVECTOR intersectionPoint = XMPlaneIntersectLine(groundPlane, rayOrigin, rayOrigin + rayDir * 1000.0f);

    XMFLOAT3 result;
    XMStoreFloat3(&result, intersectionPoint);
    return result;
}

bool SkillComponent::IsSkillKeyPressed(SkillSlot slot, InputSystem* pInputSystem) const
{
    switch (slot)
    {
    case SkillSlot::Q:
        return pInputSystem->IsKeyDown('Q');
    case SkillSlot::E:
        return pInputSystem->IsKeyDown('E');
    case SkillSlot::R:
        return pInputSystem->IsKeyDown('R');
    case SkillSlot::RightClick:
        return pInputSystem->IsMouseButtonDown(1);  // Right mouse button
    default:
        return false;
    }
}
