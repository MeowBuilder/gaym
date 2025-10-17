## Refactoring Summary: Third-Person Player and Camera Implementation

**Overall Goal:** To implement a controllable third-person player character and a camera system that follows and orbits around the player, with movement relative to the camera's orientation.

**Key Architectural Decisions:**
*   **Component-Based Design:** Adhering to the existing component-based architecture for player functionality.
*   **Non-Singleton InputSystem:** The `InputSystem` was designed as a regular class, managed by `Dx12App` and passed to `CScene` for dependency injection, avoiding global state.
*   **Camera-Relative Player Movement:** Player movement (WASD) is calculated based on the camera's forward and right vectors, providing intuitive controls.
*   **Separation of Concerns:** General `GameObject` and `Component` updates do not directly receive `InputSystem*`. Only player-specific logic (`PlayerComponent::PlayerUpdate`) interacts with the `InputSystem` and `CCamera`.

---

### **1. Camera System Implementation**

*   **`Camera.h` & `Camera.cpp` Creation:**
    *   Created `CCamera` class to manage camera position, rotation, view, and projection matrices.
    *   Initial implementation included `SetLens`, `Update` (for mouse input and orbit calculation), and `UpdateViewMatrix`.
    *   **Correction:** Renamed `CGameObject` to `GameObject` in `Camera.h`'s forward declaration to resolve a type mismatch error.
    *   **Enhancement:** Added `GetLookDirection()` and `GetRightDirection()` methods to `CCamera.h` and implemented them in `CCamera.cpp` to extract camera's orientation vectors from its view matrix.

### **2. Input System Implementation**

*   **`InputSystem.h` & `InputSystem.cpp` Creation:**
    *   Created `InputSystem` class (non-singleton) to capture and manage keyboard (`IsKeyDown`, `OnKeyDown`, `OnKeyUp`) and mouse input (`OnMouseMove`, `OnMouseWheel`, `GetMouseDeltaX/Y`, `GetMouseWheelDelta`).
    *   Includes a `Reset()` method to clear input deltas each frame.

### **3. Integration of InputSystem into Dx12App**

*   **`Dx12App.h` Modification:**
    *   Included `InputSystem.h`.
    *   Added `InputSystem m_inputSystem;` as a private member.
    *   Added `InputSystem& GetInputSystem() { return m_inputSystem; }` as a public getter.
*   **`gaym.cpp` Modification (WndProc):**
    *   Modified the global `WndProc` function to capture `WM_KEYDOWN`, `WM_KEYUP`, `WM_MOUSEMOVE`, and `WM_MOUSEWHEEL` messages.
    *   These messages are now passed to `g_pDx12App->GetInputSystem().On...()` methods for processing.
*   **`Dx12App.cpp` Modification:**
    *   Added `m_inputSystem.Reset();` at the end of `Dx12App::FrameAdvance()` to clear input deltas for the next frame.

### **4. Integration of Camera and InputSystem into CScene**

*   **`CScene.h` Modification:**
    *   Included `Camera.h` and `InputSystem.h`.
    *   Added `std::unique_ptr<CCamera> m_pCamera;` as a member.
    *   Added `CCamera* GetCamera() const { return m_pCamera.get(); }` as a getter.
    *   Removed old `XMFLOAT4X4 m_xmf4x4View` and `m_xmf4x4Projection` members.
    *   Changed `void Update(float deltaTime);` to `void Update(float deltaTime, InputSystem* pInputSystem);` to pass input to the scene's update logic.
    *   Added `GameObject* m_pPlayerGameObject = nullptr;` to store a pointer to the player object.
*   **`CScene.cpp` Modification:**
    *   **Constructor:** Initialized `m_pCamera = std::make_unique<CCamera>();`.
    *   **`Init()`:**
        *   Called `m_pCamera->SetLens()` to set up the projection matrix.
        *   **Player Creation:** Created a new `GameObject` for the player, added `RenderComponent` and `PlayerComponent` to it, and assigned it to `m_pPlayerGameObject`.
        *   Set `m_pCamera->SetTarget(m_pPlayerGameObject);` to make the camera follow the player.
    *   **`Update()`:**
        *   Called `m_pCamera->Update()` with mouse deltas and scroll delta from `pInputSystem`.
        *   Updated `PassConstants` with `m_pCamera->GetViewMatrix()` and `m_pCamera->GetProjectionMatrix()`.
        *   **Player Update:** Added a specific call to `m_pPlayerGameObject->GetComponent<PlayerComponent>()->PlayerUpdate(deltaTime, pInputSystem, m_pCamera.get());` to handle player-specific input and movement.
*   **`Dx12App.cpp` Modification:**
    *   Modified the call to `m_pScene->Update()` in `Dx12App::FrameAdvance()` to pass `&m_inputSystem`.

### **5. Player Component and Movement Logic**

*   **`Component.h` Reversion:** Reverted `virtual void Update(float deltaTime, InputSystem* pInputSystem) {}` back to `virtual void Update(float deltaTime) {}` as per user request for cleaner design.
*   **`GameObject.h` & `GameObject.cpp` Reversion:** Reverted `Update` method signatures and component update calls back to `Update(float deltaTime)` to avoid passing `InputSystem*` to all objects.
*   **`PlayerComponent.h` Modification:**
    *   Added `class CCamera;` forward declaration.
    *   Added `void PlayerUpdate(float deltaTime, InputSystem* pInputSystem, CCamera* pCamera);` method for player-specific input-driven updates.
*   **`PlayerComponent.cpp` Implementation:**
    *   Implemented `PlayerUpdate` method.
    *   **Camera-Relative Movement:** Used `pCamera->GetLookDirection()` and `pCamera->GetRightDirection()` to calculate movement displacement relative to the camera's orientation.
    *   **Horizontal Movement Restriction:** Zeroed out the Y component of the camera's `forward` and `right` vectors and re-normalized them to ensure player movement is restricted to the X-Z plane.
    *   Handled player rotation (yaw) based on mouse X input, applying it to the player's `TransformComponent`.
*   **`TransformComponent.h` & `TransformComponent.cpp` Modification:**
    *   Added `XMVECTOR GetLook() const;`, `XMVECTOR GetRight() const;`, `XMVECTOR GetUp() const;` methods to `TransformComponent.h` and implemented them in `TransformComponent.cpp` to extract direction vectors from the world matrix.
    *   Added `void Rotate(float pitch, float yaw, float roll);` method to `TransformComponent.h` and implemented it in `TransformComponent.cpp` to update Euler angles and rebuild the world matrix.