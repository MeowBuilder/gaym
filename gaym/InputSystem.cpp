#include "stdafx.h"
#include "InputSystem.h"

InputSystem::InputSystem()
    : m_mouseX(0), m_mouseY(0),
      m_mouseDeltaX(0.0f), m_mouseDeltaY(0.0f),
      m_mouseWheelDelta(0.0f),
      m_firstMouseMove(true)
{
    ZeroMemory(m_keyState, sizeof(m_keyState));
    ZeroMemory(m_prevKeyState, sizeof(m_prevKeyState));
    ZeroMemory(m_mouseButtonState, sizeof(m_mouseButtonState));
    ZeroMemory(m_prevMouseButtonState, sizeof(m_prevMouseButtonState));
}

InputSystem::~InputSystem()
{
}

void InputSystem::OnKeyDown(int key)
{
    if (key >= 0 && key < MAX_KEYS)
    {
        m_keyState[key] = true;
    }
}

void InputSystem::OnKeyUp(int key)
{
    if (key >= 0 && key < MAX_KEYS)
    {
        m_keyState[key] = false;
    }
}

void InputSystem::OnMouseMove(int x, int y)
{
    if (m_firstMouseMove)
    {
        m_mouseX = x;
        m_mouseY = y;
        m_firstMouseMove = false;
    }
    else
    {
        m_mouseDeltaX = (float)(x - m_mouseX);
        m_mouseDeltaY = (float)(y - m_mouseY);
        m_mouseX = x;
        m_mouseY = y;
    }
}

void InputSystem::OnMouseWheel(short delta)
{
    m_mouseWheelDelta = (float)delta / WHEEL_DELTA;
}

void InputSystem::OnMouseButtonDown(int button)
{
    if (button >= 0 && button < MAX_MOUSE_BUTTONS)
    {
        m_mouseButtonState[button] = true;
    }
}

void InputSystem::OnMouseButtonUp(int button)
{
    if (button >= 0 && button < MAX_MOUSE_BUTTONS)
    {
        m_mouseButtonState[button] = false;
    }
}

bool InputSystem::IsMouseButtonDown(int button) const
{
    if (button >= 0 && button < MAX_MOUSE_BUTTONS)
    {
        return m_mouseButtonState[button];
    }
    return false;
}

bool InputSystem::IsMouseButtonPressed(int button) const
{
    if (button >= 0 && button < MAX_MOUSE_BUTTONS)
    {
        // Pressed = currently down AND was not down last frame
        return m_mouseButtonState[button] && !m_prevMouseButtonState[button];
    }
    return false;
}

void InputSystem::Reset()
{
    m_mouseDeltaX = 0.0f;
    m_mouseDeltaY = 0.0f;
    m_mouseWheelDelta = 0.0f;

    // Save current state as previous for next frame
    memcpy(m_prevKeyState, m_keyState, sizeof(m_keyState));
    memcpy(m_prevMouseButtonState, m_mouseButtonState, sizeof(m_mouseButtonState));
}
