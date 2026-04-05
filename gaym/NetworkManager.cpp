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

    // 타 플레이어 스킬 VFX 생성/업데이트
    FluidSkillVFXManager* pVFXManager = pScene ? pScene->GetFluidVFXManager() : nullptr;
    if (pVFXManager)
    {
        // skillType을 SkillSlot으로 변환 (1=Q, 2=E, 3=R, 4=RightClick)
        SkillSlot slot = SkillSlot::Q;
        switch (skillType)
        {
        case 1: slot = SkillSlot::Q; break;
        case 2: slot = SkillSlot::E; break;
        case 3: slot = SkillSlot::R; break;
        case 4: slot = SkillSlot::RightClick; break;
        default: slot = SkillSlot::Q; break;
        }

        // VFX 원점 (캐릭터 위치 + 높이 오프셋)
        XMFLOAT3 vfxOrigin = XMFLOAT3(x, y + 1.5f, z);
        XMFLOAT3 vfxDirection = XMFLOAT3(dirX, dirY, dirZ);

        // 기존 VFX 상태 확인
        auto vfxIt = m_mapRemotePlayerVFX.find(playerId);
        bool hasExistingVFX = (vfxIt != m_mapRemotePlayerVFX.end() && vfxIt->second.vfxId >= 0);

        if (hasExistingVFX && vfxIt->second.skillType == skillType)
        {
            // 같은 스킬 계속 사용 중 → TrackEffect로 방향 업데이트
            pVFXManager->TrackEffect(vfxIt->second.vfxId, vfxOrigin, vfxDirection);
            vfxIt->second.lastUpdateTime = 0.0f;  // 타이머 리셋
        }
        else
        {
            // 새 스킬이거나 기존 VFX 없음 → 기존 VFX 종료 후 새로 생성
            if (hasExistingVFX)
            {
                pVFXManager->StopEffect(vfxIt->second.vfxId);
            }

            // 기본 VFX 정의 가져오기 (룬 없음, Fire 속성)
            VFXSequenceDef seqDef = VFXLibrary::Get().GetDef(slot, RUNE_NONE, ElementType::Fire);

            // VFX 생성
            int vfxId = pVFXManager->SpawnSequenceEffect(vfxOrigin, vfxDirection, seqDef);

            // 상태 저장
            RemoteVFXState state;
            state.vfxId = vfxId;
            state.skillType = skillType;
            state.lastUpdateTime = 0.0f;
            m_mapRemotePlayerVFX[playerId] = state;

            wchar_t vfxBuf[128];
            swprintf_s(vfxBuf, L"[Network] Spawned VFX for remote player skill: vfxId=%d\n", vfxId);
            OutputDebugString(vfxBuf);
        }
    }

    wchar_t buf[128];
    swprintf_s(buf, L"[Network] ProcessSkill: PlayerId=%llu SkillType=%d\n", playerId, skillType);
    OutputDebugString(buf);
}
