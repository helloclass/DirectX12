//***************************************************************************************
// d3dApp.h by Frank Luna (C) 2015 All Rights Reserved.
//***************************************************************************************

#pragma once

#if defined(DEBUG) || defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#include "d3dUtil.h"
#include "GameTimer.h"

#include "Camera.h"

//# define _USE_UBER_SHADER

// Link necessary d3d12 libraries.
#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")

class D3DApp
{
protected:

    D3DApp(HINSTANCE hInstance);
    D3DApp(const D3DApp& rhs) = delete;
    D3DApp& operator=(const D3DApp& rhs) = delete;
    virtual ~D3DApp();

public:

    static D3DApp* GetApp();
    
	HINSTANCE AppInst()const;
	HWND      MainWnd()const;
	float     AspectRatio()const;

    bool Get4xMsaaState()const;
    void Set4xMsaaState(bool value);

	int Run();
 
    virtual bool Initialize();
    virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

protected:
    virtual void CreateRtvAndDsvDescriptorHeaps();
	virtual void OnResize(); 
	virtual void Update(const GameTimer& gt)=0;
    virtual void Draw(const GameTimer& gt)=0;

	// Convenience overrides for handling mouse input.
	virtual void OnMouseDown(WPARAM btnState, int x, int y){ }
	virtual void OnMouseUp(WPARAM btnState, int x, int y)  { }
	virtual void OnMouseMove(WPARAM btnState, int x, int y){ }

protected:

	bool InitMainWindow();
	bool InitDirect3D();
	void CreateCommandObjects();
    void CreateSwapChain();

	void FlushCommandQueue();

	static ID3D12Resource* CurrentBackBuffer();
	static D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView();
	static D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView();

	void CalculateFrameStats();

    void LogAdapters();
    void LogAdapterOutputs(IDXGIAdapter* adapter);
    void LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format);

protected:

    static D3DApp* mApp;

    HINSTANCE mhAppInst = nullptr; // application instance handle
    HWND      mhMainWnd = nullptr; // main window handle
	bool      mAppPaused = false;  // is the application paused?
	bool      mMinimized = false;  // is the application minimized?
	bool      mMaximized = false;  // is the application maximized?
	bool      mResizing = false;   // are the resize bars being dragged?
    bool      mFullscreenState = false;// fullscreen enabled

	// Set true to use 4X MSAA (?.1.8).  The default is false.
    bool      m4xMsaaState = false;    // 4X MSAA enabled
    UINT      m4xMsaaQuality = 0;      // quality level of 4X MSAA

	// Used to keep track of the �delta-time?and game time (?.4).
	GameTimer mTimer;
	
    Microsoft::WRL::ComPtr<IDXGIFactory4> mdxgiFactory;
    Microsoft::WRL::ComPtr<IDXGISwapChain> mSwapChain;
#ifdef _USE_UBER_SHADER
	// ����Ʈ���̽� ������������ ������ �� ����
	Microsoft::WRL::ComPtr<ID3D12Device5> md3dDevice;
#else // _USE_UBER_SHADER
	Microsoft::WRL::ComPtr<ID3D12Device> md3dDevice;
#endif

    static Microsoft::WRL::ComPtr<ID3D12Fence> mFence;
    static UINT64 mCurrentFence;
	
    static Microsoft::WRL::ComPtr<ID3D12CommandQueue> mCommandQueue;
    static Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mDirectCmdListAlloc;
	static Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mSSAOCmdListAlloc;
	// Non-Uber Shader CommandList
    static Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCommandList;
	static Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mSSAOCommandList;

#ifdef _USE_UBER_SHADER
	// Uber Shader CommandList
	static Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> mCommandList4;
	static Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> D3DApp::mSSAOCommandList4;
#endif

	static Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mMultiCmdListAlloc[8];
	static Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mMultiCommandList[8];
#ifdef _USE_UBER_SHADER
	// Uber Shader CommandList
	static Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> mMultiCommandList4[3];
#endif

	static Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mMultiShadowCmdListAlloc[8];
	static Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mMultiShadowCommandList[8];

	static const int SwapChainBufferCount = 2;
	static int mCurrBackBuffer;
    static Microsoft::WRL::ComPtr<ID3D12Resource> mSwapChainBuffer[SwapChainBufferCount];
    static Microsoft::WRL::ComPtr<ID3D12Resource> mDepthStencilBuffer;

    static Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mRtvHeap;
    static Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mDsvHeap;

    static D3D12_VIEWPORT mScreenViewport; 
    static D3D12_RECT mScissorRect;

	static UINT mRtvDescriptorSize;
	static UINT mDsvDescriptorSize;
	static UINT mCbvSrvUavDescriptorSize;

	// Derived class should set these in derived constructor to customize starting values.
	std::wstring mMainWndCaption = L"d3d App";
	D3D_DRIVER_TYPE md3dDriverType = D3D_DRIVER_TYPE_HARDWARE;
    DXGI_FORMAT mBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    DXGI_FORMAT mDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	int mClientWidth = 800;
	int mClientHeight = 600;
};
