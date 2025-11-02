#pragma once

#include <vector>
#include <memory>
#include "GameObject.h"
#include "Shader.h"
#include "Mesh.h"
#include "DescriptorHeap.h"
#include "InputSystem.h"
#include "Camera.h" // Added Camera.h include

struct ID3D12Device;
struct ID3D12GraphicsCommandList;

struct PassConstants
{
    XMFLOAT4X4 m_xmf4x4ViewProj;
};

class Scene
{
public:
    Scene();
    ~Scene();

    void Init(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList);
    void LoadSceneFromFile(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList, const char* pstrFileName);
    void Update(float deltaTime, InputSystem* pInputSystem);
    void Render(ID3D12GraphicsCommandList* pCommandList);

    CCamera* GetCamera() const { return m_pCamera.get(); } // Added getter for CCamera

    GameObject* CreateGameObject(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList);

private:

    std::vector<std::unique_ptr<GameObject>> m_vGameObjects;
    std::vector<std::unique_ptr<Shader>> m_vShaders;

    std::unique_ptr<CDescriptorHeap> m_pDescriptorHeap;
    UINT m_nNextDescriptorIndex = 0;

    // Pass Constant Buffer
    ComPtr<ID3D12Resource> m_pd3dcbPass = nullptr;
    PassConstants* m_pcbMappedPass = nullptr;

    std::unique_ptr<CCamera> m_pCamera; // Added CCamera member
    GameObject* m_pPlayerGameObject = nullptr; // Added player GameObject pointer

    void AddRenderComponentsToHierarchy(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList, GameObject* pGameObject, Shader* pShader);
    void PrintHierarchy(GameObject* pGameObject, int nDepth);
public:
    D3D12_GPU_VIRTUAL_ADDRESS GetPassCBVAddress() const { if(m_pd3dcbPass) return m_pd3dcbPass->GetGPUVirtualAddress(); return 0; }
};