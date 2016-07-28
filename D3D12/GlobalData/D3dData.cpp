#include "D3dData.h"

#include <GlobalData/Settings.h>
#include <Utils/DebugUtils.h>

namespace {
	void InitMainWindow(HWND& hwnd, const HINSTANCE hInstance) noexcept {
		WNDCLASS wc = {};
		wc.style = CS_HREDRAW | CS_VREDRAW;
		wc.lpfnWndProc = DefWindowProc;
		wc.cbClsExtra = 0;
		wc.cbWndExtra = 0;
		wc.hInstance = hInstance;
		wc.hIcon = LoadIcon(0, IDI_APPLICATION);
		wc.hCursor = LoadCursor(0, IDC_ARROW);
		wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
		wc.lpszMenuName = 0;
		wc.lpszClassName = L"MainWnd";

		ASSERT(RegisterClass(&wc));

		// Compute window rectangle dimensions based on requested client area dimensions.
		RECT r = { 0, 0, Settings::sWindowWidth, Settings::sWindowHeight };
		AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, false);
		const int32_t width{ r.right - r.left };
		const int32_t height{ r.bottom - r.top };

		const std::uint32_t dwStyle = Settings::sFullscreen ? WS_POPUP : WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
		hwnd = CreateWindowEx(WS_EX_APPWINDOW, L"MainWnd", L"App", dwStyle, CW_USEDEFAULT, CW_USEDEFAULT, width, height, nullptr, nullptr, hInstance, 0);
		ASSERT(hwnd);

		ShowWindow(hwnd, SW_SHOW);
		UpdateWindow(hwnd);
	}
}

HWND D3dData::mHwnd;
Microsoft::WRL::ComPtr<IDXGIFactory4> D3dData::mDxgiFactory{ nullptr };
Microsoft::WRL::ComPtr<IDXGISwapChain3> D3dData::mSwapChain{ nullptr };
Microsoft::WRL::ComPtr<ID3D12Device> D3dData::mDevice{ nullptr };

void D3dData::InitDirect3D(const HINSTANCE hInstance) noexcept {
	InitMainWindow(mHwnd, hInstance);

#if defined(DEBUG) || defined(_DEBUG) 
	// Enable the D3D12 debug layer.
	{
		Microsoft::WRL::ComPtr<ID3D12Debug> debugController;
		CHECK_HR(D3D12GetDebugInterface(IID_PPV_ARGS(debugController.GetAddressOf())));
		debugController->EnableDebugLayer();
	}
#endif

	// Create device
	CHECK_HR(CreateDXGIFactory1(IID_PPV_ARGS(D3dData::mDxgiFactory.GetAddressOf())));
	CHECK_HR(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(D3dData::mDevice.GetAddressOf())));
}

void D3dData::CreateSwapChain(const HWND hwnd, ID3D12CommandQueue& cmdQueue) noexcept {
	IDXGISwapChain* swapChain{ nullptr };

	DXGI_SWAP_CHAIN_DESC sd = {};
	sd.BufferDesc.Width = Settings::sWindowWidth;
	sd.BufferDesc.Height = Settings::sWindowHeight;
	sd.BufferDesc.RefreshRate.Numerator = 60U;
	sd.BufferDesc.RefreshRate.Denominator = 1U;
	sd.BufferDesc.Format = Settings::sBackBufferFormat;
	sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	sd.SampleDesc.Count = 1U;
	sd.SampleDesc.Quality = 0U;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferCount = Settings::sSwapChainBufferCount;
	sd.OutputWindow = hwnd;
	sd.Windowed = true;
	sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	sd.Flags = 0;

	// Note: Swap chain uses queue to perform flush.
	CHECK_HR(D3dData::mDxgiFactory->CreateSwapChain(&cmdQueue, &sd, &swapChain));

	CHECK_HR(swapChain->QueryInterface(IID_PPV_ARGS(D3dData::mSwapChain.GetAddressOf())));

	// Set sRGB color space
	D3dData::mSwapChain->SetColorSpace1(DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709);
	D3dData::mSwapChain->SetFullscreenState(Settings::sFullscreen, nullptr);

	// Resize the swap chain.
	CHECK_HR(D3dData::mSwapChain->ResizeBuffers(Settings::sSwapChainBufferCount, Settings::sWindowWidth, Settings::sWindowHeight, Settings::sBackBufferFormat, 0U));
}