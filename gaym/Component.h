#pragma once

class GameObject; // Forward declaration
struct ID3D12GraphicsCommandList; // Forward declaration
struct ID3D12Device; // Forward declaration
class InputSystem; // Forward declaration for InputSystem

class Component
{
public:
    Component(GameObject* pOwner) : m_pOwner(pOwner) {}
    virtual ~Component() = default;

    virtual void Init(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList) {}
    virtual void Update(float deltaTime) {}
    virtual void Render(ID3D12GraphicsCommandList* pCommandList) {}

    GameObject* GetOwner() { return m_pOwner; }
protected:
    GameObject* m_pOwner;
};