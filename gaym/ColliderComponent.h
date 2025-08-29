#pragma once
#include "Component.h"
#include <DirectXCollision.h>

class TransformComponent; // 전방 선언

class ColliderComponent : public Component
{
public:
    ColliderComponent(GameObject* pOwner);

    virtual void Init(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList) override;
    virtual void Update(float deltaTime) override;

    bool Intersects(const ColliderComponent& other) const;
    const DirectX::BoundingOrientedBox& GetBoundingBox() const { return m_boundingBox; }

private:
    DirectX::BoundingBox m_initialBox;
    DirectX::BoundingOrientedBox m_boundingBox;
    TransformComponent* m_pTransform = nullptr;
};