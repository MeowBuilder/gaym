#pragma once
#include "Component.h"

class TransformComponent;

class RotatorComponent : public Component
{
public:
    RotatorComponent(GameObject* pOwner);

    virtual void Init(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList) override;
    virtual void Update(float deltaTime) override;

    void SetRotationSpeed(const XMFLOAT3& speed) { m_rotationSpeed = speed; }
    const XMFLOAT3& GetRotationSpeed() const { return m_rotationSpeed; }

private:
    XMFLOAT3 m_rotationSpeed;
    TransformComponent* m_pTransform = nullptr;
};
