#pragma once

#include "Component.h"
#include "MathUtils.h"
#include <functional>
#include <string>

class InteractableComponent : public Component
{
public:
    InteractableComponent(GameObject* pOwner);
    virtual ~InteractableComponent() = default;

    virtual void Update(float deltaTime) override;

    // Configuration
    void SetInteractionDistance(float distance) { m_fInteractionDistance = distance; }
    float GetInteractionDistance() const { return m_fInteractionDistance; }

    void SetPromptText(const std::wstring& text) { m_sPromptText = text; }
    const std::wstring& GetPromptText() const { return m_sPromptText; }

    // Activation state
    bool IsActive() const { return m_bIsActive; }
    void SetActive(bool active) { m_bIsActive = active; }

    // Interaction callback - called when player interacts with this object
    void SetOnInteract(std::function<void(InteractableComponent*)> callback) { m_OnInteract = callback; }

    // Check if player is within interaction range
    bool IsPlayerInRange(GameObject* pPlayer) const;

    // Trigger the interaction (called externally, e.g., on F key press)
    void Interact();

    // Hide this object (moves to y=-1000)
    void Hide();

private:
    bool m_bIsActive = true;
    float m_fInteractionDistance = 5.0f;
    std::wstring m_sPromptText = L"[F] Interact";
    std::function<void(InteractableComponent*)> m_OnInteract;
};
