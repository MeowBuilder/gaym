#pragma once

#include <Windows.h>
#include <DirectXMath.h>

#define MAX_KEYS 256

class InputSystem
{
public:
    InputSystem(); // Public constructor
    ~InputSystem(); // Public destructor

    // Input handling methods
    void OnKeyDown(int key);
    void OnKeyUp(int key);
    void OnMouseMove(int x, int y);
    void OnMouseWheel(short delta);

    // Reset deltas for the next frame
    void Reset();

    // Query input state
    bool IsKeyDown(int key) const { return m_keyState[key]; }
    float GetMouseDeltaX() const { return m_mouseDeltaX; }
    float GetMouseDeltaY() const { return m_mouseDeltaY; }
    float GetMouseWheelDelta() const { return m_mouseWheelDelta; }

private:
    bool m_keyState[MAX_KEYS];
    int m_mouseX, m_mouseY; // Current mouse position
    float m_mouseDeltaX, m_mouseDeltaY; // Mouse movement delta
    float m_mouseWheelDelta; // Mouse wheel delta
    bool m_firstMouseMove; // Flag for initial mouse position
};
