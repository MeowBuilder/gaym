#pragma once
#include "stdafx.h"
#include <SpriteBatch.h>
#include <SpriteFont.h>
#include <vector>

struct DamageNumber
{
    DirectX::XMFLOAT3 worldPos;
    float damage;
    float lifetime;     // counts down from LIFETIME to 0
    float riseOffset;   // accumulated upward offset (world units)
};

class DamageNumberManager
{
public:
    static DamageNumberManager& Get();

    void AddNumber(const DirectX::XMFLOAT3& worldPos, float damage);
    void Update(float dt);
    void Render(DirectX::SpriteBatch* pBatch, DirectX::SpriteFont* pFont,
                const DirectX::XMFLOAT4X4& viewProj, int screenW, int screenH);

private:
    DamageNumberManager() = default;

    static constexpr float LIFETIME   = 1.0f;   // seconds a number lives
    static constexpr float RISE_SPEED = 2.5f;   // world units per second upward

    std::vector<DamageNumber> m_numbers;
};
