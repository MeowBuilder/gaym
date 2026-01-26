#pragma once
#include <cstdint>

// Collision Layer (Bit Flags)
enum class CollisionLayer : uint32_t
{
    None         = 0,
    Player       = 1 << 0,  // 0x01
    Enemy        = 1 << 1,  // 0x02
    PlayerBullet = 1 << 2,  // 0x04
    EnemyBullet  = 1 << 3,  // 0x08
    Wall         = 1 << 4,  // 0x10
    Pickup       = 1 << 5,  // 0x20
    Trigger      = 1 << 6,  // 0x40
    All          = 0xFFFFFFFF
};

// Bitwise operators for CollisionLayer
constexpr CollisionLayer operator|(CollisionLayer a, CollisionLayer b)
{
    return static_cast<CollisionLayer>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

constexpr CollisionLayer operator&(CollisionLayer a, CollisionLayer b)
{
    return static_cast<CollisionLayer>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

constexpr CollisionLayer operator~(CollisionLayer a)
{
    return static_cast<CollisionLayer>(~static_cast<uint32_t>(a));
}

inline CollisionLayer& operator|=(CollisionLayer& a, CollisionLayer b)
{
    a = a | b;
    return a;
}

inline CollisionLayer& operator&=(CollisionLayer& a, CollisionLayer b)
{
    a = a & b;
    return a;
}

// Helper function to check if a layer mask contains a specific layer
constexpr bool HasLayer(CollisionLayer mask, CollisionLayer layer)
{
    return (static_cast<uint32_t>(mask) & static_cast<uint32_t>(layer)) != 0;
}

// Predefined collision masks
namespace CollisionMask
{
    // Player collides with: Enemy, EnemyBullet, Wall, Pickup, Trigger
    constexpr CollisionLayer Player =
        CollisionLayer::Enemy |
        CollisionLayer::EnemyBullet |
        CollisionLayer::Wall |
        CollisionLayer::Pickup |
        CollisionLayer::Trigger;

    // Enemy collides with: Player, PlayerBullet, Wall, Enemy (for pushing)
    constexpr CollisionLayer Enemy =
        CollisionLayer::Player |
        CollisionLayer::PlayerBullet |
        CollisionLayer::Wall |
        CollisionLayer::Enemy;

    // PlayerBullet collides with: Enemy, Wall
    constexpr CollisionLayer PlayerBullet =
        CollisionLayer::Enemy |
        CollisionLayer::Wall;

    // EnemyBullet collides with: Player, Wall
    constexpr CollisionLayer EnemyBullet =
        CollisionLayer::Player |
        CollisionLayer::Wall;

    // Wall collides with: Player, Enemy, PlayerBullet, EnemyBullet
    constexpr CollisionLayer Wall =
        CollisionLayer::Player |
        CollisionLayer::Enemy |
        CollisionLayer::PlayerBullet |
        CollisionLayer::EnemyBullet;

    // Pickup collides with: Player only
    constexpr CollisionLayer Pickup =
        CollisionLayer::Player;

    // Trigger collides with: Player only
    constexpr CollisionLayer Trigger =
        CollisionLayer::Player;
}
