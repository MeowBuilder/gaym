#pragma once

#include <Windows.h>
#include <DirectXMath.h>

#define MAX_KEYS 256
#define MAX_MOUSE_BUTTONS 3

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
    void OnMouseButtonDown(int button);
    void OnMouseButtonUp(int button);

    // Reset deltas for the next frame
    void Reset();

    // Query input state
    bool IsKeyDown(int key) const { return m_keyState[key]; }
    float GetMouseDeltaX() const { return m_mouseDeltaX; }
    float GetMouseDeltaY() const { return m_mouseDeltaY; }
    float GetMouseWheelDelta() const { return m_mouseWheelDelta; }
    DirectX::XMFLOAT2 GetMousePosition() const { return DirectX::XMFLOAT2((float)m_mouseX, (float)m_mouseY); }

    // Mouse button queries (0 = Left, 1 = Right, 2 = Middle)
    bool IsMouseButtonDown(int button) const;
    bool IsMouseButtonPressed(int button) const;  // True only on the frame the button was pressed

private:
    bool m_keyState[MAX_KEYS];
    bool m_mouseButtonState[MAX_MOUSE_BUTTONS]; // Left, Right, Middle
    bool m_prevMouseButtonState[MAX_MOUSE_BUTTONS]; // Previous frame state
    int m_mouseX, m_mouseY; // Current mouse position
    float m_mouseDeltaX, m_mouseDeltaY; // Mouse movement delta
    float m_mouseWheelDelta; // Mouse wheel delta
    bool m_firstMouseMove; // Flag for initial mouse position
};
