#pragma once

#include <DirectXMath.h>
#include <cmath>

using namespace DirectX;

namespace MathUtils
{
    // 2D distance (XZ plane only, ignores Y)
    inline float Distance2D(const XMFLOAT3& a, const XMFLOAT3& b)
    {
        float dx = a.x - b.x;
        float dz = a.z - b.z;
        return sqrtf(dx * dx + dz * dz);
    }

    // 3D distance
    inline float Distance3D(const XMFLOAT3& a, const XMFLOAT3& b)
    {
        float dx = a.x - b.x;
        float dy = a.y - b.y;
        float dz = a.z - b.z;
        return sqrtf(dx * dx + dy * dy + dz * dz);
    }

    // 2D distance squared (faster, use when comparing distances)
    inline float DistanceSq2D(const XMFLOAT3& a, const XMFLOAT3& b)
    {
        float dx = a.x - b.x;
        float dz = a.z - b.z;
        return dx * dx + dz * dz;
    }

    // 3D distance squared (faster, use when comparing distances)
    inline float DistanceSq3D(const XMFLOAT3& a, const XMFLOAT3& b)
    {
        float dx = a.x - b.x;
        float dy = a.y - b.y;
        float dz = a.z - b.z;
        return dx * dx + dy * dy + dz * dz;
    }

    // Normalize 2D direction (XZ plane)
    inline XMFLOAT2 Normalize2D(float dx, float dz)
    {
        float length = sqrtf(dx * dx + dz * dz);
        if (length < 0.0001f) return XMFLOAT2(0.0f, 0.0f);
        return XMFLOAT2(dx / length, dz / length);
    }

    // Get direction from a to b (XZ plane)
    inline XMFLOAT2 Direction2D(const XMFLOAT3& from, const XMFLOAT3& to)
    {
        float dx = to.x - from.x;
        float dz = to.z - from.z;
        return Normalize2D(dx, dz);
    }
}
