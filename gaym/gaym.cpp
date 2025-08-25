#include "stdafx.h"
#include "Dx12App.h"

static Dx12App g_app;
static bool g_fullscreen = false;
static RECT g_window_rect = { 0 };

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_KEYDOWN:
		if (wParam == VK_RETURN && (GetKeyState(VK_SHIFT) & 0x8000))
		{
			g_fullscreen = !g_fullscreen;
			if (g_fullscreen)
			{
				GetWindowRect(hWnd, &g_window_rect);

				SetWindowLong(hWnd, GWL_STYLE, WS_POPUP);

				HMONITOR hmon = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
				MONITORINFO mi = { sizeof(mi) };
				GetMonitorInfo(hmon, &mi);

				SetWindowPos(hWnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
					mi.rcMonitor.right - mi.rcMonitor.left,
					mi.rcMonitor.bottom - mi.rcMonitor.top,
					SWP_FRAMECHANGED | SWP_NOACTIVATE);
				ShowWindow(hWnd, SW_MAXIMIZE);
			}
			else
			{
				SetWindowLong(hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);
				SetWindowPos(hWnd, HWND_NOTOPMOST, g_window_rect.left, g_window_rect.top,
					g_window_rect.right - g_window_rect.left,
					g_window_rect.bottom - g_window_rect.top,
					SWP_FRAMECHANGED | SWP_NOACTIVATE);
				ShowWindow(hWnd, SW_NORMAL);
			}
		}
		return 0;
	case WM_SIZE:
	{
		UINT w = LOWORD(lParam), h = HIWORD(lParam);
		try { g_app.Resize(w, h); }
		catch (...) {}
		return 0;
	}
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	default:
		return DefWindowProc(hWnd, msg, wParam, lParam);
	}
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow)
{
	// 윈도우 클래스 등록
	const wchar_t* kClass = L"DX12WindowClass";
	WNDCLASSEXW wc{};
	wc.cbSize = sizeof(wc);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.lpszClassName = kClass;
	RegisterClassExW(&wc);

	// 창 생성
	RECT rc{ 0,0,1280,720 };
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
	HWND hwnd = CreateWindowW(
		kClass, L"DX12 Starter",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		rc.right - rc.left, rc.bottom - rc.top,
		nullptr, nullptr, hInstance, nullptr);

	if (!hwnd) return -1;

	ShowWindow(hwnd, nCmdShow);
	UpdateWindow(hwnd);

	// DX12 초기화
	try
	{
		g_app.Initialize(hwnd, 1280, 720, /*debug*/true);
	}
	catch (const std::exception& e)
	{
		MessageBoxA(hwnd, e.what(), "DX12 Init Failed", MB_ICONERROR);
		return -2;
	}

	// 메시지 루프
	MSG msg{};
	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			// 한 프레임 렌더
			try { g_app.Render(); }
			catch (const std::exception& e)
			{
				MessageBoxA(hwnd, e.what(), "Render Error", MB_ICONERROR);
				break;
			}
		}
	}

	g_app.WaitForGPU();
	return static_cast<int>(msg.wParam);
}