#pragma once
#include "stdafx.h"
#include <vector>
#include <memory>
#include "Mesh.h"

struct ID3D12GraphicsCommandList; // 전방 선언
struct ID3D12Device; // 전방 선언
class Component;                   // 전방 선언
class TransformComponent;          // 전방 선언
class InputSystem;                 // 전방 선언 for InputSystem

struct MATERIAL
{
    XMFLOAT4 m_cAmbient;
    XMFLOAT4 m_cDiffuse;
    XMFLOAT4 m_cSpecular; // a = power
    XMFLOAT4 m_cEmissive;
};

struct ObjectConstants
{
	XMFLOAT4X4 m_xmf4x4World;
	UINT m_nMaterialIndex = 0;
    float pad1; // 4 bytes
    float pad2; // 4 bytes
    float pad3; // 4 bytes
	MATERIAL mMaterial;
};


class GameObject
{
public:
	GameObject();
	~GameObject();

	void Init(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList);
	void Update(float deltaTime);
	void Render(ID3D12GraphicsCommandList* pCommandList);

	template<typename T> T* GetComponent();
	template<typename T, typename... TArgs> T* AddComponent(TArgs&&... args);

	TransformComponent* GetTransform() { return m_pTransform; }

	void CreateConstantBuffer(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList, UINT nBufferSize, D3D12_CPU_DESCRIPTOR_HANDLE d3dCbvCPUDescriptorHandle);
	D3D12_GPU_DESCRIPTOR_HANDLE GetGpuDescriptorHandle() const { return m_cbvGPUDescriptorHandle; }
	void SetGpuDescriptorHandle(D3D12_GPU_DESCRIPTOR_HANDLE handle) { m_cbvGPUDescriptorHandle = handle; }
	void SetMaterialIndex(UINT index) { m_nMaterialIndex = index; }
	UINT GetMaterialIndex() const { return m_nMaterialIndex; }

	void SetMesh(Mesh* pMesh);
	Mesh* GetMesh() { return m_pMesh; }
	void SetChild(GameObject* pChild);
	void SetTransform(const XMFLOAT4X4& transform);

	void SetMaterial(const MATERIAL& material); // New method for setting material

public:
	char			m_pstrFrameName[64];

	GameObject* m_pParent = nullptr;
	GameObject* m_pChild = nullptr;
	GameObject* m_pSibling = nullptr;

private:
	std::vector<std::unique_ptr<Component>> m_vComponents;
	TransformComponent* m_pTransform = nullptr;

	ComPtr<ID3D12Resource> m_pd3dcbGameObject = nullptr;
	ObjectConstants* m_pcbMappedGameObject = nullptr;
	D3D12_GPU_DESCRIPTOR_HANDLE m_cbvGPUDescriptorHandle;
	UINT m_nMaterialIndex = 0;
	MATERIAL m_Material; // New material member
	Mesh* m_pMesh = nullptr; // Re-added m_pMesh member
};

#include "GameObject.inl"
