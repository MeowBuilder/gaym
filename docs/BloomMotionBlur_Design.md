# Bloom & Motion Blur 후처리 효과 설계 문서

## 개요
게임의 시각적 품질을 향상시키기 위한 Bloom(발광 효과)과 Motion Blur(모션 블러) 후처리 구현

## 의존성
- **기반 시스템**: `PostProcessSystem_Design.md`의 PostProcessManager 필요
- **라이브러리**: DirectXTK12 BasicPostProcess (Bloom용)
- **커스텀 셰이더**: Motion Blur용 HLSL 셰이더

---

## Part 1: Bloom 효과

### 1.1 Bloom 파이프라인

```
SceneRT
   │
   ▼
[BloomExtract] ─► Extract 밝은 영역 (threshold 기준)
   │
   ▼
[DownScale 4x4] ─► 해상도 1/4로 축소 (성능 최적화)
   │
   ▼
[BloomBlur H] ─► 수평 블러
   │
   ▼
[BloomBlur V] ─► 수직 블러
   │
   ▼
[BloomCombine] ─► 원본 + 블러된 밝은 영역 합성
   │
   ▼
Final Output
```

### 1.2 BloomPass 클래스

**파일: BloomPass.h / BloomPass.cpp**

```cpp
class BloomPass : public IPostProcessPass
{
public:
    BloomPass();
    ~BloomPass();

    void Init(ID3D12Device* pDevice, const RenderTargetState& rtState) override;
    void Process(ID3D12GraphicsCommandList* pCmdList,
                D3D12_GPU_DESCRIPTOR_HANDLE srcSRV,
                D3D12_CPU_DESCRIPTOR_HANDLE dstRTV,
                UINT width, UINT height) override;
    void OnResize(ID3D12Device* pDevice, UINT width, UINT height) override;

    // 파라미터
    void SetThreshold(float threshold);      // 밝기 임계값 (기본: 0.8)
    void SetIntensity(float intensity);      // 블룸 강도 (기본: 1.0)
    void SetBlurSize(float size);            // 블러 크기 (기본: 4.0)

private:
    // DirectXTK12 PostProcess 객체
    std::unique_ptr<BasicPostProcess> m_pBloomExtract;
    std::unique_ptr<BasicPostProcess> m_pDownScale;
    std::unique_ptr<BasicPostProcess> m_pBloomBlurH;
    std::unique_ptr<BasicPostProcess> m_pBloomBlurV;
    std::unique_ptr<DualPostProcess> m_pBloomCombine;

    // 중간 렌더 타겟
    RenderTarget m_ExtractRT;      // 추출된 밝은 영역
    RenderTarget m_DownScaleRT;    // 축소된 RT
    RenderTarget m_BlurHRT;        // 수평 블러 결과
    RenderTarget m_BlurVRT;        // 수직 블러 결과

    // 파라미터
    float m_fThreshold = 0.8f;
    float m_fIntensity = 1.0f;
    float m_fBlurSize = 4.0f;
};
```

### 1.3 Bloom 파라미터 가이드

| 파라미터 | 범위 | 설명 |
|---------|------|------|
| Threshold | 0.0 ~ 1.0 | 낮을수록 더 많은 영역이 발광 |
| Intensity | 0.0 ~ 3.0 | 높을수록 강한 발광 |
| BlurSize | 1.0 ~ 10.0 | 클수록 넓게 퍼짐 |

**추천 프리셋:**
- **Subtle**: threshold=0.9, intensity=0.5, blurSize=2.0
- **Standard**: threshold=0.8, intensity=1.0, blurSize=4.0
- **Intense**: threshold=0.6, intensity=1.5, blurSize=6.0
- **Fire Map**: threshold=0.5, intensity=2.0, blurSize=5.0 (용암 맵용)

### 1.4 Bloom 렌더 타겟 구성

```
해상도: 1920x1080 기준

SceneRT:      1920 x 1080  (RGBA8)
ExtractRT:    1920 x 1080  (RGBA8)
DownScaleRT:   480 x 270   (RGBA8) - 1/4 해상도
BlurHRT:       480 x 270   (RGBA8)
BlurVRT:       480 x 270   (RGBA8)
```

---

## Part 2: Motion Blur 효과

### 2.1 Motion Blur 방식 비교

| 방식 | 장점 | 단점 |
|------|------|------|
| **Object-based** | 정확한 오브젝트별 블러 | 구현 복잡, 성능 비용 높음 |
| **Camera-based** | 구현 간단 | 정적 오브젝트도 블러됨 |
| **Velocity Buffer** | 가장 정확 | G-Buffer 확장 필요 |

**선택: Camera-based Motion Blur** (구현 단순성 우선)

### 2.2 Camera Motion Blur 원리

```
현재 프레임 픽셀 위치 → 이전 프레임 위치 역산 → 방향 벡터 계산 → 해당 방향으로 샘플링
```

**필요 데이터:**
- 현재 View-Projection 행렬
- 이전 프레임 View-Projection 행렬
- 뎁스 버퍼 (월드 위치 복원용)

### 2.3 MotionBlurPass 클래스

**파일: MotionBlurPass.h / MotionBlurPass.cpp**

```cpp
class MotionBlurPass : public IPostProcessPass
{
public:
    void Init(ID3D12Device* pDevice, const RenderTargetState& rtState) override;
    void Process(ID3D12GraphicsCommandList* pCmdList,
                D3D12_GPU_DESCRIPTOR_HANDLE srcSRV,
                D3D12_CPU_DESCRIPTOR_HANDLE dstRTV,
                UINT width, UINT height) override;

    // 카메라 행렬 설정 (매 프레임 호출)
    void SetViewProjection(const XMMATRIX& currentVP, const XMMATRIX& previousVP);

    // 뎁스 버퍼 설정
    void SetDepthTexture(D3D12_GPU_DESCRIPTOR_HANDLE depthSRV);

    // 파라미터
    void SetSampleCount(int count);      // 샘플 수 (기본: 8)
    void SetIntensity(float intensity);  // 블러 강도 (기본: 1.0)
    void SetMaxBlur(float maxPixels);    // 최대 블러 픽셀 (기본: 20)

private:
    // PSO & Root Signature
    ComPtr<ID3D12RootSignature> m_pRootSignature;
    ComPtr<ID3D12PipelineState> m_pPipelineState;

    // 상수 버퍼
    struct MotionBlurConstants
    {
        XMFLOAT4X4 InvViewProj;        // 현재 프레임 역행렬
        XMFLOAT4X4 PrevViewProj;       // 이전 프레임 행렬
        float SampleCount;
        float Intensity;
        float MaxBlurPixels;
        float Padding;
    };

    ComPtr<ID3D12Resource> m_pConstantBuffer;
    MotionBlurConstants* m_pMappedCB = nullptr;

    // 이전 프레임 행렬 저장
    XMMATRIX m_PrevViewProj;
    bool m_bFirstFrame = true;

    int m_nSampleCount = 8;
    float m_fIntensity = 1.0f;
    float m_fMaxBlurPixels = 20.0f;
};
```

### 2.4 Motion Blur 셰이더

**파일: MotionBlur.hlsl**

```hlsl
// 상수 버퍼
cbuffer MotionBlurCB : register(b0)
{
    float4x4 InvViewProj;
    float4x4 PrevViewProj;
    float SampleCount;
    float Intensity;
    float MaxBlurPixels;
    float Padding;
};

Texture2D SceneTexture : register(t0);
Texture2D DepthTexture : register(t1);
SamplerState LinearSampler : register(s0);

struct VSOutput
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

// Fullscreen Triangle VS
VSOutput VSMain(uint vertexID : SV_VertexID)
{
    VSOutput output;
    output.TexCoord = float2((vertexID << 1) & 2, vertexID & 2);
    output.Position = float4(output.TexCoord * float2(2, -2) + float2(-1, 1), 0, 1);
    return output;
}

float4 PSMain(VSOutput input) : SV_TARGET
{
    float2 texCoord = input.TexCoord;
    float depth = DepthTexture.Sample(LinearSampler, texCoord).r;

    // NDC 좌표로 변환
    float4 ndcPos = float4(texCoord * 2.0 - 1.0, depth, 1.0);
    ndcPos.y = -ndcPos.y;

    // 월드 위치 복원
    float4 worldPos = mul(ndcPos, InvViewProj);
    worldPos /= worldPos.w;

    // 이전 프레임 NDC 위치 계산
    float4 prevClipPos = mul(worldPos, PrevViewProj);
    float2 prevNDC = prevClipPos.xy / prevClipPos.w;
    prevNDC.y = -prevNDC.y;
    float2 prevTexCoord = prevNDC * 0.5 + 0.5;

    // 속도 벡터 계산
    float2 velocity = (texCoord - prevTexCoord) * Intensity;

    // 최대 블러 제한
    float velocityLength = length(velocity);
    float2 texelSize = 1.0 / float2(1920, 1080); // TODO: 상수로 전달
    float maxVelocity = MaxBlurPixels * texelSize.x;
    if (velocityLength > maxVelocity)
    {
        velocity = normalize(velocity) * maxVelocity;
    }

    // 모션 블러 샘플링
    float4 color = float4(0, 0, 0, 0);
    for (int i = 0; i < (int)SampleCount; i++)
    {
        float t = (float)i / (SampleCount - 1.0) - 0.5;
        float2 sampleCoord = texCoord + velocity * t;
        color += SceneTexture.Sample(LinearSampler, sampleCoord);
    }
    color /= SampleCount;

    return color;
}
```

### 2.5 Motion Blur 파라미터 가이드

| 파라미터 | 범위 | 설명 |
|---------|------|------|
| SampleCount | 4 ~ 16 | 높을수록 부드럽지만 성능 비용 증가 |
| Intensity | 0.0 ~ 2.0 | 블러 강도 배수 |
| MaxBlurPixels | 10 ~ 50 | 최대 블러 거리 제한 |

**추천 프리셋:**
- **Subtle**: samples=4, intensity=0.5, maxBlur=10
- **Standard**: samples=8, intensity=1.0, maxBlur=20
- **Cinematic**: samples=12, intensity=1.5, maxBlur=30

---

## Part 3: 통합 파이프라인

### 3.1 렌더링 순서

```
Scene Render (to OffscreenRT)
      │
      ▼
[MotionBlurPass] ─► 모션 블러 적용 (선택)
      │
      ▼
[BloomPass] ─► 블룸 효과 적용
      │
      ▼
[ToneMapPass] ─► 톤 매핑 (선택, HDR용)
      │
      ▼
[Copy to BackBuffer]
      │
      ▼
Text/UI Render
      │
      ▼
Present
```

### 3.2 PostProcessManager 확장

```cpp
// 패스 등록 순서 (순서 중요!)
m_pPostProcessManager->AddPass(std::make_unique<MotionBlurPass>());
m_pPostProcessManager->AddPass(std::make_unique<BloomPass>());
// m_pPostProcessManager->AddPass(std::make_unique<ToneMapPass>()); // 선택

// 런타임 on/off
m_pPostProcessManager->SetPassEnabled("MotionBlur", true);
m_pPostProcessManager->SetPassEnabled("Bloom", true);
```

### 3.3 디스크립터 힙 확장

**기존 PostProcessManager SRV 힙 (16슬롯) 확장:**

| 슬롯 | 용도 |
|------|------|
| 0 | SceneRT SRV |
| 1 | IntermediateRT[0] SRV |
| 2 | IntermediateRT[1] SRV |
| 3 | DepthBuffer SRV (Motion Blur용) |
| 4 | BloomExtractRT SRV |
| 5 | BloomDownScaleRT SRV |
| 6 | BloomBlurHRT SRV |
| 7 | BloomBlurVRT SRV |
| 8~15 | 예약 |

---

## Part 4: 구현 순서

### Phase 1: 기반 시스템
1. PostProcessManager 기본 구현 (이미 설계됨)
2. RenderTarget 래퍼 클래스
3. Dx12App 렌더링 흐름 수정

### Phase 2: Bloom
1. BloomPass 클래스 생성
2. DirectXTK12 BasicPostProcess 연동
3. 파라미터 조정 및 테스트

### Phase 3: Motion Blur
1. MotionBlur.hlsl 셰이더 작성
2. MotionBlurPass 클래스 생성
3. 뎁스 버퍼 SRV 생성
4. 카메라 행렬 연동

### Phase 4: 통합 및 폴리싱
1. 패스 순서 최적화
2. 퍼포먼스 프로파일링
3. 디버그 토글 (F2: Bloom, F3: Motion Blur)

---

## Part 5: 새로 생성할 파일

```
gaym/
├── RenderTarget.h
├── RenderTarget.cpp
├── IPostProcessPass.h
├── PostProcessManager.h
├── PostProcessManager.cpp
├── BloomPass.h
├── BloomPass.cpp
├── MotionBlurPass.h
├── MotionBlurPass.cpp
└── Shaders/
    └── MotionBlur.hlsl
```

---

## Part 6: 수정할 기존 파일

| 파일 | 수정 내용 |
|------|-----------|
| Dx12App.h | PostProcessManager 멤버 추가 |
| Dx12App.cpp | OnCreate에서 초기화, FrameAdvance 렌더링 흐름 변경 |
| Camera.h | GetPreviousViewProjection() 추가 |
| Camera.cpp | 이전 프레임 VP 행렬 저장 |
| Scene.h | 뎁스 버퍼 SRV 접근자 추가 |

---

## Part 7: 성능 고려사항

### Bloom 최적화
- DownScale을 통해 1/4 해상도에서 블러 수행
- 블러 패스 분리 (H, V) - Separable Gaussian

### Motion Blur 최적화
- 샘플 수 동적 조절 (속도에 비례)
- 하프 해상도 옵션 제공
- 스카이박스/UI 마스킹 (선택)

### 메모리
- 렌더 타겟 풀링 (동일 크기 RT 재사용)
- 포맷 최적화 (RGBA8 vs R11G11B10_FLOAT)

---

## Part 8: 검증 체크리스트

### Bloom
- [ ] 밝은 오브젝트(Emissive) 주변 발광 확인
- [ ] Threshold 조절 시 발광 영역 변화
- [ ] Intensity 조절 시 발광 강도 변화
- [ ] 화면 리사이즈 시 정상 동작

### Motion Blur
- [ ] 카메라 회전 시 블러 효과 확인
- [ ] 카메라 이동 시 블러 효과 확인
- [ ] 정지 상태에서 블러 없음 확인
- [ ] MaxBlur 제한 정상 동작

### 통합
- [ ] Bloom + Motion Blur 동시 적용 확인
- [ ] 효과 on/off 토글 정상 동작
- [ ] 프레임 레이트 영향 측정 (목표: 60fps 유지)

---

## Part 9: 참고 자료

### 기존 코드
| 기능 | 파일 | 참고 |
|------|------|------|
| 현재 렌더링 흐름 | Dx12App.cpp:328 | FrameAdvance() |
| 셰이더 컴파일 | Shader.cpp | CreateShaderFromFile() |
| 디스크립터 힙 | DescriptorHeap.h | DirectXTK12 |
| 기존 셰이더 | shaders.hlsl | VS/PS 구조 참고 |

### DirectXTK12 PostProcess
| 효과 | 클래스 | 용도 |
|------|--------|------|
| BloomExtract | BasicPostProcess | 밝기 추출 |
| BloomBlur | BasicPostProcess | 블룸 블러 |
| BloomCombine | DualPostProcess | 원본+블룸 합성 |
| GaussianBlur_5x5 | BasicPostProcess | 범용 블러 |
| DownScale_4x4 | BasicPostProcess | 다운샘플링 |

---

## Part 10: 향후 확장

- **DOF (Depth of Field)**: 뎁스 기반 포커스 블러
- **Vignette**: 화면 가장자리 어둡게
- **Color Grading**: LUT 기반 색 보정
- **Chromatic Aberration**: 색수차 효과
- **Film Grain**: 필름 그레인 노이즈
