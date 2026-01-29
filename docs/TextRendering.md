# Text Rendering System

DirectXTK12를 사용한 텍스트 렌더링 시스템 가이드입니다.

## 개요

이 프로젝트는 **DirectXTK12 (DirectX Tool Kit for DX12)**의 `SpriteFont`와 `SpriteBatch`를 사용하여 2D 텍스트를 3D 씬 위에 오버레이로 렌더링합니다.

## 구조

```
Dx12App
├── m_graphicsMemory      // GPU 메모리 관리
├── m_fontDescriptorHeap  // 폰트 텍스처용 디스크립터 힙
├── m_spriteBatch         // 2D 스프라이트 배치 렌더러
└── m_spriteFont          // 폰트 데이터
```

## 파일 위치

| 파일 | 역할 |
|------|------|
| `Dx12App.h` | 텍스트 렌더링 멤버 변수 선언 |
| `Dx12App.cpp` | `InitializeText()`, `RenderText()` 구현 |
| `Fonts/myFont.spritefont` | 폰트 데이터 파일 |

---

## 사용 방법

### 1. 기본 텍스트 출력

```cpp
void Dx12App::RenderText()
{
    // 디스크립터 힙 바인딩 (필수)
    ID3D12DescriptorHeap* heaps[] = { m_fontDescriptorHeap->Heap() };
    m_pd3dCommandList->SetDescriptorHeaps(1, heaps);

    m_spriteBatch->Begin(m_pd3dCommandList.Get());

    // 텍스트 그리기
    m_spriteFont->DrawString(
        m_spriteBatch.get(),
        L"Hello World!",           // 텍스트 (wchar_t*)
        XMFLOAT2(100, 100),        // 화면 위치 (픽셀)
        DirectX::Colors::White     // 색상
    );

    m_spriteBatch->End();
}
```

### 2. DrawString 파라미터

```cpp
m_spriteFont->DrawString(
    m_spriteBatch.get(),    // SpriteBatch 포인터
    L"텍스트",               // 출력할 문자열
    XMFLOAT2(x, y),         // 위치 (좌상단 기준)
    color,                   // 색상 (XMVECTORF32 또는 FXMVECTOR)
    rotation,                // 회전 (라디안, 기본값 0.0f)
    origin,                  // 회전 원점 (기본값 XMFLOAT2(0,0))
    scale                    // 스케일 (기본값 1.0f)
);
```

### 3. 색상 사용

```cpp
// 미리 정의된 색상 (DirectXColors.h)
DirectX::Colors::White
DirectX::Colors::Red
DirectX::Colors::Yellow
DirectX::Colors::Orange
DirectX::Colors::Green
DirectX::Colors::Blue
DirectX::Colors::Cyan
DirectX::Colors::Magenta

// 커스텀 색상 (RGBA, 0.0~1.0)
XMVECTORF32 customColor = { 1.0f, 0.5f, 0.0f, 1.0f };  // 주황색
```

### 4. 텍스트 중앙 정렬

```cpp
const wchar_t* text = L"Centered Text";

// 텍스트 크기 측정
XMVECTOR textSize = m_spriteFont->MeasureString(text);
float textWidth = XMVectorGetX(textSize);
float textHeight = XMVectorGetY(textSize);

// 화면 중앙 계산
float screenCenterX = (float)m_nWndClientWidth / 2.0f;
float screenCenterY = (float)m_nWndClientHeight / 2.0f;

// 중앙 정렬 위치
XMFLOAT2 position(
    screenCenterX - textWidth / 2.0f,
    screenCenterY - textHeight / 2.0f
);

m_spriteFont->DrawString(m_spriteBatch.get(), text, position, DirectX::Colors::White);
```

### 5. 여러 줄 텍스트

```cpp
// \n으로 줄바꿈
m_spriteFont->DrawString(
    m_spriteBatch.get(),
    L"Line 1\nLine 2\nLine 3",
    XMFLOAT2(50, 50),
    DirectX::Colors::White
);
```

### 6. 조건부 텍스트 표시

```cpp
void Dx12App::RenderText()
{
    // ... 힙 바인딩, Begin ...

    // 특정 조건에서만 표시
    if (m_pScene->IsNearInteractionCube())
    {
        m_spriteFont->DrawString(
            m_spriteBatch.get(),
            L"[F] Interact",
            XMFLOAT2(screenCenterX, screenCenterY + 100),
            DirectX::Colors::Yellow
        );
    }

    // ... End ...
}
```

---

## 폰트 파일 생성

### MakeSpriteFont 도구 위치

```
packages\directxtk12_desktop_2019.xxxx\tools\MakeSpriteFont.exe
```

### 기본 사용법

```batch
MakeSpriteFont "폰트이름" 출력파일.spritefont [옵션]
```

### 한글 폰트 생성 예시

```batch
MakeSpriteFont "맑은 고딕" myFont.spritefont ^
    /FontSize:24 ^
    /CharacterRegion:0x0020-0x007F ^
    /CharacterRegion:0xAC00-0xD7AF
```

### 주요 옵션

| 옵션 | 설명 | 예시 |
|------|------|------|
| `/FontSize:N` | 폰트 크기 (픽셀) | `/FontSize:32` |
| `/FontStyle:X` | 스타일 (Bold, Italic) | `/FontStyle:Bold` |
| `/CharacterRegion:시작-끝` | 포함할 유니코드 범위 | 아래 참조 |
| `/DefaultCharacter:X` | 없는 글자 대체 문자 | `/DefaultCharacter:?` |
| `/Sharp` | 선명한 렌더링 | |

### 유니코드 범위

| 범위 | 설명 |
|------|------|
| `0x0020-0x007F` | 기본 ASCII (영문, 숫자, 특수문자) |
| `0xAC00-0xD7AF` | 한글 완성형 (11,172자) |
| `0x3131-0x318E` | 한글 자모 (ㄱ,ㄴ,ㄷ...) |

### 폰트 생성 배치 파일 예시 (make_font.bat)

```batch
@echo off
set TOOL=packages\directxtk12_desktop_2019.2025.10.28.1\tools\MakeSpriteFont.exe

%TOOL% "맑은 고딕" Fonts\UIFont.spritefont ^
    /FontSize:24 ^
    /CharacterRegion:0x0020-0x007F ^
    /CharacterRegion:0xAC00-0xD7AF ^
    /DefaultCharacter:?

echo Font created!
pause
```

---

## 새 폰트 추가하기

### 1. 폰트 파일 생성

```batch
MakeSpriteFont "나눔고딕" Fonts\NanumFont.spritefont /FontSize:20 /CharacterRegion:0x0020-0x007F /CharacterRegion:0xAC00-0xD7AF
```

### 2. 멤버 변수 추가 (Dx12App.h)

```cpp
std::unique_ptr<DirectX::SpriteFont> m_spriteFont;      // 기존
std::unique_ptr<DirectX::SpriteFont> m_spriteFontLarge; // 추가
```

### 3. 디스크립터 힙 크기 증가 (Dx12App.cpp - InitializeText)

```cpp
m_fontDescriptorHeap = std::make_unique<DirectX::DescriptorHeap>(
    m_pd3dDevice.Get(),
    D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
    D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
    2  // 폰트 2개로 증가
);
```

### 4. 새 폰트 로드 (InitializeText)

```cpp
// 기존 폰트 (인덱스 0)
m_spriteFont = std::make_unique<DirectX::SpriteFont>(
    m_pd3dDevice.Get(),
    resourceUpload,
    L"Fonts/myFont.spritefont",
    m_fontDescriptorHeap->GetCpuHandle(0),
    m_fontDescriptorHeap->GetGpuHandle(0)
);

// 새 폰트 (인덱스 1)
m_spriteFontLarge = std::make_unique<DirectX::SpriteFont>(
    m_pd3dDevice.Get(),
    resourceUpload,
    L"Fonts/NanumFont.spritefont",
    m_fontDescriptorHeap->GetCpuHandle(1),
    m_fontDescriptorHeap->GetGpuHandle(1)
);
```

### 5. 사용

```cpp
m_spriteFont->DrawString(...);      // 기본 폰트
m_spriteFontLarge->DrawString(...); // 큰 폰트
```

---

## UI 텍스트 박스 예시

### 스킬 설명 박스

```cpp
void RenderSkillTooltip(const wchar_t* skillName, const wchar_t* description, int damage)
{
    wchar_t buffer[256];
    swprintf_s(buffer, L"[%s]\n%s\nDamage: %d", skillName, description, damage);

    // 배경 위치 (실제 배경은 별도 스프라이트 필요)
    XMFLOAT2 boxPos(50, 400);

    m_spriteFont->DrawString(
        m_spriteBatch.get(),
        buffer,
        boxPos,
        DirectX::Colors::White
    );
}

// 사용
RenderSkillTooltip(L"Fireball", L"Launches a ball of fire", 50);
```

### 체력 표시

```cpp
void RenderHealthBar(int currentHP, int maxHP)
{
    wchar_t buffer[64];
    swprintf_s(buffer, L"HP: %d / %d", currentHP, maxHP);

    // 체력 비율에 따른 색상
    float hpRatio = (float)currentHP / maxHP;
    XMVECTORF32 color;
    if (hpRatio > 0.5f)
        color = DirectX::Colors::Green;
    else if (hpRatio > 0.25f)
        color = DirectX::Colors::Yellow;
    else
        color = DirectX::Colors::Red;

    m_spriteFont->DrawString(
        m_spriteBatch.get(),
        buffer,
        XMFLOAT2(50, 50),
        color
    );
}
```

### 상호작용 프롬프트

```cpp
void RenderInteractionPrompt(const wchar_t* key, const wchar_t* action)
{
    wchar_t buffer[64];
    swprintf_s(buffer, L"[%s] %s", key, action);

    // 텍스트 중앙 정렬
    XMVECTOR size = m_spriteFont->MeasureString(buffer);
    float width = XMVectorGetX(size);

    XMFLOAT2 pos(
        m_nWndClientWidth / 2.0f - width / 2.0f,
        m_nWndClientHeight / 2.0f + 100.0f
    );

    m_spriteFont->DrawString(m_spriteBatch.get(), buffer, pos, DirectX::Colors::Yellow);
}

// 사용
RenderInteractionPrompt(L"F", L"Open Door");
RenderInteractionPrompt(L"E", L"Pick Up Item");
```

---

## 주의사항

1. **디스크립터 힙 바인딩**: `DrawString` 호출 전에 반드시 `SetDescriptorHeaps` 호출
2. **Begin/End**: 모든 `DrawString`은 `Begin()`과 `End()` 사이에서 호출
3. **GraphicsMemory::Commit**: 매 프레임 끝에 호출 필수
4. **한글 지원**: 폰트 생성 시 한글 유니코드 범위 포함 필요
5. **렌더링 순서**: 3D 씬 렌더링 후 텍스트 렌더링 (오버레이)

---

## 렌더링 파이프라인 순서

```cpp
void Dx12App::FrameAdvance()
{
    // 1. 3D 씬 렌더링
    m_pScene->Render(m_pd3dCommandList.Get());

    // 2. 2D 텍스트 오버레이 (3D 위에 그려짐)
    RenderText();

    // 3. Present 전 GPU 메모리 커밋
    m_graphicsMemory->Commit(m_pd3dCommandQueue.Get());

    // 4. Present
    m_pdxgiSwapChain->Present(1, 0);
}
```

---

## 참고 자료

- [DirectXTK12 Wiki - SpriteFont](https://github.com/microsoft/DirectXTK12/wiki/SpriteFont)
- [DirectXTK12 Wiki - SpriteBatch](https://github.com/microsoft/DirectXTK12/wiki/SpriteBatch)
- [MakeSpriteFont 사용법](https://github.com/microsoft/DirectXTK12/wiki/MakeSpriteFont)
