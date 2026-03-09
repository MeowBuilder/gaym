#pragma once
#include "Component.h"
#include "Mesh.h"
#include <memory>

class Shader;

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

    void SetCastsShadow(bool bCasts) { m_bCastsShadow = bCasts; }
    bool CastsShadow() const { return m_bCastsShadow; }

    void SetOwnerShader(Shader* pShader) { m_pOwnerShader = pShader; }

private:
    Mesh* m_pMesh = nullptr;
    bool m_bCastsShadow = false;
    Shader* m_pOwnerShader = nullptr;
};