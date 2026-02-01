#include "stdafx.h"
#include "InteractableComponent.h"
#include "GameObject.h"
#include "TransformComponent.h"

InteractableComponent::InteractableComponent(GameObject* pOwner)
    : Component(pOwner)
{
}

void InteractableComponent::Update(float deltaTime)
{
    // Base implementation does nothing
    // Derived classes can override for animations, etc.
}

bool InteractableComponent::IsPlayerInRange(GameObject* pPlayer) const
{
    if (!m_bIsActive || !m_pOwner || !pPlayer)
        return false;

    TransformComponent* pMyTransform = m_pOwner->GetTransform();
    TransformComponent* pPlayerTransform = pPlayer->GetTransform();

    if (!pMyTransform || !pPlayerTransform)
        return false;

    float distance = MathUtils::Distance3D(
        pMyTransform->GetPosition(),
        pPlayerTransform->GetPosition()
    );

    return distance <= m_fInteractionDistance;
}

void InteractableComponent::Interact()
{
    if (!m_bIsActive)
        return;

    OutputDebugString(L"[Interactable] Interaction triggered\n");

    if (m_OnInteract)
    {
        m_OnInteract(this);
    }
}

void InteractableComponent::Hide()
{
    m_bIsActive = false;

    if (m_pOwner)
    {
        TransformComponent* pTransform = m_pOwner->GetTransform();
        if (pTransform)
        {
            XMFLOAT3 pos = pTransform->GetPosition();
            pTransform->SetPosition(pos.x, -1000.0f, pos.z);
        }
    }

    OutputDebugString(L"[Interactable] Hidden\n");
}
