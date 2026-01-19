#pragma once
#include "stdafx.h"
#include "GameObject.h"

enum class RoomState {
    Inactive,   // 플레이어가 아직 진입하지 않음
    Active,     // 플레이어가 진입하여 전투 중
    Cleared     // 모든 적을 처치하여 클리어됨
};

class CRoom {
public:
    CRoom();
    virtual ~CRoom();

    virtual void Update(float deltaTime);
    virtual void Render(ID3D12GraphicsCommandList* pCommandList);

    // 오브젝트 관리
    void AddGameObject(std::unique_ptr<GameObject> pGameObject);
    const std::vector<std::unique_ptr<GameObject>>& GetGameObjects() const { return m_vGameObjects; }

    // 상태 및 영역 설정
    void SetState(RoomState state);
    RoomState GetState() const { return m_eState; }
    
    void SetBoundingBox(const BoundingBox& box) { m_BoundingBox = box; }
    const BoundingBox& GetBoundingBox() const { return m_BoundingBox; }

    // 플레이어 진입 체크
    bool IsPlayerInside(const XMFLOAT3& playerPos);

    // 방 클리어 조건 체크 (기본적으로는 적이 모두 제거되었는지 확인)
    virtual void CheckClearCondition();

protected:
    std::vector<std::unique_ptr<GameObject>> m_vGameObjects; // 방에 속한 모든 오브젝트
    RoomState m_eState = RoomState::Inactive;
    BoundingBox m_BoundingBox; // 방의 영역 (AABB)

    // 편리한 관리를 위해 적 오브젝트들만 따로 추적할 수도 있음 (필요 시)
};
