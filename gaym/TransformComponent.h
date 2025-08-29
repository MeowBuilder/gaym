#pragma once
#include "Component.h"

class TransformComponent : public Component
{
public:
    TransformComponent(GameObject* pOwner);

    virtual void Update(float deltaTime) override;

    const XMFLOAT4X4& GetWorldMatrix() const { return m_matWorld; }

    // Position
    void SetPosition(const XMFLOAT3& position) { m_position = position; }
    void SetPosition(float x, float y, float z) { m_position = XMFLOAT3(x, y, z); }
    const XMFLOAT3& GetPosition() const { return m_position; }

    // Rotation
    void SetRotation(const XMFLOAT3& rotation) { m_rotation = rotation; }
    void SetRotation(float x, float y, float z) { m_rotation = XMFLOAT3(x, y, z); }
    const XMFLOAT3& GetRotation() const { return m_rotation; }

    // Scale
    void SetScale(const XMFLOAT3& scale) { m_scale = scale; }
    void SetScale(float x, float y, float z) { m_scale = XMFLOAT3(x, y, z); }
    const XMFLOAT3& GetScale() const { return m_scale; }

private:
    XMFLOAT4X4 m_matWorld;
    XMFLOAT3 m_position;
    XMFLOAT3 m_rotation; // Euler angles
    XMFLOAT3 m_scale;
};