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

    void SetMesh(std::shared_ptr<Mesh> pMesh) { m_pMesh = pMesh; }
    std::shared_ptr<Mesh> GetMesh() { return m_pMesh; }

private:
    std::shared_ptr<Mesh> m_pMesh;

    ComPtr<ID3D12Resource> m_pConstantBuffer;
    void* m_pConstantBufferWO = nullptr;
};
