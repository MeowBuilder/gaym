#include "stdafx.h"
#include "NetworkManager.h"
#include "Scene.h"
#include "GameObject.h"
#include "TransformComponent.h"
#include "RenderComponent.h"
#include "Mesh.h"
#include "Shader.h"
#include "MeshLoader.h"
#include "AnimationComponent.h"
#include "FluidSkillVFXManager.h"
#include "VFXLibrary.h"
#include "SkillTypes.h"
#include "ProjectileManager.h"

// 싱글톤 인스턴스
NetworkManager* NetworkManager::s_pInstance = nullptr;

// =============================================================================
// GameSession 구현
// =============================================================================

GameSession::~GameSession()
{
    OutputDebugString(L"[Network] GameSession destroyed\n");
}

void GameSession::OnConnected()
{
    OutputDebugString(L"[Network] Connected to server!\n");

    // NetworkManager에 세션 등록
    NetworkManager::GetInstance()->SetSession(
        std::static_pointer_cast<GameSession>(shared_from_this()));

    // C_LOGIN 패킷 전송
    Protocol::C_LOGIN loginPkt;
    auto sendBuffer = ServerPacketHandler::MakeSendBuffer(loginPkt);
    Send(sendBuffer);

    OutputDebugString(L"[Network] C_LOGIN sent\n");
}

void GameSession::OnRecvPacket(BYTE* buffer, int32 len)
{
    // ServerPacketHandler를 통해 패킷 처리
    PacketSessionRef session = GetPacketSessionRef();
    ServerPacketHandler::HandlePacket(session, buffer, len);
}

void GameSession::OnDisconnected()
{
    OutputDebugString(L"[Network] Disconnected from server\n");
}

// =============================================================================
// NetworkManager 구현
// =============================================================================

NetworkManager* NetworkManager::GetInstance()
{
    if (s_pInstance == nullptr)
    {
        s_pInstance = new NetworkManager();
    }
    return s_pInstance;
}

NetworkManager::NetworkManager()
{
    OutputDebugString(L"[Network] NetworkManager created\n");
}

NetworkManager::~NetworkManager()
{
    Shutdown();
    OutputDebugString(L"[Network] NetworkManager destroyed\n");
}

bool NetworkManager::Initialize()
{
    // ServerPacketHandler 초기화
    ServerPacketHandler::Init();

    OutputDebugString(L"[Network] NetworkManager initialized\n");
    return true;
}

void NetworkManager::Shutdown()
{
    if (!m_bConnected && !m_pService)
        return;

    m_bShutdownRequested = true;
    m_bConnected = false;

    // 워커 스레드가 종료될 시간을 줌
    // (Dispatch 타임아웃이 10ms이므로 충분히 대기)
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // 모든 스레드 조인
    GThreadManager->Join();

    if (m_pService)
    {
        m_pService->CloseService();
        m_pService = nullptr;
    }

    m_pSession = nullptr;

    // 원격 플레이어 맵 클리어 (GameObject는 Scene이 관리하므로 여기서 delete 하지 않음)
    m_mapRemotePlayers.clear();

    // 큐 정리
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_vCommandQueue.clear();
    }
    m_vPendingSpawns.clear();
    m_nLocalPlayerId.store(0);

    OutputDebugString(L"[Network] NetworkManager shutdown complete\n");
}

bool NetworkManager::Connect(const std::wstring& ip, uint16 port)
{
    if (m_bConnected)
    {
        OutputDebugString(L"[Network] Already connected!\n");
        return true;
    }

    try
    {
        // ClientService 생성
        m_pService = MakeShared<ClientService>(
            NetAddress(ip, port),
            MakeShared<IocpCore>(),
            MakeShared<GameSession>,  // 세션 팩토리
            1  // 최대 세션 수
        );

        if (!m_pService->Start())
        {
            OutputDebugString(L"[Network] Failed to start ClientService\n");
            return false;
        }

        // 워커 스레드 시작 (IOCP 이벤트 처리)
        GThreadManager->Launch([this]()
        {
            while (!m_bShutdownRequested)
            {
                // 10ms 타임아웃으로 Dispatch
                m_pService->GetIocpCore()->Dispatch(10);
            }
        });

        m_bConnected = true;
        OutputDebugString(L"[Network] Connection started to server\n");
        return true;
    }
    catch (const std::exception& e)
    {
        OutputDebugStringA("[Network] Exception during Connect: ");
        OutputDebugStringA(e.what());
        OutputDebugStringA("\n");
        return false;
    }
}

void NetworkManager::Disconnect()
{
    if (!m_bConnected)
        return;

    if (m_pSession)
    {
        m_pSession->Disconnect(L"User requested disconnect");
    }

    m_bConnected = false;
    OutputDebugString(L"[Network] Disconnected\n");
}

void NetworkManager::Update(Scene* pScene, ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList)
{
    if (!pScene || !pDevice || !pCommandList)
        return;

    // 큐에 쌓인 명령들을 메인 스레드에서 처리
    std::vector<NetworkCommandData> commands;
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        commands.swap(m_vCommandQueue);
    }

    // 1차: SetLocalPlayerId 명령을 먼저 처리 (Spawn보다 먼저 ID가 설정되어야 함)
    bool localIdWasSet = false;
    for (const auto& cmd : commands)
    {
        if (cmd.type == NetworkCommand::SetLocalPlayerId)
        {
            m_nLocalPlayerId.store(cmd.playerId);
            localIdWasSet = true;
            wchar_t buf[128];
            swprintf_s(buf, L"[Network] Local player ID set to: %llu\n", cmd.playerId);
            OutputDebugString(buf);
        }
    }

    // LocalPlayerId가 방금 설정되었으면 pending spawn 처리
    if (localIdWasSet && !m_vPendingSpawns.empty())
    {
        OutputDebugString(L"[Network] Processing pending spawns after LocalPlayerId set\n");
        for (const auto& pending : m_vPendingSpawns)
        {
            ProcessSpawnPlayer(pScene, pDevice, pCommandList,
                             pending.playerId, pending.name, pending.playerType,
                             pending.x, pending.y, pending.z);
        }
        m_vPendingSpawns.clear();
    }

    // 2차: 나머지 명령 처리
    for (const auto& cmd : commands)
    {
        switch (cmd.type)
        {
        case NetworkCommand::Spawn:
            // LocalPlayerId가 아직 설정되지 않았으면 pending 큐에 보관
            if (m_nLocalPlayerId.load() == 0)
            {
                wchar_t buf[128];
                swprintf_s(buf, L"[Network] Spawn deferred (LocalPlayerId not set): PlayerId=%llu\n", cmd.playerId);
                OutputDebugString(buf);
                m_vPendingSpawns.push_back(cmd);
            }
            else
            {
                ProcessSpawnPlayer(pScene, pDevice, pCommandList,
                                 cmd.playerId, cmd.name, cmd.playerType, cmd.x, cmd.y, cmd.z);
            }
            break;

        case NetworkCommand::Despawn:
            ProcessDespawnPlayer(pScene, cmd.playerId);
            break;

        case NetworkCommand::Move:
            ProcessMovePlayer(cmd.playerId, cmd.x, cmd.y, cmd.z, cmd.dirX, cmd.dirY, cmd.dirZ);
            break;

        case NetworkCommand::Skill:
            ProcessSkill(pScene, cmd.playerId, cmd.skillType, cmd.x, cmd.y, cmd.z, cmd.dirX, cmd.dirY, cmd.dirZ);
            break;

        case NetworkCommand::SetLocalPlayerId:
            // 이미 1차에서 처리됨
            break;

        case NetworkCommand::RoomTransition:
            ProcessRoomTransition(pScene, cmd.stageIndex, cmd.roomIndex, cmd.isBossRoom);
            break;

        case NetworkCommand::MonsterSpawn:
            ProcessMonsterSpawn(pScene, pDevice, pCommandList,
                                cmd.monsterId, cmd.monsterType,
                                cmd.x, cmd.y, cmd.z, cmd.monsterYaw,
                                cmd.monsterHp, cmd.monsterIsBoss);
            break;

        case NetworkCommand::MonsterMove:
            ProcessMonsterMove(cmd.monsterId, cmd.x, cmd.y, cmd.z, cmd.monsterYaw);
            break;

        case NetworkCommand::MonsterDespawn:
            ProcessMonsterDespawn(pScene, cmd.monsterId);
            break;
        }
    }
}

void NetworkManager::SendMove(float x, float y, float z, float dirX, float dirY, float dirZ)
{
    if (!m_bConnected || !m_pSession)
        return;

    Protocol::C_MOVE movePkt;
    movePkt.set_x(x);
    movePkt.set_y(y);
    movePkt.set_z(z);
    movePkt.set_dirx(dirX);
    movePkt.set_diry(dirY);
    movePkt.set_dirz(dirZ);

    auto sendBuffer = ServerPacketHandler::MakeSendBuffer(movePkt);
    m_pSession->Send(sendBuffer);
}

void NetworkManager::SendSkill(int skillType, float x, float y, float z, float dirX, float dirY, float dirZ)
{
    if (!m_bConnected || !m_pSession)
        return;

    Protocol::C_SKILL skillPkt;
    skillPkt.set_skilltype(static_cast<Protocol::SkillType>(skillType));
    skillPkt.set_x(x);
    skillPkt.set_y(y);
    skillPkt.set_z(z);
    skillPkt.set_dirx(dirX);
    skillPkt.set_diry(dirY);
    skillPkt.set_dirz(dirZ);

    auto sendBuffer = ServerPacketHandler::MakeSendBuffer(skillPkt);
    m_pSession->Send(sendBuffer);
}

void NetworkManager::SendPortalInteract()
{
    if (!m_bConnected || !m_pSession)
        return;

    Protocol::C_PORTAL_INTERACT pkt;
    auto sendBuffer = ServerPacketHandler::MakeSendBuffer(pkt);
    m_pSession->Send(sendBuffer);

    OutputDebugString(L"[Network] C_PORTAL_INTERACT sent\n");
}

void NetworkManager::SendTorchInteract()
{
    if (!m_bConnected || !m_pSession)
        return;

    Protocol::C_TORCH_INTERACT pkt;
    auto sendBuffer = ServerPacketHandler::MakeSendBuffer(pkt);
    m_pSession->Send(sendBuffer);

    OutputDebugString(L"[Network] C_TORCH_INTERACT sent\n");
}

void NetworkManager::QueueRoomTransition(uint32 stageIndex, uint32 roomIndex, bool isBossRoom)
{
    std::lock_guard<std::mutex> lock(m_queueMutex);

    NetworkCommandData cmd{};
    cmd.type = NetworkCommand::RoomTransition;
    cmd.stageIndex = stageIndex;
    cmd.roomIndex = roomIndex;
    cmd.isBossRoom = isBossRoom;

    m_vCommandQueue.push_back(cmd);
}

void NetworkManager::QueueMonsterSpawn(uint64 monsterId, uint32 monsterType,
                                       float x, float y, float z, float yaw,
                                       float hp, bool isBoss)
{
    std::lock_guard<std::mutex> lock(m_queueMutex);
    NetworkCommandData cmd{};
    cmd.type = NetworkCommand::MonsterSpawn;
    cmd.monsterId = monsterId;
    cmd.monsterType = monsterType;
    cmd.x = x; cmd.y = y; cmd.z = z;
    cmd.monsterYaw = yaw;
    cmd.monsterHp = hp;
    cmd.monsterIsBoss = isBoss;
    m_vCommandQueue.push_back(cmd);
}

void NetworkManager::QueueMonsterMove(uint64 monsterId, float x, float y, float z, float yaw)
{
    std::lock_guard<std::mutex> lock(m_queueMutex);
    NetworkCommandData cmd{};
    cmd.type = NetworkCommand::MonsterMove;
    cmd.monsterId = monsterId;
    cmd.x = x; cmd.y = y; cmd.z = z;
    cmd.monsterYaw = yaw;
    m_vCommandQueue.push_back(cmd);
}

void NetworkManager::QueueMonsterDespawn(uint64 monsterId)
{
    std::lock_guard<std::mutex> lock(m_queueMutex);
    NetworkCommandData cmd{};
    cmd.type = NetworkCommand::MonsterDespawn;
    cmd.monsterId = monsterId;
    m_vCommandQueue.push_back(cmd);
}

GameObject* NetworkManager::GetServerMonster(uint64 monsterId)
{
    auto it = m_mapServerMonsters.find(monsterId);
    return (it != m_mapServerMonsters.end()) ? it->second : nullptr;
}

void NetworkManager::ProcessRoomTransition(Scene* pScene, uint32 stageIndex, uint32 roomIndex, bool isBossRoom)
{
    if (!pScene)
        return;

    wchar_t buf[256];
    swprintf_s(buf, L"[Network] ProcessRoomTransition stage=%u room=%u boss=%d\n",
        stageIndex, roomIndex, isBossRoom ? 1 : 0);
    OutputDebugString(buf);

    if (isBossRoom)
    {
        // A단계: 현재는 불 보스만 연결 (스테이지별 보스는 차후 작업)
        pScene->TransitionToBossRoom();
    }
    else
    {
        // roomIndex를 풀 인덱스로 사용 → 모든 클라가 동일한 맵 로드
        pScene->TransitionToRoomByIndex(static_cast<int>(roomIndex));
    }
}

void NetworkManager::QueueSpawnPlayer(uint64 playerId, const std::string& name, int playerType, float x, float y, float z)
{
    std::lock_guard<std::mutex> lock(m_queueMutex);

    NetworkCommandData cmd;
    cmd.type = NetworkCommand::Spawn;
    cmd.playerId = playerId;
    cmd.name = name;
    cmd.playerType = playerType;
    cmd.x = x;
    cmd.y = y;
    cmd.z = z;

    m_vCommandQueue.push_back(cmd);
}

void NetworkManager::QueueDespawnPlayer(uint64 playerId)
{
    std::lock_guard<std::mutex> lock(m_queueMutex);

    NetworkCommandData cmd;
    cmd.type = NetworkCommand::Despawn;
    cmd.playerId = playerId;

    m_vCommandQueue.push_back(cmd);
}

void NetworkManager::QueueMovePlayer(uint64 playerId, float x, float y, float z, float dirX, float dirY, float dirZ)
{
    std::lock_guard<std::mutex> lock(m_queueMutex);

    NetworkCommandData cmd;
    cmd.type = NetworkCommand::Move;
    cmd.playerId = playerId;
    cmd.x = x;
    cmd.y = y;
    cmd.z = z;
    cmd.dirX = dirX;
    cmd.dirY = dirY;
    cmd.dirZ = dirZ;

    m_vCommandQueue.push_back(cmd);
}

void NetworkManager::QueueSkill(uint64 playerId, int skillType, float x, float y, float z, float dirX, float dirY, float dirZ)
{
    std::lock_guard<std::mutex> lock(m_queueMutex);

    NetworkCommandData cmd;
    cmd.type = NetworkCommand::Skill;
    cmd.playerId = playerId;
    cmd.skillType = skillType;
    cmd.x = x;
    cmd.y = y;
    cmd.z = z;
    cmd.dirX = dirX;
    cmd.dirY = dirY;
    cmd.dirZ = dirZ;

    m_vCommandQueue.push_back(cmd);
}

void NetworkManager::QueueSetLocalPlayerId(uint64 playerId)
{
    std::lock_guard<std::mutex> lock(m_queueMutex);

    NetworkCommandData cmd;
    cmd.type = NetworkCommand::SetLocalPlayerId;
    cmd.playerId = playerId;

    m_vCommandQueue.push_back(cmd);
}

GameObject* NetworkManager::GetRemotePlayer(uint64 playerId)
{
    auto it = m_mapRemotePlayers.find(playerId);
    if (it != m_mapRemotePlayers.end())
    {
        return it->second;
    }
    return nullptr;
}

void NetworkManager::ProcessSpawnPlayer(Scene* pScene, ID3D12Device* pDevice,
                                        ID3D12GraphicsCommandList* pCommandList,
                                        uint64 playerId, const std::string& name,
                                        int playerType, float x, float y, float z)
{
    wchar_t idLog[256]; // 로그용 버퍼 선언

    // 로컬 플레이어 ID 확인
    uint64 myId = GetLocalPlayerId();
    
    swprintf_s(idLog, 256, L"[Network] Handle Spawn: PktId=%llu, MyLocalId=%llu\n", playerId, myId);
    OutputDebugString(idLog);

    // 로컬 플레이어라면 무시
    if (playerId == myId)
    {
        OutputDebugString(L"[Network] Skipping spawn for local player (Self)\n");
        return;
    }

    // 이미 존재하는 플레이어라면 무시
    if (m_mapRemotePlayers.find(playerId) != m_mapRemotePlayers.end())
    {
        swprintf_s(idLog, 256, L"[Network] Remote player %llu already exists. Updating position.\n", playerId);
        OutputDebugString(idLog);
        // Spawn에는 방향 정보가 없으므로 기본 방향 (0, 0, 1) 사용
        ProcessMovePlayer(playerId, x, y, z, 0.0f, 0.0f, 1.0f);
        return;
    }

    // 원격 플레이어를 전역 오브젝트로 생성하기 위해 CurrentRoom을 임시 해제
    // (Room에 등록되면 Room 전환 시 삭제되거나 업데이트가 안 될 수 있음)
    CRoom* pTempRoom = pScene->GetCurrentRoom();
    pScene->SetCurrentRoom(nullptr);

    // 새 원격 플레이어 모델 로드
    GameObject* pRemotePlayer = MeshLoader::LoadGeometryFromFile(pScene, pDevice, pCommandList, NULL, "Assets/Player/MageBlue.bin");
    if (!pRemotePlayer)
    {
        OutputDebugString(L"[Network] Failed to load remote player model, falling back to cube\n");
        pRemotePlayer = pScene->CreateGameObject(pDevice, pCommandList);

        CubeMesh* pCubeMesh = new CubeMesh(pDevice, pCommandList, 1.0f, 2.0f, 1.0f);
        pCubeMesh->AddRef();
        pRemotePlayer->SetMesh(pCubeMesh);
        pRemotePlayer->AddComponent<RenderComponent>()->SetMesh(pCubeMesh);
    }

    // CurrentRoom 복원
    pScene->SetCurrentRoom(pTempRoom);

    // 이름 설정
    sprintf_s(pRemotePlayer->m_pstrFrameName, "RemotePlayer_%llu", playerId);

    // 위치 및 스케일 설정
    TransformComponent* pTransform = pRemotePlayer->GetTransform();
    if (pTransform)
    {
        pTransform->SetPosition(x, y, z);
        pTransform->SetScale(5.0f, 5.0f, 5.0f); 
    }

    // 애니메이션 추가
    auto* pAnim = pRemotePlayer->AddComponent<AnimationComponent>();
    if (pAnim)
    {
        pAnim->LoadAnimation("Assets/Player/MageBlue_Anim.bin");
        pAnim->Play("Idle", true);
    }

    // 셰이더 등록
    Shader* pDefaultShader = pScene->GetDefaultShader();
    if (pDefaultShader)
    {
        pScene->AddRenderComponentsToHierarchy(pDevice, pCommandList, pRemotePlayer, pDefaultShader, true);
    }

    // 컴포넌트 초기화 (AnimationComponent::BuildBoneCache 포함)
    pRemotePlayer->Init(pDevice, pCommandList);

    // 맵에 등록
    m_mapRemotePlayers[playerId] = pRemotePlayer;

    swprintf_s(idLog, 256, L"[Network] SUCCESS: Spawned RemotePlayer_%llu (%hs). Total RemoteCount: %zu\n",
              playerId, name.c_str(), m_mapRemotePlayers.size());
    OutputDebugString(idLog);
}

void NetworkManager::ProcessDespawnPlayer(Scene* pScene, uint64 playerId)
{
    // 로컬 플레이어라면 무시
    if (playerId == m_nLocalPlayerId.load())
        return;

    auto it = m_mapRemotePlayers.find(playerId);
    if (it == m_mapRemotePlayers.end())
    {
        wchar_t buf[128];
        swprintf_s(buf, L"[Network] Despawn failed: player %llu not found\n", playerId);
        OutputDebugString(buf);
        return;
    }

    // Scene에 삭제 요청
    GameObject* pRemotePlayer = it->second;
    pScene->MarkForDeletion(pRemotePlayer);

    // 맵에서 제거
    m_mapRemotePlayers.erase(it);

    wchar_t buf[128];
    swprintf_s(buf, L"[Network] Despawned remote player %llu\n", playerId);
    OutputDebugString(buf);
}

void NetworkManager::ProcessMovePlayer(uint64 playerId, float x, float y, float z, float dirX, float dirY, float dirZ)
{
    // 로컬 플레이어라면 무시 (로컬은 자체 업데이트)
    if (playerId == m_nLocalPlayerId.load())
        return;

    auto it = m_mapRemotePlayers.find(playerId);
    if (it == m_mapRemotePlayers.end())
        return;

    GameObject* pRemotePlayer = it->second;
    TransformComponent* pTransform = pRemotePlayer->GetTransform();
    if (pTransform)
    {
        // 위치 설정
        pTransform->SetPosition(x, y, z);

        // 방향 벡터로 Y축 회전 계산 (XZ 평면 기준)
        float length = sqrtf(dirX * dirX + dirZ * dirZ);
        if (length > 0.001f)
        {
            // atan2로 Y축 회전각 계산 (라디안)
            float yaw = atan2f(dirX, dirZ);
            // 라디안을 도(degree)로 변환
            float yawDegrees = XMConvertToDegrees(yaw);

            // Y축 회전만 적용 (기존 X, Z 회전은 유지)
            XMFLOAT3 currentRot = pTransform->GetRotation();
            pTransform->SetRotation(currentRot.x, yawDegrees, currentRot.z);
        }
    }

    // 걷기 애니메이션 활성화
    AnimationComponent* pAnim = pRemotePlayer->GetComponent<AnimationComponent>();
    if (pAnim)
    {
        // CrossFade로 부드럽게 전환 (이미 걷기 중이면 무시)
        pAnim->CrossFade("Walk", 0.1f, true);
    }

    // 마지막 이동 시간 기록 (idle 전환용)
    m_mapRemotePlayerMoveTime[playerId] = 0.0f;
}

void NetworkManager::CheckRemotePlayerIdle(float deltaTime)
{
    // 원격 플레이어들의 마지막 이동 시간 업데이트
    for (auto it = m_mapRemotePlayerMoveTime.begin(); it != m_mapRemotePlayerMoveTime.end(); )
    {
        uint64 playerId = it->first;
        float& timeSinceMove = it->second;
        timeSinceMove += deltaTime;

        // 일정 시간 동안 이동 패킷이 없으면 idle로 전환
        if (timeSinceMove >= IDLE_TRANSITION_TIME)
        {
            auto playerIt = m_mapRemotePlayers.find(playerId);
            if (playerIt != m_mapRemotePlayers.end())
            {
                AnimationComponent* pAnim = playerIt->second->GetComponent<AnimationComponent>();
                if (pAnim)
                {
                    pAnim->CrossFade("Idle", 0.2f, true);
                }
            }
            // 처리 완료 후 맵에서 제거
            it = m_mapRemotePlayerMoveTime.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void NetworkManager::CheckRemotePlayerVFXTimeout(Scene* pScene, float deltaTime)
{
    FluidSkillVFXManager* pVFXManager = pScene ? pScene->GetFluidVFXManager() : nullptr;
    if (!pVFXManager)
        return;

    for (auto it = m_mapRemotePlayerVFX.begin(); it != m_mapRemotePlayerVFX.end(); )
    {
        RemoteVFXState& state = it->second;
        state.lastUpdateTime += deltaTime;

        // 타임아웃 시 VFX 종료
        if (state.lastUpdateTime >= VFX_TIMEOUT)
        {
            if (state.vfxId >= 0)
            {
                pVFXManager->StopEffect(state.vfxId);
            }
            it = m_mapRemotePlayerVFX.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void NetworkManager::ProcessSkill(Scene* pScene, uint64 playerId, int skillType, float x, float y, float z, float dirX, float dirY, float dirZ)
{
    // 로컬 플레이어라면 무시 (로컬은 자체 처리)
    if (playerId == m_nLocalPlayerId.load())
        return;

    auto it = m_mapRemotePlayers.find(playerId);
    if (it == m_mapRemotePlayers.end())
        return;

    GameObject* pRemotePlayer = it->second;
    TransformComponent* pTransform = pRemotePlayer->GetTransform();
    if (pTransform)
    {
        // 위치 설정
        pTransform->SetPosition(x, y, z);

        // 방향 벡터로 Y축 회전 계산 (XZ 평면 기준)
        float length = sqrtf(dirX * dirX + dirZ * dirZ);
        if (length > 0.001f)
        {
            float yaw = atan2f(dirX, dirZ);
            float yawDegrees = XMConvertToDegrees(yaw);
            XMFLOAT3 currentRot = pTransform->GetRotation();
            pTransform->SetRotation(currentRot.x, yawDegrees, currentRot.z);
        }
    }

    // 스킬 애니메이션 재생 (현재 플레이어 모델은 Attack1만 지원)
    AnimationComponent* pAnim = pRemotePlayer->GetComponent<AnimationComponent>();
    if (pAnim)
    {
        // 스킬 애니메이션은 한 번만 재생 (루프 X), forceRestart=true로 연속 공격 시에도 재시작
        pAnim->CrossFade("Attack1", 0.1f, false, true);
    }

    // 스킬 원점 (캐릭터 위치 + 높이 오프셋)
    XMFLOAT3 skillOrigin = XMFLOAT3(x, y + 1.5f, z);
    XMFLOAT3 skillDirection = XMFLOAT3(dirX, dirY, dirZ);

    // 방향 벡터 정규화
    float dirLen = sqrtf(dirX * dirX + dirY * dirY + dirZ * dirZ);
    if (dirLen > 0.001f)
    {
        skillDirection.x /= dirLen;
        skillDirection.y /= dirLen;
        skillDirection.z /= dirLen;
    }
    else
    {
        skillDirection = XMFLOAT3(0.0f, 0.0f, 1.0f);  // 기본 방향
    }

    // 스킬 타입에 따라 분기 처리
    // 1=Q (WaveSlash - VFX), 2=E (FireBeam - 채널링 VFX), 3=R (Meteor - 낙하 VFX), 4=RightClick (Fireball - 투사체)

    FluidSkillVFXManager* pVFXManager = pScene ? pScene->GetFluidVFXManager() : nullptr;
    ProjectileManager* pProjManager = pScene ? pScene->GetProjectileManager() : nullptr;

    switch (skillType)
    {
    case 1:  // Q 스킬 (WaveSlash) - VFX 단발
        if (pVFXManager)
        {
            // 수평 방향 VFX
            XMFLOAT3 horizontalDir = skillDirection;
            horizontalDir.y = 0.0f;
            float hLen = sqrtf(horizontalDir.x * horizontalDir.x + horizontalDir.z * horizontalDir.z);
            if (hLen > 0.001f)
            {
                horizontalDir.x /= hLen;
                horizontalDir.z /= hLen;
            }

            VFXSequenceDef seqDef = VFXLibrary::Get().GetDef(SkillSlot::Q, RUNE_NONE, ElementType::Fire);
            int vfxId = pVFXManager->SpawnSequenceEffect(skillOrigin, horizontalDir, seqDef);

            wchar_t vfxBuf[128];
            swprintf_s(vfxBuf, L"[Network] Spawned Q (WaveSlash) VFX: vfxId=%d\n", vfxId);
            OutputDebugString(vfxBuf);
        }
        break;

    case 2:  // E 스킬 (FireBeam) - 채널링 VFX (TrackEffect 필요)
        if (pVFXManager)
        {
            // 기존 VFX 상태 확인
            auto vfxIt = m_mapRemotePlayerVFX.find(playerId);
            bool hasExistingVFX = (vfxIt != m_mapRemotePlayerVFX.end() && vfxIt->second.vfxId >= 0);

            if (hasExistingVFX && vfxIt->second.skillType == skillType)
            {
                // 같은 스킬 계속 사용 중 → TrackEffect로 방향 업데이트
                pVFXManager->TrackEffect(vfxIt->second.vfxId, skillOrigin, skillDirection);
                vfxIt->second.lastUpdateTime = 0.0f;
            }
            else
            {
                // 새 스킬 → 기존 VFX 종료 후 새로 생성
                if (hasExistingVFX)
                {
                    pVFXManager->StopEffect(vfxIt->second.vfxId);
                }

                VFXSequenceDef seqDef = VFXLibrary::Get().GetDef(SkillSlot::E, RUNE_NONE, ElementType::Fire);
                int vfxId = pVFXManager->SpawnSequenceEffect(skillOrigin, skillDirection, seqDef);

                RemoteVFXState state;
                state.vfxId = vfxId;
                state.skillType = skillType;
                state.lastUpdateTime = 0.0f;
                m_mapRemotePlayerVFX[playerId] = state;

                wchar_t vfxBuf[128];
                swprintf_s(vfxBuf, L"[Network] Spawned E (FireBeam) VFX: vfxId=%d\n", vfxId);
                OutputDebugString(vfxBuf);
            }
        }
        break;

    case 3:  // R 스킬 (Meteor) - 상공에서 낙하 VFX
        if (pVFXManager)
        {
            // R 스킬은 송신 측이 dirX/Y/Z 슬롯에 **절대 타겟 좌표**를 실어 보냄
            // (SkillComponent 송신 로직 참조). 정규화된 방향 벡터가 아님에 주의.
            XMFLOAT3 targetPos = XMFLOAT3(dirX, dirY, dirZ);

            // 송신 측과 동일하게 MeteorBehavior::METEOR_SPAWN_HEIGHT = 50 사용
            const float meteorSpawnHeight = 50.0f;
            XMFLOAT3 spawnPos = XMFLOAT3(targetPos.x, targetPos.y + meteorSpawnHeight, targetPos.z);

            // 낙하 방향 = 아래
            XMFLOAT3 downDir = XMFLOAT3(0.0f, -1.0f, 0.0f);

            VFXSequenceDef seqDef = VFXLibrary::Get().GetDef(SkillSlot::R, RUNE_NONE, ElementType::Fire);
            int vfxId = pVFXManager->SpawnSequenceEffect(spawnPos, downDir, seqDef);

            wchar_t vfxBuf[128];
            swprintf_s(vfxBuf, L"[Network] Spawned R (Meteor) VFX: vfxId=%d, target=(%.1f,%.1f,%.1f) spawn=(%.1f,%.1f,%.1f)\n",
                vfxId, targetPos.x, targetPos.y, targetPos.z, spawnPos.x, spawnPos.y, spawnPos.z);
            OutputDebugString(vfxBuf);
        }
        break;

    case 4:  // 우클릭 (Fireball) - 실제 투사체
        if (pProjManager)
        {
            // Fireball 파라미터 (FireballBehavior 기준)
            float speed = 30.0f;
            float radius = 0.5f;
            float explosionRadius = 3.0f;
            float scale = 1.0f;

            // 타겟 위치 (수평 방향으로 50m)
            XMFLOAT3 targetPos = XMFLOAT3(
                skillOrigin.x + skillDirection.x * 50.0f,
                skillOrigin.y,  // 수평 유지
                skillOrigin.z + skillDirection.z * 50.0f
            );

            pProjManager->SpawnProjectile(
                skillOrigin,
                targetPos,
                0.0f,               // damage=0 (원격은 데미지 없음)
                speed,
                radius,
                explosionRadius,
                ElementType::Fire,
                pRemotePlayer,
                true,
                scale,
                RuneCombo{},
                0.0f
            );

            wchar_t projBuf[128];
            swprintf_s(projBuf, L"[Network] Spawned RightClick (Fireball) projectile\n");
            OutputDebugString(projBuf);
        }
        break;

    default:
        OutputDebugString(L"[Network] Unknown skill type\n");
        break;
    }

    wchar_t buf[128];
    swprintf_s(buf, L"[Network] ProcessSkill: PlayerId=%llu SkillType=%d\n", playerId, skillType);
    OutputDebugString(buf);
}

// =============================================================================
// 몬스터 처리 (서버 권위)
// =============================================================================

// monsterType (서버 MonsterType enum) → 클라 프리셋 메쉬/애니메이션/스케일 매핑
struct MonsterPreset
{
    const char* meshPath;
    const char* animPath;
    float scale;
    const char* idleClip;
    const char* walkClip;
    const char* texturePath;  // 명시적 텍스처 경로 — .bin에 <AlbedoMap> 없을 때 필수
};

static MonsterPreset GetMonsterPresetByType(uint32 monsterType)
{
    // 서버 MonsterType enum 순서와 일치해야 함:
    // 0 None, 1 TestEnemy, 2 AirElemental, 3 RangedEnemy, 4 RushAoEEnemy, 5 RushFrontEnemy,
    // 6 Dragon, 7 Kraken, 8 Golem, 9 Demon, 10 BlueDragon
    // 클립 이름은 서버 동기화 이전 상태로 원복 — invisibility 디버깅 중이므로 diff 최소화.
    // (실제 .bin에 맞지 않을 수 있으나, 그건 별개 문제로 나중에 별도 처리)
    switch (monsterType)
    {
    case 2: // AirElemental
        return { "Assets/Enemies/Elementals/AirElemental_Bl/AirElemental_Bl.bin",
                 "Assets/Enemies/Elementals/AirElemental_Bl/AirElemental_Bl_Anim.bin",
                 2.0f, "Idle", "Walk",
                 "Assets/Enemies/Elementals/AirElemental_Bl/Textures/T_AirElemental_Body_Bl_D.png" };
    case 3: // RangedEnemy (StormElemental)
        return { "Assets/Enemies/Elementals/StormElemental_Bl/StormElemental_Bl.bin",
                 "Assets/Enemies/Elementals/StormElemental_Bl/StormElemental_Bl_Anim.bin",
                 2.0f, "Idle", "Walk",
                 "Assets/Enemies/Elementals/StormElemental_Bl/Textures/T_StormElemental_Bl_D.png" };
    case 4: // RushAoEEnemy (FireGolem)
        return { "Assets/Enemies/Elementals/FireGolem_Rd/FireGolem_Rd.bin",
                 "Assets/Enemies/Elementals/FireGolem_Rd/FireGolem_Rd_Anim.bin",
                 2.0f, "Idle", "Walk",
                 "Assets/Enemies/Elementals/FireGolem_Rd/Textures/T_FireGolem_Rd_D.png" };
    case 5: // RushFrontEnemy (EarthElemental)
        return { "Assets/Enemies/Elementals/EarthElemental_Gn/EarthElemental_Gn.bin",
                 "Assets/Enemies/Elementals/EarthElemental_Gn/EarthElemental_Gn_Anim.bin",
                 2.0f, "Idle", "Walk",
                 "Assets/Enemies/Elementals/EarthElemental_Gn/Textures/T_EarthElemental_Gn_D.png" };
    case 6: // Dragon (Red)
        return { "Assets/Enemies/Dragon/Red.bin",
                 "Assets/Enemies/Dragon/Red_Anim.bin",
                 3.0f, "Idle01", "Walk", "" };
    case 7: // Kraken
        return { "Assets/Enemies/Kraken/KRAKEN.bin",
                 "Assets/Enemies/Kraken/KRAKEN_Anim.bin",
                 3.0f, "Idle", "Walk", "" };
    case 8: // Golem
        return { "Assets/Enemies/golem/Golem01_Generic_prefab.bin",
                 "Assets/Enemies/golem/Golem01_Generic_prefab_Anim.bin",
                 8.0f, "Golem_battle_stand_ge", "Golem_battle_walk_ge", "" };
    case 9: // Demon
        return { "Assets/Enemies/demon/Demon.bin",
                 "Assets/Enemies/demon/Demon_Anim.bin",
                 3.5f, "Idle1", "Run", "" };
    case 10: // BlueDragon
        return { "Assets/Enemies/Dragon_blue/Blue.bin",
                 "Assets/Enemies/Dragon_blue/Blue_Anim.bin",
                 3.0f, "Idle01", "Walk", "" };
    case 1: // TestEnemy — 메쉬 없음, 큐브 fallback 생략하고 air 대체
    default:
        return { "Assets/Enemies/Elementals/AirElemental_Bl/AirElemental_Bl.bin",
                 "Assets/Enemies/Elementals/AirElemental_Bl/AirElemental_Bl_Anim.bin",
                 2.0f, "Idle", "Walk",
                 "Assets/Enemies/Elementals/AirElemental_Bl/Textures/T_AirElemental_Body_Bl_D.png" };
    }
}

// EnemySpawner::LoadTextureToHierarchy 미러 — 하이러키 순회하며 텍스처+흰 머티리얼 적용.
// 목적: MATERIAL이 garbage로 초기화되어 diffuse=0 → 메쉬가 까맣게 렌더되어 보이지 않는 문제 해결.
static void ApplyWhiteMaterialAndTextureToHierarchy(
    Scene* pScene, ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList,
    GameObject* pGO, const char* texturePath)
{
    if (!pGO || !pScene) return;

    if (pGO->GetMesh())
    {
        MATERIAL mat;
        mat.m_cAmbient  = XMFLOAT4(0.3f, 0.3f, 0.3f, 1.0f);
        mat.m_cDiffuse  = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        mat.m_cSpecular = XMFLOAT4(0.3f, 0.3f, 0.3f, 32.0f);
        mat.m_cEmissive = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
        pGO->SetMaterial(mat);

        if (texturePath && texturePath[0] != '\0' && !pGO->HasTexture())
        {
            pGO->SetTextureName(texturePath);
            D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
            D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle;
            pScene->AllocateDescriptor(&cpuHandle, &gpuHandle);
            pGO->LoadTexture(pDevice, pCommandList, cpuHandle);
            pGO->SetSrvGpuDescriptorHandle(gpuHandle);

            wchar_t buf[256];
            swprintf_s(buf, L"[Network] Applied white material + texture to [%hs] tex=%hs\n",
                pGO->m_pstrFrameName, texturePath);
            OutputDebugString(buf);
        }
        else
        {
            wchar_t buf[256];
            swprintf_s(buf, L"[Network] Applied white material to [%hs] (no new texture, hasTexture=%d)\n",
                pGO->m_pstrFrameName, pGO->HasTexture() ? 1 : 0);
            OutputDebugString(buf);
        }
    }

    if (pGO->m_pChild)   ApplyWhiteMaterialAndTextureToHierarchy(pScene, pDevice, pCommandList, pGO->m_pChild, texturePath);
    if (pGO->m_pSibling) ApplyWhiteMaterialAndTextureToHierarchy(pScene, pDevice, pCommandList, pGO->m_pSibling, texturePath);
}

void NetworkManager::ProcessMonsterSpawn(Scene* pScene, ID3D12Device* pDevice,
                                         ID3D12GraphicsCommandList* pCommandList,
                                         uint64 monsterId, uint32 monsterType,
                                         float x, float y, float z, float yaw,
                                         float hp, bool isBoss)
{
    // 중복 방지
    if (m_mapServerMonsters.find(monsterId) != m_mapServerMonsters.end())
    {
        // 이미 있으면 위치만 갱신
        ProcessMonsterMove(monsterId, x, y, z, yaw);
        return;
    }

    MonsterPreset preset = GetMonsterPresetByType(monsterType);

    // 서버 몬스터는 로컬 Room에 속하지 않는 전역 오브젝트로 생성
    CRoom* pPrevRoom = pScene->GetCurrentRoom();
    pScene->SetCurrentRoom(nullptr);

    GameObject* pMonster = MeshLoader::LoadGeometryFromFile(
        pScene, pDevice, pCommandList, nullptr, preset.meshPath);

    pScene->SetCurrentRoom(pPrevRoom);

    if (!pMonster)
    {
        wchar_t buf[256];
        swprintf_s(buf, L"[Network] ProcessMonsterSpawn: mesh load FAILED type=%u path=%hs\n",
            monsterType, preset.meshPath);
        OutputDebugString(buf);
        return;
    }

    sprintf_s(pMonster->m_pstrFrameName, "NetMonster_%llu", monsterId);

    // 위치/회전/스케일
    TransformComponent* pT = pMonster->GetTransform();
    if (pT)
    {
        pT->SetPosition(x, y, z);
        pT->SetScale(preset.scale, preset.scale, preset.scale);
        // 서버가 yaw를 도(degree)로 보냄 → 그대로 사용 (이중 변환 버그 제거)
        pT->SetRotation(0.0f, yaw, 0.0f);
    }

    // 애니메이션
    auto* pAnim = pMonster->AddComponent<AnimationComponent>();
    if (pAnim)
    {
        pAnim->LoadAnimation(preset.animPath);
        pAnim->Play(preset.idleClip, true);
    }

    // 흰 머티리얼 + 텍스처 강제 적용 (MeshLoader가 .bin에서 세팅 안 했을 경우 대비)
    // → 서버 권위 스폰에서 유일하게 빠져 있던 스텝. EnemySpawner::LoadTextureToHierarchy 미러.
    ApplyWhiteMaterialAndTextureToHierarchy(pScene, pDevice, pCommandList, pMonster, preset.texturePath);

    // 쉐이더 등록 (렌더링)
    Shader* pDefaultShader = pScene->GetDefaultShader();
    if (pDefaultShader)
    {
        pScene->AddRenderComponentsToHierarchy(pDevice, pCommandList, pMonster, pDefaultShader, true);
    }

    // AnimationComponent::BuildBoneCache 호출 포함
    pMonster->Init(pDevice, pCommandList);

    m_mapServerMonsters[monsterId] = pMonster;
    m_mapServerMonsterClips[monsterId] = { preset.idleClip, preset.walkClip };

    // 디버그: 실제 배치된 transform과 preset 클립 확인
    XMFLOAT3 finalPos = pT ? pT->GetPosition() : XMFLOAT3{0,0,0};
    XMFLOAT3 finalRot = pT ? pT->GetRotation() : XMFLOAT3{0,0,0};
    XMFLOAT3 finalSca = pT ? pT->GetScale()    : XMFLOAT3{1,1,1};
    wchar_t buf[512];
    swprintf_s(buf, L"[Network] Spawned NetMonster_%llu type=%u boss=%d hp=%.1f\n"
                    L"  pos=(%.2f,%.2f,%.2f) rot=(%.1f,%.1f,%.1f) scale=(%.2f,%.2f,%.2f)\n"
                    L"  idleClip=%hs walkClip=%hs mesh=%hs\n",
        monsterId, monsterType, isBoss ? 1 : 0, hp,
        finalPos.x, finalPos.y, finalPos.z,
        finalRot.x, finalRot.y, finalRot.z,
        finalSca.x, finalSca.y, finalSca.z,
        preset.idleClip, preset.walkClip, preset.meshPath);
    OutputDebugString(buf);
}

void NetworkManager::ProcessMonsterMove(uint64 monsterId, float x, float y, float z, float yaw)
{
    auto it = m_mapServerMonsters.find(monsterId);
    if (it == m_mapServerMonsters.end())
        return;

    GameObject* pMonster = it->second;
    TransformComponent* pT = pMonster->GetTransform();
    if (pT)
    {
        pT->SetPosition(x, y, z);
        // 서버가 yaw를 도(degree)로 보냄 → 그대로 사용
        XMFLOAT3 rot = pT->GetRotation();
        pT->SetRotation(rot.x, yaw, rot.z);
    }

    // 걷기 애니메이션 부드럽게 전환 — preset별 walk 클립 이름 사용
    auto* pAnim = pMonster->GetComponent<AnimationComponent>();
    if (pAnim)
    {
        auto clipIt = m_mapServerMonsterClips.find(monsterId);
        const char* walkClip = (clipIt != m_mapServerMonsterClips.end())
            ? clipIt->second.walk.c_str() : "Walk";
        pAnim->CrossFade(walkClip, 0.1f, true);
    }

    m_mapServerMonsterMoveTime[monsterId] = 0.0f;
}

void NetworkManager::CheckServerMonsterIdle(float deltaTime)
{
    for (auto it = m_mapServerMonsterMoveTime.begin(); it != m_mapServerMonsterMoveTime.end(); )
    {
        it->second += deltaTime;
        if (it->second >= IDLE_TRANSITION_TIME)
        {
            auto mIt = m_mapServerMonsters.find(it->first);
            if (mIt != m_mapServerMonsters.end())
            {
                auto* pAnim = mIt->second->GetComponent<AnimationComponent>();
                if (pAnim)
                {
                    auto clipIt = m_mapServerMonsterClips.find(it->first);
                    const char* idleClip = (clipIt != m_mapServerMonsterClips.end())
                        ? clipIt->second.idle.c_str() : "Idle";
                    pAnim->CrossFade(idleClip, 0.2f, true);
                }
            }
            it = m_mapServerMonsterMoveTime.erase(it);
        }
        else ++it;
    }
}

void NetworkManager::ProcessMonsterDespawn(Scene* pScene, uint64 monsterId)
{
    auto it = m_mapServerMonsters.find(monsterId);
    if (it == m_mapServerMonsters.end())
        return;

    GameObject* pMonster = it->second;
    pScene->MarkForDeletion(pMonster);
    m_mapServerMonsters.erase(it);
    m_mapServerMonsterMoveTime.erase(monsterId);
    m_mapServerMonsterClips.erase(monsterId);

    wchar_t buf[128];
    swprintf_s(buf, L"[Network] Despawned NetMonster_%llu\n", monsterId);
    OutputDebugString(buf);
}
