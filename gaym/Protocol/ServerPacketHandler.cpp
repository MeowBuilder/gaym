#include "../ServerCore/pch.h"
#include "ServerPacketHandler.h"
#include "../NetworkManager.h"
#include <fstream>
#include <string>

// 파일 로그 함수
void WriteNetworkLog(const std::string& msg)
{
    std::ofstream ofs("network_log.txt", std::ios::app);
    ofs << msg << std::endl;
}

PacketHandlerFunc GPacketHandler[UINT16_MAX];

// 잘못된 패킷 처리
bool Handle_INVALID(PacketSessionRef& session, BYTE* buffer, int32 len)
{
    PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);

    OutputDebugStringA("[Network] Invalid packet received\n");
    WriteNetworkLog("[Network] Invalid packet received");

    return false;
}

// 로그인 응답 처리
bool Handle_S_LOGIN(PacketSessionRef& session, Protocol::S_LOGIN& pkt)
{
    if (pkt.success() == false)
    {
        OutputDebugStringA("[Network] Login failed\n");
        WriteNetworkLog("[Network] Login failed");
        return true;
    }

    OutputDebugStringA("[Network] Login success\n");
    WriteNetworkLog("[Network] Login success");

    if (pkt.players().size() == 0)
    {
        OutputDebugStringA("[Network] No characters, need to create one\n");
        WriteNetworkLog("[Network] No characters, need to create one");
    }

    // 자동으로 게임 입장 요청
    Protocol::C_ENTER_GAME enterGamePkt;
    enterGamePkt.set_playerindex(0);  // 첫번째 캐릭터로 입장
    auto sendBuffer = ServerPacketHandler::MakeSendBuffer(enterGamePkt);
    session->Send(sendBuffer);

    OutputDebugStringA("[Network] C_ENTER_GAME sent\n");
    WriteNetworkLog("[Network] C_ENTER_GAME sent");

    return true;
}

// 게임 입장 응답 처리
bool Handle_S_ENTER_GAME(PacketSessionRef& session, Protocol::S_ENTER_GAME& pkt)
{
    if (pkt.success())
    {
        uint64 playerId = pkt.playerid();

        // 로컬 플레이어 ID 설정
        NetworkManager* pNetMgr = NetworkManager::GetInstance();
        if (pNetMgr)
        {
            pNetMgr->SetLocalPlayerId(playerId);

            wchar_t wbuf[128];
            swprintf_s(wbuf, L"[Network] ENTER_GAME Success! My LocalPlayerId = %llu\n", playerId);
            OutputDebugString(wbuf);

            char buf[128];
            sprintf_s(buf, "[Network] ENTER_GAME Success! My LocalPlayerId = %llu", playerId);
            WriteNetworkLog(buf);
        }
    }
    else
    {
        OutputDebugStringA("[Network] ENTER_GAME Failed by server\n");
        WriteNetworkLog("[Network] ENTER_GAME Failed by server");
    }

    return true;
}

// 채팅 메시지 수신
bool Handle_S_CHAT(PacketSessionRef& session, Protocol::S_CHAT& pkt)
{
    OutputDebugStringA("[Network] Chat: ");
    OutputDebugStringA(pkt.msg().c_str());
    OutputDebugStringA("\n");

    std::string log = "[Network] Chat: " + pkt.msg();
    WriteNetworkLog(log);

    return true;
}

// 플레이어 스폰 처리
bool Handle_S_SPAWN(PacketSessionRef& session, Protocol::S_SPAWN& pkt)
{
    const Protocol::Player& player = pkt.player();

    uint64 playerId = player.playerid();
    std::string name = player.name();
    int playerType = static_cast<int>(player.playertype());

    // 현재 설정된 내 로컬 ID 확인
    NetworkManager* pNetMgr = NetworkManager::GetInstance();
    uint64 myLocalId = pNetMgr ? pNetMgr->GetLocalPlayerId() : 0;

    wchar_t wbuf[256];
    swprintf_s(wbuf, L"[Network] S_SPAWN received: PlayerId=%llu (MyLocalId=%llu) Name=%hs\n",
        playerId, myLocalId, name.c_str());
    OutputDebugString(wbuf);

    char buf[256];
    sprintf_s(buf, "[Network] Handle Spawn: PktId=%llu, MyLocalId=%llu, Name=%s",
        playerId, myLocalId, name.c_str());
    WriteNetworkLog(buf);

    // 기본 위치 (서버 Player 구조체에 좌표가 없으므로 일단 0)
    float x = 0.0f, y = 0.0f, z = 0.0f;

    // 나 자신이 아닌 원격 플레이어면 임시 오프셋 부여
    if (playerId != myLocalId)
    {
        x = (float)(rand() % 10 - 5);
        z = (float)(rand() % 10 - 5);
    }

    // NetworkManager를 통해 메인 스레드에서 처리하도록 큐에 추가
    if (pNetMgr)
    {
        pNetMgr->QueueSpawnPlayer(playerId, name, playerType, x, y, z);

        char buf2[256];
        sprintf_s(buf2, "[Network] QueueSpawnPlayer called: PlayerId=%llu, IsRemote=%s, Pos=(%.1f, %.1f, %.1f)",
            playerId, (playerId != myLocalId ? "TRUE" : "FALSE"), x, y, z);
        WriteNetworkLog(buf2);
    }

    return true;
}

// 플레이어 디스폰 처리
bool Handle_S_DESPAWN(PacketSessionRef& session, Protocol::S_DESPAWN& pkt)
{
    uint64 playerId = pkt.playerid();

    wchar_t wbuf[128];
    swprintf_s(wbuf, L"[Network] S_DESPAWN received: PlayerId=%llu\n", playerId);
    OutputDebugString(wbuf);

    char buf[128];
    sprintf_s(buf, "[Network] S_DESPAWN received: PlayerId=%llu", playerId);
    WriteNetworkLog(buf);

    // NetworkManager를 통해 메인 스레드에서 처리
    NetworkManager* pNetMgr = NetworkManager::GetInstance();
    if (pNetMgr)
    {
        pNetMgr->QueueDespawnPlayer(playerId);
        WriteNetworkLog("[Network] QueueDespawnPlayer called");
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

    char buf[256];
    sprintf_s(buf, "[Network] S_MOVE received: PlayerId=%llu Pos=(%.1f, %.1f, %.1f)",
        playerId, x, y, z);
    WriteNetworkLog(buf);

    // NetworkManager를 통해 메인 스레드에서 처리
    NetworkManager* pNetMgr = NetworkManager::GetInstance();
    if (pNetMgr)
    {
        pNetMgr->QueueMovePlayer(playerId, x, y, z);
        WriteNetworkLog("[Network] QueueMovePlayer called");
    }

    return true;
}