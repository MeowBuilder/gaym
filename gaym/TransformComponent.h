#pragma once
#include "Component.h"

class TransformComponent : public Component
{
public:
    TransformComponent(GameObject* pOwner);

    virtual void Update(float deltaTime) override;

    const XMFLOAT4X4& GetWorldMatrix() const { return m_matWorld; }

private:
    XMFLOAT4X4 m_matWorld;
    XMFLOAT3 m_position;
    XMFLOAT3 m_rotation; // Euler angles
    XMFLOAT3 m_scale;
};