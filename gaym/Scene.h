#pragma once

#include <vector>
#include <memory>
#include "GameObject.h"

struct ID3D12Device;
struct ID3D12GraphicsCommandList;

class Scene
{
public:
    Scene();
    ~Scene();

    void Init(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList);
    void Update(float deltaTime);
    void Render(ID3D12GraphicsCommandList* pCommandList);

    GameObject* CreateGameObject();

private:
    std::vector<std::unique_ptr<GameObject>> m_vGameObjects;
};