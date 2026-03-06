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
    static bool LoadIntoScene(
        const char*                     jsonPath,
        Scene*                          pScene,
        ID3D12Device*                   pDevice,
        ID3D12GraphicsCommandList*      pCommandList,
        Shader*                         pShader);
};
