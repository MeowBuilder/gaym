#pragma once
#include "Component.h"
#include "Mesh.h"
#include <memory>

class RenderComponent : public Component
{
public:
    RenderComponent(GameObject* pOwner);
    ~RenderComponent();

    virtual void Init(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList) override;
    virtual void Update(float deltaTime) override;
    virtual void Render(ID3D12GraphicsCommandList* pCommandList) override;

    void SetMesh(Mesh* pMesh) { m_pMesh = pMesh; }
    Mesh* GetMesh() { return m_pMesh; }

private:
    Mesh* m_pMesh = nullptr;
};