#pragma once
#include <vector>
#include <memory>

struct ID3D12GraphicsCommandList; // 전방 선언
struct ID3D12Device; // 전방 선언
class Component;                   // 전방 선언
class TransformComponent;          // 전방 선언

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

private:
	std::vector<std::unique_ptr<Component>> m_vComponents;
	TransformComponent* m_pTransform = nullptr;
};

#include "GameObject.inl"