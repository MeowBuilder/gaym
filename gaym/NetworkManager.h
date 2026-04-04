#pragma once

// ServerCore 헤더들
#include "ServerCore/CorePch.h"
#include "ServerCore/Service.h"
#include "ServerCore/ThreadManager.h"
#include "Protocol/ServerPacketHandler.h"

#include <unordered_map>
#include <mutex>
#include <DirectXMath.h>

// 전방 선언
struct ID3D12Device;
struct ID3D12GraphicsCommandList;
class GameObject;
class Scene;

// 네트워크 플레이어 정보 (큐에 저장용)
struct NetworkPlayerInfo
{
    uint64 playerId;
    std::string name;
    int playerType;
    float x, y, z;
};

// 패킷 처리를 위한 명령 타입
enum class NetworkCommand
{
    Spawn,
    Despawn,
    Move,
    Skill,
    SetLocalPlayerId
};

// 네트워크 명령 구조체
struct NetworkCommandData
{
    NetworkCommand type;
    uint64 playerId;
    std::string name;
    int playerType;
    float x, y, z;
    float dirX, dirY, dirZ;  // 방향 정보
    int skillType;           // 스킬 타입 (Protocol::SkillType)
};

// =============================================================================
// GameSession: 서버와의 세션을 관리하는 클래스
// =============================================================================
class GameSession : public PacketSession
{
public:
    virtual ~GameSession() override;

protected:
    // 서버 연결 성공 시 호출
    virtual void OnConnected() override;

    // 서버로부터 패킷 수신 시 호출
    virtual void OnRecvPacket(BYTE* buffer, int32 len) override;

    // 연결 해제 시 호출
    virtual void OnDisconnected() override;
};

// =============================================================================
// NetworkManager: 네트워크 연결 및 원격 플레이어 관리
// =============================================================================
class NetworkManager
{
public:
    // 싱글톤 접근자
    static NetworkManager* GetInstance();

    NetworkManager();
    ~NetworkManager();

    // 초기화 및 정리
    bool Initialize();
    void Shutdown();

    // 서버 연결
    bool Connect(const std::wstring& ip, uint16 port);
    void Disconnect();
    bool IsConnected() const { return m_bConnected; }

    // 프레임마다 호출 (큐에 쌓인 명령 처리)
    void Update(Scene* pScene, ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList);

    // 로컬 플레이어 이동 전송 (위치 + 방향)
    void SendMove(float x, float y, float z, float dirX, float dirY, float dirZ);

    // 로컬 플레이어 스킬 전송
    void SendSkill(int skillType, float x, float y, float z, float dirX, float dirY, float dirZ);

    // 로컬 플레이어 ID 설정/조회 (atomic으로 스레드 안전)
    void SetLocalPlayerId(uint64 playerId) { m_nLocalPlayerId.store(playerId); }
    uint64 GetLocalPlayerId() const { return m_nLocalPlayerId.load(); }

    // 세션 참조 설정 (GameSession::OnConnected에서 호출)
    void SetSession(std::shared_ptr<GameSession> session) { m_pSession = session; }
    std::shared_ptr<GameSession> GetSession() const { return m_pSession; }

    // 네트워크 스레드에서 호출 - 명령을 큐에 추가
    void QueueSpawnPlayer(uint64 playerId, const std::string& name, int playerType, float x, float y, float z);
    void QueueDespawnPlayer(uint64 playerId);
    void QueueMovePlayer(uint64 playerId, float x, float y, float z, float dirX, float dirY, float dirZ);
    void QueueSkill(uint64 playerId, int skillType, float x, float y, float z, float dirX, float dirY, float dirZ);
    void QueueSetLocalPlayerId(uint64 playerId);

    // 원격 플레이어 조회
    GameObject* GetRemotePlayer(uint64 playerId);

private:
    static NetworkManager* s_pInstance;

    // 서버 연결 상태
    std::atomic<bool> m_bConnected = false;
    std::atomic<bool> m_bShutdownRequested = false;

    // 로컬 플레이어 ID (atomic으로 스레드 안전하게 관리)
    std::atomic<uint64> m_nLocalPlayerId = 0;

    // 네트워크 서비스 및 세션
    std::shared_ptr<ClientService> m_pService;
    std::shared_ptr<GameSession> m_pSession;

    // 원격 플레이어 관리 (메인 스레드에서만 접근)
    std::unordered_map<uint64, GameObject*> m_mapRemotePlayers;

    // 네트워크 스레드에서 메인 스레드로 전달할 명령 큐
    std::mutex m_queueMutex;
    std::vector<NetworkCommandData> m_vCommandQueue;

    // LocalPlayerId가 설정되기 전에 도착한 Spawn 명령을 보류
    std::vector<NetworkCommandData> m_vPendingSpawns;

    // 메인 스레드에서 실행할 명령 처리
    void ProcessSpawnPlayer(Scene* pScene, ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList,
                           uint64 playerId, const std::string& name, int playerType, float x, float y, float z);
    void ProcessDespawnPlayer(Scene* pScene, uint64 playerId);
    void ProcessMovePlayer(uint64 playerId, float x, float y, float z, float dirX, float dirY, float dirZ);
    void ProcessSkill(uint64 playerId, int skillType, float x, float y, float z, float dirX, float dirY, float dirZ);

    // 원격 플레이어 마지막 이동 시간 (idle 전환용)
    std::unordered_map<uint64, float> m_mapRemotePlayerMoveTime;

    // idle 전환까지 대기 시간 (초)
    static constexpr float IDLE_TRANSITION_TIME = 0.15f;

public:
    // 원격 플레이어 idle 전환 체크 (Update에서 호출)
    void CheckRemotePlayerIdle(float deltaTime);
};
