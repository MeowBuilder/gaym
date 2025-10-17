#include "stdafx.h"
#include "InputSystem.h"

InputSystem::InputSystem()
    : m_mouseX(0), m_mouseY(0),
      m_mouseDeltaX(0.0f), m_mouseDeltaY(0.0f),
      m_mouseWheelDelta(0.0f),
      m_firstMouseMove(true)
{
    ZeroMemory(m_keyState, sizeof(m_keyState));
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

void InputSystem::Reset()
{
    m_mouseDeltaX = 0.0f;
    m_mouseDeltaY = 0.0f;
    m_mouseWheelDelta = 0.0f;
}
