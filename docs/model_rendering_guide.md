# gaym 엔진: 새 3D 모델을 장면에 추가하는 방법 가이드

이 가이드는 `gaym` 프로젝트에서 `.bin` 형식의 3D 모델 파일을 로드하고 장면에 추가하는 방법을 단계별로 설명합니다.

## 1단계: `Scene.cpp` 파일 열기

장면의 모든 초기화 로직은 `Scene.cpp` 파일의 `Scene::Init` 함수에 있습니다. 이 파일을 엽니다.

## 2단계: `Scene::Init` 함수에 코드 추가

`Scene::Init` 함수 내에서, 기존에 추가된 모델 로딩 코드(예: `// Add two static Apache models for testing` 블록)를 참고하여 다음과 같은 패턴으로 코드를 추가합니다.

```cpp
// Scene::Init 함수 내, 적절한 위치에 추가

{ // 새 모델을 위한 지역 스코프를 생성하여 변수 충돌을 방지합니다.

    // 1. 모델을 제어할 최상위 GameObject를 생성하고 Scene에 추가합니다.
    //    이 GameObject가 로드된 모델 전체의 '뿌리' 역할을 합니다.
    GameObject* pMyNewObject = CreateGameObject(pDevice, pCommandList);
    m_vGameObjects.push_back(std::unique_ptr<GameObject>(pMyNewObject));

    // 2. 최상위 객체의 초기 위치를 설정합니다.
    //    이 객체의 위치를 변경하면 로드된 모델 전체가 함께 움직입니다.
    pMyNewObject->GetTransform()->SetPosition(0.0f, 5.0f, 10.0f); // 예시: (x, y, z) 위치

    // 3. MeshLoader를 사용하여 .bin 파일에서 모델 계층 구조를 로드합니다.
    //    "Model/Gunship.bin" 부분을 원하는 모델 파일명으로 변경하세요.
    GameObject* pLoadedModel = MeshLoader::LoadGeometryFromFile(this, pDevice, pCommandList, NULL, "Model/Gunship.bin");

    // 4. 모델이 성공적으로 로드되었는지 확인하고, 로드된 모델을 최상위 객체의 자식으로 연결합니다.
    //    이렇게 하면 최상위 객체가 로드된 모델 전체를 제어할 수 있게 됩니다.
    if (pLoadedModel)
    {
        pMyNewObject->SetChild(pLoadedModel);
        
        // 5. 로드된 모델의 모든 부분(메쉬)에 RenderComponent를 추가하여 렌더링 파이프라인에 등록합니다.
        //    이 함수를 호출해야만 모델의 각 부분이 화면에 그려집니다.
        //    pShader.get()은 현재 장면에 사용되는 기본 셰이더를 의미합니다.
        AddRenderComponentsToHierarchy(pDevice, pCommandList, pLoadedModel, pShader.get());

        // (선택 사항) 모델의 기본 색상을 설정합니다.
        // pMyNewObject->SetBaseColor(XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f)); // 예시: 빨간색
    }
    else
    {
        OutputDebugString(L"Failed to load model: Model/Gunship.bin\n");
    }
}
```

**핵심 개념:**

*   **최상위 `GameObject`**: 로드된 모델 전체를 제어하는 '컨테이너' 역할을 합니다. 이 객체의 변환(위치, 회전, 크기)을 변경하면 모델 전체가 함께 변환됩니다.
*   **`MeshLoader::LoadGeometryFromFile`**: 지정된 `.bin` 파일에서 모델의 복잡한 계층 구조를 파싱하고, 각 부분을 `GameObject` 객체들로 구성하여 반환합니다.
*   **`GameObject::SetChild`**: 로드된 모델의 루트 `GameObject`를 우리가 만든 최상위 `GameObject`의 자식으로 연결합니다.
*   **`Scene::AddRenderComponentsToHierarchy`**: 로드된 모델의 모든 `GameObject`들을 재귀적으로 순회하며, 메쉬를 가진 객체에 `RenderComponent`를 추가하고 렌더링 파이프라인에 등록하는 필수적인 단계입니다.

이 가이드를 통해 `gaym` 엔진에 새로운 3D 모델을 쉽게 추가하고 렌더링할 수 있습니다.
