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

struct SpotLight
{
    XMFLOAT4 m_xmf4SpotLightColor;
    XMFLOAT3 m_xmf3SpotLightPosition; float m_fSpotLightRange;
    XMFLOAT3 m_xmf3SpotLightDirection; float m_fSpotLightInnerCone;
    float m_fSpotLightOuterCone; float m_fPad5; float m_fPad6; float m_fPad7;
};

struct PassConstants
{
    XMFLOAT4X4 m_xmf4x4ViewProj;
    XMFLOAT4 m_xmf4LightColor;
    XMFLOAT3 m_xmf3LightDirection; float m_fPad0;
    XMFLOAT4 m_xmf4PointLightColor;
    XMFLOAT3 m_xmf3PointLightPosition; float m_fPad1;
    float m_fPointLightRange; float m_fPad2; float m_fPad3; float m_fPad4;
    XMFLOAT4 m_xmf4AmbientLight;
    XMFLOAT3 m_xmf3CameraPosition; float m_fPadCam; // Camera World Position
    SpotLight m_SpotLight;
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