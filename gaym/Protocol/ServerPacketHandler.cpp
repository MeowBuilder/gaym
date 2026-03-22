#include "../ServerCore/pch.h"
#include "ServerPacketHandler.h"
#include "../NetworkManager.h"

PacketHandlerFunc GPacketHandler[UINT16_MAX];

// 잘못된 패킷 처리
bool Handle_INVALID(PacketSessionRef& session, BYTE* buffer, int32 len)
{
    PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);
    OutputDebugStringA("[Network] Invalid packet received\n");
    return false;
}

// 로그인 응답 처리
bool Handle_S_LOGIN(PacketSessionRef& session, Protocol::S_LOGIN& pkt)
{
    if (pkt.success() == false)
    {
        OutputDebugStringA("[Network] Login failed\n");
        return true;
    }

    OutputDebugStringA("[Network] Login success\n");

    if (pkt.players().size() == 0)
    {
        // 캐릭터 생성 필요 (현재는 자동으로 첫번째 캐릭터로 입장)
        OutputDebugStringA("[Network] No characters, need to create one\n");
    }

    // 자동으로 게임 입장 요청
    Protocol::C_ENTER_GAME enterGamePkt;
    enterGamePkt.set_playerindex(0);  // 첫번째 캐릭터로 입장
    auto sendBuffer = ServerPacketHandler::MakeSendBuffer(enterGamePkt);
    session->Send(sendBuffer);

    OutputDebugStringA("[Network] C_ENTER_GAME sent\n");
    return true;
}

// 게임 입장 응답 처리
bool Handle_S_ENTER_GAME(PacketSessionRef& session, Protocol::S_ENTER_GAME& pkt)
{
    if (pkt.success())
    {
        OutputDebugStringA("[Network] Enter game success!\n");
    }
    else
    {
        OutputDebugStringA("[Network] Enter game failed\n");
    }
    return true;
}

// 채팅 메시지 수신
bool Handle_S_CHAT(PacketSessionRef& session, Protocol::S_CHAT& pkt)
{
    OutputDebugStringA("[Network] Chat: ");
    OutputDebugStringA(pkt.msg().c_str());
    OutputDebugStringA("\n");
    return true;
}

// 플레이어 스폰 처리
bool Handle_S_SPAWN(PacketSessionRef& session, Protocol::S_SPAWN& pkt)
{
    const Protocol::Player& player = pkt.player();

    uint64 playerId = player.playerid();
    std::string name = player.name();
    int playerType = static_cast<int>(player.playertype());

    // 기본 위치 (서버에서 위치 정보가 오면 그걸 사용)
    float x = 0.0f, y = 0.0f, z = 0.0f;

    wchar_t buf[256];
    swprintf_s(buf, L"[Network] S_SPAWN received: PlayerId=%llu Name=%hs\n", playerId, name.c_str());
    OutputDebugString(buf);

    // NetworkManager를 통해 메인 스레드에서 처리하도록 큐에 추가
    NetworkManager* pNetMgr = NetworkManager::GetInstance();
    if (pNetMgr)
    {
        // 첫 번째 스폰이면 로컬 플레이어 ID로 설정
        if (pNetMgr->GetLocalPlayerId() == 0)
        {
            pNetMgr->QueueSetLocalPlayerId(playerId);
        }

        pNetMgr->QueueSpawnPlayer(playerId, name, playerType, x, y, z);
    }

    return true;
}

// 플레이어 디스폰 처리
bool Handle_S_DESPAWN(PacketSessionRef& session, Protocol::S_DESPAWN& pkt)
{
    uint64 playerId = pkt.playerid();

    wchar_t buf[128];
    swprintf_s(buf, L"[Network] S_DESPAWN received: PlayerId=%llu\n", playerId);
    OutputDebugString(buf);

    // NetworkManager를 통해 메인 스레드에서 처리
    NetworkManager* pNetMgr = NetworkManager::GetInstance();
    if (pNetMgr)
    {
        pNetMgr->QueueDespawnPlayer(playerId);
    }

    return true;
}

// 플레이어 이동 처리
bool Handle_S_MOVE(PacketSessionRef& session, Protocol::S_MOVE& pkt)
{
    uint64 playerId = pkt.playerid();
    float x = pkt.x();
    float y = pkt.y();
    float z = pkt.z();

    // 디버그 로그 (너무 많이 출력되면 주석 처리)
    // wchar_t buf[128];
    // swprintf_s(buf, L"[Network] S_MOVE: PlayerId=%llu Pos=(%.1f, %.1f, %.1f)\n", playerId, x, y, z);
    // OutputDebugString(buf);

    // NetworkManager를 통해 메인 스레드에서 처리
    NetworkManager* pNetMgr = NetworkManager::GetInstance();
    if (pNetMgr)
    {
        pNetMgr->QueueMovePlayer(playerId, x, y, z);
    }

    return true;
}
