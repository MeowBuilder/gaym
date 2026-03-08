# 후처리 파이프라인 시스템 설계 문서

## 목표
카툰 렌더링, 블룸, 블러 등 후처리 효과를 쉽게 추가할 수 있는 렌더링 시스템 구축

## 렌더링 흐름 변경

**현재:**
```
3D Scene → BackBuffer → Present
```

**변경 후:**
```
3D Scene → Offscreen RT → [후처리 패스들] → BackBuffer → Present
```

---

## 생성할 파일

### 1. RenderTarget.h/cpp
렌더 타겟 리소스 래퍼 클래스

```cpp
class RenderTarget {
public:
    void Create(ID3D12Device* pDevice, UINT width, UINT height, DXGI_FORMAT format,
                D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle,
                D3D12_CPU_DESCRIPTOR_HANDLE srvCpuHandle,
                D3D12_GPU_DESCRIPTOR_HANDLE srvGpuHandle);
    void Resize(ID3D12Device* pDevice, UINT width, UINT height);
    void TransitionTo(ID3D12GraphicsCommandList* pCmdList, D3D12_RESOURCE_STATES newState);
    void Release();

    ID3D12Resource* GetResource() const;
    D3D12_CPU_DESCRIPTOR_HANDLE GetRTV() const;
    D3D12_GPU_DESCRIPTOR_HANDLE GetSRVGpu() const;
    UINT GetWidth() const;
    UINT GetHeight() const;

private:
    ComPtr<ID3D12Resource> m_pResource;
    D3D12_CPU_DESCRIPTOR_HANDLE m_RtvHandle;
    D3D12_CPU_DESCRIPTOR_HANDLE m_SrvCpuHandle;
    D3D12_GPU_DESCRIPTOR_HANDLE m_SrvGpuHandle;
    DXGI_FORMAT m_Format;
    UINT m_Width, m_Height;
    D3D12_RESOURCE_STATES m_CurrentState;
    float m_ClearColor[4];
};
```

### 2. IPostProcessPass.h
후처리 패스 인터페이스

```cpp
class IPostProcessPass {
public:
    virtual ~IPostProcessPass() = default;
    virtual const std::string& GetName() const = 0;
    virtual bool IsEnabled() const = 0;
    virtual void SetEnabled(bool enabled) = 0;
    virtual void Init(ID3D12Device* pDevice, const DirectX::RenderTargetState& rtState) = 0;
    virtual void Process(ID3D12GraphicsCommandList* pCmdList,
                        D3D12_GPU_DESCRIPTOR_HANDLE srcSRV,
                        D3D12_CPU_DESCRIPTOR_HANDLE dstRTV,
                        UINT width, UINT height) = 0;
    virtual void OnResize(ID3D12Device* pDevice, UINT width, UINT height) {}

protected:
    std::string m_Name;
    bool m_bEnabled = true;
};
```

### 3. PostProcessManager.h/cpp
후처리 시스템 통합 관리자

```cpp
class PostProcessManager {
public:
    void Init(ID3D12Device* pDevice, ID3D12CommandQueue* pCmdQueue,
              UINT width, UINT height,
              DXGI_FORMAT sceneFormat = DXGI_FORMAT_R8G8B8A8_UNORM,
              DXGI_FORMAT depthFormat = DXGI_FORMAT_D24_UNORM_S8_UINT);
    void OnResize(ID3D12Device* pDevice, UINT width, UINT height);

    // 렌더링 흐름
    D3D12_CPU_DESCRIPTOR_HANDLE BeginSceneRender(ID3D12GraphicsCommandList* pCmdList,
                                                  D3D12_CPU_DESCRIPTOR_HANDLE* outDSV = nullptr);
    void EndSceneRender(ID3D12GraphicsCommandList* pCmdList);
    void Execute(ID3D12GraphicsCommandList* pCmdList,
                 D3D12_CPU_DESCRIPTOR_HANDLE backBufferRTV,
                 UINT backBufferWidth, UINT backBufferHeight);

    // 패스 관리
    void AddPass(std::unique_ptr<IPostProcessPass> pPass);
    void RemovePass(const std::string& name);
    void SetPassEnabled(const std::string& name, bool enabled);
    IPostProcessPass* GetPass(const std::string& name);

    void SetEnabled(bool enabled);
    bool IsEnabled() const;

private:
    RenderTarget m_SceneRT;           // 씬 렌더링용
    RenderTarget m_IntermediateRT[2]; // 핑퐁용
    ComPtr<ID3D12Resource> m_pDepthBuffer;

    std::unique_ptr<DirectX::DescriptorHeap> m_pSrvHeap;  // SRV용
    ComPtr<ID3D12DescriptorHeap> m_pRtvHeap;              // RTV용
    ComPtr<ID3D12DescriptorHeap> m_pDsvHeap;              // DSV용

    std::vector<std::unique_ptr<IPostProcessPass>> m_vPasses;
    std::unique_ptr<DirectX::BasicPostProcess> m_pCopyPass;  // 최종 복사용

    bool m_bEnabled = true;
    UINT m_Width, m_Height;
    DXGI_FORMAT m_SceneFormat, m_DepthFormat;
};
```

---

## 수정할 파일

### Dx12App.h
```cpp
#include "PostProcessManager.h"

class Dx12App {
    // 추가
    std::unique_ptr<PostProcessManager> m_pPostProcessManager;
};
```

### Dx12App.cpp

**OnCreate():**
```cpp
m_pPostProcessManager = std::make_unique<PostProcessManager>();
m_pPostProcessManager->Init(m_pd3dDevice.Get(), m_pd3dCommandQueue.Get(),
                            m_nWndClientWidth, m_nWndClientHeight);
```

**FrameAdvance() 변경:**
```cpp
D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle, dsvHandle;

if (m_pPostProcessManager && m_pPostProcessManager->IsEnabled()) {
    // 오프스크린 RT로 렌더링
    rtvHandle = m_pPostProcessManager->BeginSceneRender(m_pd3dCommandList.Get(), &dsvHandle);
} else {
    // 기존: 백버퍼 직접 렌더링
    rtvHandle = ... // 백버퍼 RTV
    dsvHandle = ... // 기존 DSV
    // 리소스 배리어 (PRESENT → RENDER_TARGET)
}

// Clear & Set RTV
m_pd3dCommandList->ClearRenderTargetView(rtvHandle, clearColor, 0, NULL);
m_pd3dCommandList->ClearDepthStencilView(dsvHandle, ...);
m_pd3dCommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

// Scene 렌더링
m_pScene->Render(m_pd3dCommandList.Get());

// 후처리 실행
if (m_pPostProcessManager && m_pPostProcessManager->IsEnabled()) {
    m_pPostProcessManager->EndSceneRender(m_pd3dCommandList.Get());

    // 백버퍼 배리어 (PRESENT → RENDER_TARGET)
    D3D12_CPU_DESCRIPTOR_HANDLE backBufferRTV = ...;
    m_pPostProcessManager->Execute(m_pd3dCommandList.Get(), backBufferRTV,
                                   m_nWndClientWidth, m_nWndClientHeight);
}

// Text 렌더링 (백버퍼에 직접)
RenderText();
```

**OnResize():**
```cpp
if (m_pPostProcessManager) {
    m_pPostProcessManager->OnResize(m_pd3dDevice.Get(), nWidth, nHeight);
}
```

---

## 디스크립터 힙 구성

### PostProcessManager 전용 힙

**RTV 힙 (3슬롯):**
| 슬롯 | 용도 |
|------|------|
| 0 | SceneRT |
| 1 | IntermediateRT[0] |
| 2 | IntermediateRT[1] |

**SRV 힙 (Shader-Visible, 16슬롯 여유):**
| 슬롯 | 용도 |
|------|------|
| 0 | SceneRT SRV |
| 1 | IntermediateRT[0] SRV |
| 2 | IntermediateRT[1] SRV |
| 3+ | 패스 내부 RT용 (BloomPass 등) |

**DSV 힙 (1슬롯):**
| 슬롯 | 용도 |
|------|------|
| 0 | 씬 렌더링용 뎁스 버퍼 |

---

## 구현 순서

1. **RenderTarget.h/cpp** - RT 래퍼 구현
2. **IPostProcessPass.h** - 인터페이스 정의
3. **PostProcessManager.h/cpp** - 기본 흐름 (패스 없이 단순 복사)
4. **Dx12App 수정** - 렌더링 흐름 통합
5. **테스트** - 후처리 없이 화면 정상 출력 확인

---

## 향후 확장 (별도 구현)

### BasicPostProcessPass
DirectXTK12 BasicPostProcess 래퍼
- GaussianBlur_5x5, BloomExtract, BloomBlur 등

### BloomPass
복합 블룸 효과 (여러 BasicPostProcess 조합)
- Extract → DownScale → BlurH → BlurV → Combine

### CustomPostProcessPass
커스텀 셰이더용 베이스 클래스
- 풀스크린 쿼드 VS + 커스텀 PS
- 상수 버퍼로 파라미터 전달

### PostProcessShaders.hlsl
커스텀 후처리 셰이더
- 카툰 렌더링 (셀 셰이딩)
- 엣지 검출 (Sobel 필터)
- 비네트

---

## DirectXTK12 PostProcess 참조

프로젝트에 이미 포함된 DirectXTK12 패키지:
`gaym\packages\directxtk12_desktop_2019.2025.10.28.1\include\PostProcess.h`

**BasicPostProcess 효과:**
- Copy, Monochrome, Sepia
- DownScale_2x2, DownScale_4x4
- GaussianBlur_5x5
- BloomExtract, BloomBlur

**DualPostProcess 효과:**
- Merge, BloomCombine

**ToneMapPostProcess:**
- None, Saturate, Reinhard, ACESFilmic
