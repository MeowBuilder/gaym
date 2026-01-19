#include "stdafx.h"
#include "Room.h"

CRoom::CRoom()
{
}

CRoom::~CRoom()
{
}

void CRoom::Update(float deltaTime)
{
    // Active 상태일 때만 내부 오브젝트 업데이트
    if (m_eState == RoomState::Active)
    {
        for (auto& pGameObject : m_vGameObjects)
        {
            pGameObject->Update(deltaTime);
        }

        CheckClearCondition();
    }
}

void CRoom::Render(ID3D12GraphicsCommandList* pCommandList)
{
    // Inactive가 아닐 때만 렌더링 (또는 거리에 따라 판단 가능)
    if (m_eState != RoomState::Inactive)
    {
        for (auto& pGameObject : m_vGameObjects)
        {
            pGameObject->Render(pCommandList);
        }
    }
}

void CRoom::AddGameObject(std::unique_ptr<GameObject> pGameObject)
{
    m_vGameObjects.push_back(std::move(pGameObject));
}

void CRoom::SetState(RoomState state)
{
    if (m_eState == state) return;

    m_eState = state;

    // 상태 변경 시 필요한 로직 (예: Active가 되면 문을 닫음 등)
    switch (m_eState)
    {
    case RoomState::Active:
        // TODO: 진입 시 이벤트 (입구 봉쇄 등)
        break;
    case RoomState::Cleared:
        // TODO: 클리어 시 이벤트 (출구 개방 등)
        break;
    }
}

bool CRoom::IsPlayerInside(const XMFLOAT3& playerPos)
{
    return m_BoundingBox.Contains(XMLoadFloat3(&playerPos)) != DISJOINT;
}

void CRoom::CheckClearCondition()
{
    // 임시 구현: 방 안의 모든 GameObject가 적은 아니지만, 
    // 나중에 EnemyComponent 등을 가진 오브젝트의 생존 여부를 체크하도록 확장 가능
    // 현재는 로직 흐름만 구성
}
