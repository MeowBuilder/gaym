#pragma once

#include "stdafx.h"

// Screen-Space Fluid Renderer (Sebastian Lague 스타일)
// Pass 1: Sphere depth + Thickness -> FluidDepthRT (R32_FLOAT) + ThicknessRT (R16_FLOAT, additive)
// Pass 2: Bilateral smooth -> SmoothedDepthRT
// Pass 3: Beer-Lambert absorption + Schlick Fresnel + Pseudo-refraction composite -> main RT
class ScreenSpaceFluid
{
public:
    ScreenSpaceFluid() = default;
    ~ScreenSpaceFluid();

    void Init(ID3D12Device* pDevice, UINT width, UINT height);
    void OnResize(ID3D12Device* pDevice, UINT width, UINT height);

    // 메인 RT 캡처: BeginDepthPass 전에 호출 (굴절 배경용)
    void CaptureSceneColor(ID3D12GraphicsCommandList* pCmdList,
                           ID3D12Resource* pMainRTBuffer);

    // Pass 1a 시작: FluidDepthRT + FluidDSV 바인딩 (깊이 테스트 있음)
    void BeginDepthPass(ID3D12GraphicsCommandList* pCmdList);
    // Pass 1b 시작: ThicknessRT만 바인딩 (깊이 테스트 없음, 가산 블렌딩)
    void BeginThicknessPass(ID3D12GraphicsCommandList* pCmdList);
    // Pass 1 종료: FluidDepthRT + ThicknessRT를 SRV로 전환
    void EndDepthPass(ID3D12GraphicsCommandList* pCmdList);

    // Pass 2+3: Bilateral Smooth 후 메인 RT에 합성
    void SmoothAndComposite(
        ID3D12GraphicsCommandList* pCmdList,
        D3D12_CPU_DESCRIPTOR_HANDLE mainRTV,
        D3D12_CPU_DESCRIPTOR_HANDLE mainDSV,
        const XMFLOAT4X4& proj,            // 비전치 프로젝션 행렬
        const XMFLOAT3& lightDirVS,        // 뷰 공간 조명 방향
        const XMFLOAT4& fluidColorOuter,   // 외곽(얇은 부분/코로나) 색상, a=발광 강도
        const XMFLOAT4& fluidColorInner);  // 코어(두꺼운 부분) 색상

    // Sphere depth PSO + root sig (FluidParticleSystem::RenderDepth에서 사용)
    ID3D12RootSignature* GetDepthRootSignature() const { return m_pDepthRootSig.Get(); }
    ID3D12PipelineState* GetDepthPSO() const { return m_pDepthPSO.Get(); }
    // Thickness PSO (깊이 테스트 없음, 가산 블렌딩)
    ID3D12PipelineState* GetThicknessPSO() const { return m_pThicknessPSO.Get(); }

    bool IsInitialized() const { return m_bInitialized; }

    UINT GetWidth() const { return m_Width; }
    UINT GetHeight() const { return m_Height; }

    // 블러 활성화 여부 (false = 개별 입자 그대로 렌더, true = Bilateral smooth 적용)
    void SetBlurEnabled(bool bEnable) { m_bEnableBlur = bEnable; }
    bool IsBlurEnabled() const { return m_bEnableBlur; }

private:
    void CreateTextures(ID3D12Device* pDevice, UINT width, UINT height);
    void CreatePipelines(ID3D12Device* pDevice);

    UINT m_Width = 0, m_Height = 0;
    bool m_bInitialized = false;
    bool m_bEnableBlur  = false;  // 기본 꺼짐; Q 파도 패스에서 SetBlurEnabled(true) 호출

    // 렌더 타겟 텍스처
    ComPtr<ID3D12Resource> m_pFluidDepthRT;    // R32_FLOAT: 구체 깊이 (선형 뷰 공간 Z)
    ComPtr<ID3D12Resource> m_pSmoothedRT;      // R32_FLOAT: Bilateral blur 결과
    ComPtr<ID3D12Resource> m_pTempRT;          // R32_FLOAT: 수평 blur 중간 결과
    ComPtr<ID3D12Resource> m_pThicknessRT;     // R16_FLOAT: 유체 두께 (additive blend)
    ComPtr<ID3D12Resource> m_pSceneColorRT;    // R8G8B8A8_UNORM: 유체 렌더 전 장면 캡처 (굴절 배경)
    ComPtr<ID3D12Resource> m_pFluidDSV;        // D32_FLOAT: 구체 깊이 패스 전용 depth buffer

    // 디스크립터 힙
    // RTV 힙 레이아웃: FluidDepth(0), Smoothed(1), Temp(2), Thickness(3)
    ComPtr<ID3D12DescriptorHeap> m_pRTVHeap;  // 4 RTVs
    ComPtr<ID3D12DescriptorHeap> m_pDSVHeap;  // 1 DSV
    // SRV 힙 레이아웃: FluidDepth(0), Temp(1), Smoothed(2), Thickness(3), SceneColor(4), TempUAV(5), SmoothedUAV(6)
    // -> CS Blur H-pass: FluidDepth SRV(0) 입력 -> TempRT UAV(5) 출력
    // -> CS Blur V-pass: Temp SRV(1) 입력 -> SmoothedRT UAV(6) 출력
    // -> Composite: Smoothed(2) + Thickness(3) + SceneColor(4) 연속 3개
    ComPtr<ID3D12DescriptorHeap> m_pSRVHeap;  // 7 descriptors (5 SRV + 2 UAV)
    UINT m_RTVIncrSize = 0;
    UINT m_SRVIncrSize = 0;

    // SceneColorRT 리소스 상태 추적
    D3D12_RESOURCE_STATES m_eSceneColorState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

    // Pass 1a: Sphere depth PSO (깊이 테스트 있음, FluidDepthRT only)
    ComPtr<ID3D12RootSignature> m_pDepthRootSig;
    ComPtr<ID3D12PipelineState> m_pDepthPSO;
    // Pass 1b: Thickness PSO (깊이 테스트 없음, ThicknessRT only, 가산 블렌딩)
    ComPtr<ID3D12PipelineState> m_pThicknessPSO;

    // Pass 2: Bilateral smooth - Compute Shader 기반
    ComPtr<ID3D12RootSignature> m_pSmoothRootSig;
    ComPtr<ID3D12PipelineState> m_pSmoothPSO;       // 기존 Graphics PSO (미사용, 유지)
    ComPtr<ID3D12RootSignature> m_pBlurCSRootSig;    // Compute blur 루트 시그니처
    ComPtr<ID3D12PipelineState> m_pBlurHPSO;         // 수평 blur Compute PSO
    ComPtr<ID3D12PipelineState> m_pBlurVPSO;         // 수직 blur Compute PSO

    // Pass 3: Composite PSO
    ComPtr<ID3D12RootSignature> m_pCompositeRootSig;
    ComPtr<ID3D12PipelineState> m_pCompositePSO;

    // Constant buffer (Upload heap, 영구 매핑)
    // Offset 0:   SmoothCB_H  (256 bytes)
    // Offset 256:  SmoothCB_V  (256 bytes)
    // Offset 512:  CompositeCB (256 bytes)
    ComPtr<ID3D12Resource> m_pCBUpload;
    BYTE* m_pMappedCB = nullptr;
};
