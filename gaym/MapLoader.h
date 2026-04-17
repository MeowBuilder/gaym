#pragma once
#include "stdafx.h"
#include <string>
#include <vector>
#include <unordered_map>

// ─────────────────────────────────────────────────────────────────────────────
//  Minimal JSON value (supports null / bool / number / string / array / object)
// ─────────────────────────────────────────────────────────────────────────────
struct JsonVal
{
    enum class T { Null, Bool, Num, Str, Arr, Obj };
    T    type = T::Null;
    double num = 0.0;
    bool   b   = false;
    std::string str;
    std::vector<JsonVal>                        arr;
    std::unordered_map<std::string, JsonVal>    obj;

    bool   isNull() const { return type == T::Null; }
    float  f()      const { return (float)num; }
    int    i()      const { return (int)num; }
    size_t size()   const { return arr.size(); }

    const JsonVal& operator[](size_t idx)           const { return arr[idx]; }
    const JsonVal& operator[](const std::string& k) const;
    bool has(const std::string& k) const { return obj.count(k) > 0; }

    // Parse a JSON string. Returns a Null value on error.
    static JsonVal parse(const std::string& text);
    // Read file and parse.
    static JsonVal parseFile(const char* path);
};

// ─────────────────────────────────────────────────────────────────────────────
//  MapLoader  –  loads map.json exported from Unity and applies it to Scene.
//
//  What it does:
//    · Creates CRoom instances from the "rooms" array
//    · Sets player start position from "playerSpawn"
//    · Creates GameObjects for each "mapObjects" entry (OBJ mesh + RenderComponent)
//    · Creates invisible GameObjects with ColliderComponent for each "obstacles" entry
//    · Reads "enemySpawns" and builds a RoomSpawnConfig for the first room
//
//  Usage (called once from Scene::Init):
//    MapLoader::LoadIntoScene("Assets/MapData/map.json", this, pDevice, pCmdList, pShader);
// ─────────────────────────────────────────────────────────────────────────────
class Scene;
class Shader;
struct ID3D12Device;
struct ID3D12GraphicsCommandList;

class MapLoader
{
public:
    // positionOffset: 모든 맵 오브젝트 위치에 추가 오프셋 적용 (복제용)
    // skipRoomAndSpawn: 방 생성/플레이어 스폰 재설정을 건너뜀 (기존 맵 위에 중첩 로딩용)
    static bool LoadIntoScene(
        const char*                     jsonPath,
        Scene*                          pScene,
        ID3D12Device*                   pDevice,
        ID3D12GraphicsCommandList*      pCommandList,
        Shader*                         pShader,
        DirectX::XMFLOAT3               positionOffset    = {0.0f, 0.0f, 0.0f},
        bool                            skipRoomAndSpawn  = false);

    // Manually load an OBJ mesh from file (cached)
    static class Mesh* LoadMesh(
        const char*                     path,
        ID3D12Device*                   pDevice,
        ID3D12GraphicsCommandList*      pCommandList);
};
