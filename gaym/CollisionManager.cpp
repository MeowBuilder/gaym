#include "stdafx.h"
#include "CollisionManager.h"
#include "ColliderComponent.h"
#include <algorithm>

CollisionManager::CollisionManager()
{
}

CollisionManager::~CollisionManager()
{
}

void CollisionManager::RegisterCollider(ColliderComponent* pCollider)
{
    if (pCollider)
    {
        // Avoid duplicates
        auto it = std::find(m_registeredColliders.begin(), m_registeredColliders.end(), pCollider);
        if (it == m_registeredColliders.end())
        {
            m_registeredColliders.push_back(pCollider);
        }
    }
}

void CollisionManager::UnregisterCollider(ColliderComponent* pCollider)
{
    if (pCollider)
    {
        auto it = std::find(m_registeredColliders.begin(), m_registeredColliders.end(), pCollider);
        if (it != m_registeredColliders.end())
        {
            m_registeredColliders.erase(it);
        }
    }
}

void CollisionManager::Update(const std::vector<ColliderComponent*>& globalColliders,
                              const std::vector<ColliderComponent*>& roomColliders)
{
    // Swap current to previous, clear current
    m_previousFrameCollisions = std::move(m_currentFrameCollisions);
    m_currentFrameCollisions.clear();

    // Combine all colliders for checking
    std::vector<ColliderComponent*> allColliders;
    allColliders.reserve(globalColliders.size() + roomColliders.size());
    allColliders.insert(allColliders.end(), globalColliders.begin(), globalColliders.end());
    allColliders.insert(allColliders.end(), roomColliders.begin(), roomColliders.end());

    // Check all pairs (O(n^2) - can be optimized with spatial partitioning later)
    for (size_t i = 0; i < allColliders.size(); ++i)
    {
        for (size_t j = i + 1; j < allColliders.size(); ++j)
        {
            CheckCollision(allColliders[i], allColliders[j]);
        }
    }

    // Check for collision exits (pairs that were colliding last frame but not this frame)
    for (const auto& pair : m_previousFrameCollisions)
    {
        if (m_currentFrameCollisions.find(pair) == m_currentFrameCollisions.end())
        {
            // Find the colliders by ID
            ColliderComponent* pA = nullptr;
            ColliderComponent* pB = nullptr;

            for (auto* pCollider : allColliders)
            {
                if (pCollider->GetColliderID() == pair.id1) pA = pCollider;
                if (pCollider->GetColliderID() == pair.id2) pB = pCollider;
                if (pA && pB) break;
            }

            // Notify exit
            if (pA && pB)
            {
                pA->NotifyCollisionExit(pB);
                pB->NotifyCollisionExit(pA);
            }
        }
    }
}

void CollisionManager::CheckCollision(ColliderComponent* pA, ColliderComponent* pB)
{
    if (!pA || !pB)
        return;

    // Skip disabled colliders
    if (!pA->IsEnabled() || !pB->IsEnabled())
        return;

    // Check layer compatibility
    if (!pA->ShouldCollideWith(*pB))
        return;

    // Check physical intersection
    if (!pA->Intersects(*pB))
        return;

    // Collision detected!
    CollisionPair pair(pA->GetColliderID(), pB->GetColliderID());
    m_currentFrameCollisions.insert(pair);

    // Check if this is a new collision or ongoing
    bool wasColliding = (m_previousFrameCollisions.find(pair) != m_previousFrameCollisions.end());

    if (wasColliding)
    {
        // Collision Stay
        pA->NotifyCollisionStay(pB);
        pB->NotifyCollisionStay(pA);
    }
    else
    {
        // Collision Enter
        pA->NotifyCollisionEnter(pB);
        pB->NotifyCollisionEnter(pA);
    }
}

void CollisionManager::ClearCollisionState()
{
    m_previousFrameCollisions.clear();
    m_currentFrameCollisions.clear();
}
