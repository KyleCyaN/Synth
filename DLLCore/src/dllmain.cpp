#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <dxgi1_2.h>
#include "MinHook.h"
#include "imgui.h"
#include <chrono>
#include <atomic>

#include "imgui_internal.h"
#include "Input.h"
#include "memory.h"
#include "menu.h"
#include "backends/imgui_impl_dx11.h"
#include "../Shared/shared.h"

#pragma comment(lib, "MinHook.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static ID3D11RenderTargetView* g_pMainRTV = nullptr;
static bool g_ImGuiInit = false;
static HWND g_hWnd = nullptr;

static HMODULE g_hModule = nullptr;
static std::atomic<bool> g_IsUnloading{false};

static HANDLE g_hMapFile = nullptr;
static SharedData* g_pShared = nullptr;

typedef HRESULT (WINAPI*Present_t)(IDXGISwapChain*, UINT, UINT);
Present_t OriginalPresent = nullptr;
typedef HRESULT (WINAPI*ResizeBuffers_t)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
ResizeBuffers_t OriginalResizeBuffers = nullptr;

void Log(const char* msg)
{
    OutputDebugStringA(msg);
}

void UpdateWindowTransparency()
{
    if (fabs(g_WindowAlpha - g_TargetAlpha) > 0.001f)
    {
        g_WindowAlpha += (g_TargetAlpha - g_WindowAlpha) *
            g_AlphaTransitionSpeed * ImGui::GetIO().DeltaTime;
        g_WindowAlpha = fmaxf(0.0f, fminf(1.0f, g_WindowAlpha));
        ImGuiStyle& style = ImGui::GetStyle();
        style.Alpha = g_WindowAlpha;
    }
}

void UpdateMouse(ImGuiIO& io)
{
    uintptr_t mouseFlagsAddr = Memory::ResolveAddress("WindowsEntryPoint.Windows_W10.exe+01C78D38->0x0->0x28->0x28");

    if (!mouseFlagsAddr)
        return;

    const int mouseFlagsValue = *(int*)mouseFlagsAddr;
    const bool curDown = (mouseFlagsValue != 0);

    static bool lastDown = false;
    if (curDown != lastDown)
    {
        io.AddMouseButtonEvent(0, curDown);
        lastDown = curDown;
    }
}

void UpdateInput()
{
    ImGuiIO& io = ImGui::GetIO();
    GameInput* gi = GetGameInput();

    if (gi)
        io.MousePos = ImVec2(gi->mouseX, gi->mouseY);

    const bool wasFocused = g_ImGuiHasFocus;

    g_ImGuiHasFocus = io.WantCaptureMouse &&
    (ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) ||
        ImGui::IsAnyItemActive());

    if (g_ImGuiHasFocus != wasFocused)
        g_TargetAlpha = g_ImGuiHasFocus ? g_FocusedAlpha : g_UnfocusedAlpha;

    UpdateMouse(io);

    static auto last = std::chrono::high_resolution_clock::now();
    auto now = std::chrono::high_resolution_clock::now();
    io.DeltaTime = std::chrono::duration<float>(now - last).count();
    last = now;
}

Present_t GetPresentAddress()
{
    ID3D11Device* pDevice = nullptr;
    ID3D11DeviceContext* pContext = nullptr;
    IDXGISwapChain1* pSwapChain1 = nullptr;
    IDXGIFactory2* pFactory = nullptr;

    WNDCLASSEX wc = {
        sizeof(WNDCLASSEX), CS_CLASSDC, DefWindowProc, 0L, 0L,
        GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr,
        L"D3D11_Temp_Window", nullptr
    };
    RegisterClassEx(&wc);
    HWND hWnd = CreateWindowEx(0, wc.lpszClassName, L"", WS_OVERLAPPEDWINDOW,
                               CW_USEDEFAULT, CW_USEDEFAULT, 100, 100,
                               nullptr, nullptr, wc.hInstance, nullptr);
    if (!hWnd) return nullptr;

    D3D_FEATURE_LEVEL featureLevel;
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
                                   nullptr, 0, D3D11_SDK_VERSION,
                                   &pDevice, &featureLevel, &pContext);
    if (FAILED(hr) || !pDevice)
    {
        DestroyWindow(hWnd);
        UnregisterClass(wc.lpszClassName, wc.hInstance);
        return nullptr;
    }

    IDXGIDevice* pDXGIDevice = nullptr;
    if (FAILED(pDevice->QueryInterface(__uuidof(IDXGIDevice), (void**)&pDXGIDevice)))
    {
        pDevice->Release();
        pContext->Release();
        DestroyWindow(hWnd);
        UnregisterClass(wc.lpszClassName, wc.hInstance);
        return nullptr;
    }
    IDXGIAdapter* pAdapter = nullptr;
    pDXGIDevice->GetAdapter(&pAdapter);
    pDXGIDevice->Release();
    pAdapter->GetParent(__uuidof(IDXGIFactory2), (void**)&pFactory);
    pAdapter->Release();
    if (!pFactory)
    {
        pDevice->Release();
        pContext->Release();
        DestroyWindow(hWnd);
        UnregisterClass(wc.lpszClassName, wc.hInstance);
        return nullptr;
    }

    DXGI_SWAP_CHAIN_DESC1 desc = {};
    desc.Width = 0;
    desc.Height = 0;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.Stereo = FALSE;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.Scaling = DXGI_SCALING_NONE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    desc.Flags = 0;
    hr = pFactory->CreateSwapChainForHwnd(pDevice, hWnd, &desc, nullptr, nullptr, &pSwapChain1);
    pFactory->Release();
    if (FAILED(hr) || !pSwapChain1)
    {
        pDevice->Release();
        pContext->Release();
        DestroyWindow(hWnd);
        UnregisterClass(wc.lpszClassName, wc.hInstance);
        return nullptr;
    }

    IDXGISwapChain* pSwapChain = nullptr;
    pSwapChain1->QueryInterface(__uuidof(IDXGISwapChain), (void**)&pSwapChain);
    Present_t presentAddress = nullptr;
    if (pSwapChain)
    {
        void** vtable = *reinterpret_cast<void***>(pSwapChain);
        presentAddress = (vtable && vtable[8]) ? (Present_t)vtable[8] : nullptr;
        pSwapChain->Release();
    }
    pSwapChain1->Release();
    pDevice->Release();
    pContext->Release();
    DestroyWindow(hWnd);
    UnregisterClass(wc.lpszClassName, wc.hInstance);
    return presentAddress;
}

ResizeBuffers_t GetResizeBuffersAddress()
{
    ID3D11Device* pDevice = nullptr;
    ID3D11DeviceContext* pContext = nullptr;
    IDXGISwapChain1* pSwapChain1 = nullptr;
    IDXGIFactory2* pFactory = nullptr;

    const WNDCLASSEX wc = {
        sizeof(WNDCLASSEX), CS_CLASSDC, DefWindowProc, 0L, 0L,
        GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr,
        L"D3D11_Resize_Temp_Window", nullptr
    };
    RegisterClassEx(&wc);
    const HWND hWnd = CreateWindowEx(0, wc.lpszClassName, L"", WS_OVERLAPPEDWINDOW,
                                     CW_USEDEFAULT, CW_USEDEFAULT, 100, 100,
                                     nullptr, nullptr, wc.hInstance, nullptr);
    if (!hWnd) return nullptr;

    D3D_FEATURE_LEVEL featureLevel;
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
                                   nullptr, 0, D3D11_SDK_VERSION,
                                   &pDevice, &featureLevel, &pContext);
    if (FAILED(hr) || !pDevice)
    {
        DestroyWindow(hWnd);
        UnregisterClass(wc.lpszClassName, wc.hInstance);
        return nullptr;
    }

    IDXGIDevice* pDXGIDevice = nullptr;
    if (FAILED(pDevice->QueryInterface(__uuidof(IDXGIDevice), (void**)&pDXGIDevice)))
    {
        pDevice->Release();
        pContext->Release();
        DestroyWindow(hWnd);
        UnregisterClass(wc.lpszClassName, wc.hInstance);
        return nullptr;
    }
    IDXGIAdapter* pAdapter = nullptr;
    pDXGIDevice->GetAdapter(&pAdapter);
    pDXGIDevice->Release();
    pAdapter->GetParent(__uuidof(IDXGIFactory2), (void**)&pFactory);
    pAdapter->Release();
    if (!pFactory)
    {
        pDevice->Release();
        pContext->Release();
        DestroyWindow(hWnd);
        UnregisterClass(wc.lpszClassName, wc.hInstance);
        return nullptr;
    }

    DXGI_SWAP_CHAIN_DESC1 desc = {};
    desc.Width = 0;
    desc.Height = 0;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.Stereo = FALSE;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.Scaling = DXGI_SCALING_NONE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    desc.Flags = 0;
    hr = pFactory->CreateSwapChainForHwnd(pDevice, hWnd, &desc, nullptr, nullptr, &pSwapChain1);
    pFactory->Release();
    if (FAILED(hr) || !pSwapChain1)
    {
        pDevice->Release();
        pContext->Release();
        DestroyWindow(hWnd);
        UnregisterClass(wc.lpszClassName, wc.hInstance);
        return nullptr;
    }

    IDXGISwapChain* pSwapChain = nullptr;
    pSwapChain1->QueryInterface(__uuidof(IDXGISwapChain), (void**)&pSwapChain);
    ResizeBuffers_t resizeAddress = nullptr;
    if (pSwapChain)
    {
        void** vtable = *reinterpret_cast<void***>(pSwapChain);
        resizeAddress = (vtable && vtable[13]) ? (ResizeBuffers_t)vtable[13] : nullptr;
        pSwapChain->Release();
    }
    pSwapChain1->Release();
    pDevice->Release();
    pContext->Release();
    DestroyWindow(hWnd);
    UnregisterClass(wc.lpszClassName, wc.hInstance);
    return resizeAddress;
}

HRESULT WINAPI HookedResizeBuffers(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height,
                                   DXGI_FORMAT NewFormat, UINT SwapChainFlags)
{
    if (g_pMainRTV)
    {
        g_pMainRTV->Release();
        g_pMainRTV = nullptr;
    }
    HRESULT hr = OriginalResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
    if (SUCCEEDED(hr) && g_ImGuiInit)
    {
        ImGui_ImplDX11_InvalidateDeviceObjects();
    }
    return hr;
}

HRESULT WINAPI HookedPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
{
    if (g_IsUnloading)
        return OriginalPresent(pSwapChain, SyncInterval, Flags);

    DXGI_SWAP_CHAIN_DESC sd;
    pSwapChain->GetDesc(&sd);
    HWND currentHwnd = sd.OutputWindow;
    if (!currentHwnd)
        currentHwnd = FindWindowW(L"Windows.UI.Core.CoreWindow", nullptr);
    g_hWnd = currentHwnd;

    if (!g_ImGuiInit)
    {
        if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&g_pd3dDevice)))
        {
            g_pd3dDevice->GetImmediateContext(&g_pd3dDeviceContext);

            g_WindowAlpha = 1.0f;
            g_TargetAlpha = 1.0f;
            g_FocusedAlpha = 1.0f;
            g_UnfocusedAlpha = 0.5f;
            g_AlphaTransitionSpeed = 5.0f;

            ImGui::CreateContext();
            ImGuiIO& io = ImGui::GetIO();
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
            ImGui::GetStyle().Colors[ImGuiCol_NavHighlight] = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
            ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

            io.Fonts->AddFontFromFileTTF(R"(C:\Windows\Fonts\msyh.ttc)", 20.0f, nullptr,
                                         io.Fonts->GetGlyphRangesChineseFull());
            UIMenu::Initialize();
            g_ImGuiInit = true;
            Log("ImGui initialized (keyboard-only, no Win32 backend)");
        }
        if (!g_ImGuiInit) return OriginalPresent(pSwapChain, SyncInterval, Flags);
    }

    ID3D11Texture2D* pBackBuffer = nullptr;
    if (FAILED(pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer)))
        return OriginalPresent(pSwapChain, SyncInterval, Flags);

    D3D11_TEXTURE2D_DESC bbDesc;
    pBackBuffer->GetDesc(&bbDesc);
    const auto width = static_cast<float>(bbDesc.Width);
    const auto height = static_cast<float>(bbDesc.Height);

    if (!g_pMainRTV)
    {
        g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pMainRTV);
    }
    pBackBuffer->Release();

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(width, height);

    UpdateInput();

    UpdateWindowTransparency();

    ImGui_ImplDX11_NewFrame();
    ImGui::NewFrame();

    UIMenu::Render();
    ImGui::Render();

    if (g_pMainRTV)
    {
        ID3D11RenderTargetView* oldRTV = nullptr;
        ID3D11DepthStencilView* oldDSV = nullptr;
        g_pd3dDeviceContext->OMGetRenderTargets(1, &oldRTV, &oldDSV);

        ID3D11BlendState* oldBlendState = nullptr;
        FLOAT oldBlendFactor[4];
        UINT oldSampleMask = 0;
        g_pd3dDeviceContext->OMGetBlendState(&oldBlendState, oldBlendFactor, &oldSampleMask);

        UINT numViewports = 1;
        D3D11_VIEWPORT oldViewport;
        g_pd3dDeviceContext->RSGetViewports(&numViewports, &oldViewport);
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_pMainRTV, nullptr);

        D3D11_VIEWPORT vp = {0.0f, 0.0f, width, height, 0.0f, 1.0f};
        g_pd3dDeviceContext->RSSetViewports(1, &vp);

        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pd3dDeviceContext->OMSetRenderTargets(1, &oldRTV, oldDSV);
        g_pd3dDeviceContext->RSSetViewports(1, &oldViewport);
        g_pd3dDeviceContext->OMSetBlendState(oldBlendState, oldBlendFactor, oldSampleMask);

        if (oldRTV) oldRTV->Release();
        if (oldDSV) oldDSV->Release();
        if (oldBlendState) oldBlendState->Release();
    }

    return OriginalPresent(pSwapChain, SyncInterval, Flags);
}

bool InstallHooks()
{
    Log("Installing Hooks...");
    Present_t pPresentTarget = GetPresentAddress();
    if (!pPresentTarget)
    {
        Log("Failed to get Present address");
        return false;
    }

    ResizeBuffers_t pResizeTarget = GetResizeBuffersAddress();
    if (!pResizeTarget) Log("Warning: Failed to get ResizeBuffers address");

    if (MH_Initialize() != MH_OK)
    {
        Log("MH_Initialize failed");
        return false;
    }

    if (MH_CreateHook(pPresentTarget, &HookedPresent, (void**)&OriginalPresent) != MH_OK ||
        MH_EnableHook(pPresentTarget) != MH_OK)
    {
        Log("MH_CreateHook Present failed");
        return false;
    }
    if (pResizeTarget)
    {
        if (MH_CreateHook(pResizeTarget, &HookedResizeBuffers, (void**)&OriginalResizeBuffers) == MH_OK &&
            MH_EnableHook(pResizeTarget) == MH_OK)
            Log("ResizeBuffers hook installed");
    }
    Log("Hooks installed successfully");
    return true;
}

void ShutdownAndCleanup()
{
    Log("Shutdown started");
    g_IsUnloading = true;

    if (g_pShared)
    {
        g_pShared->unloadRequested = 2;
        Log("Sent unload confirmation to injector");
        Sleep(500);
    }

    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();

    if (g_ImGuiInit)
    {
        UIMenu::Shutdown();
        ImGui_ImplDX11_Shutdown();
        ImGui::DestroyContext();
        g_ImGuiInit = false;
    }

    if (g_pMainRTV)
    {
        g_pMainRTV->Release();
        g_pMainRTV = nullptr;
    }

    if (g_pd3dDeviceContext)
    {
        g_pd3dDeviceContext->Release();
        g_pd3dDeviceContext = nullptr;
    }

    if (g_pd3dDevice)
    {
        g_pd3dDevice->Release();
        g_pd3dDevice = nullptr;
    }

    Log("Shutdown finished");
}

DWORD WINAPI UnloadThread(LPVOID lpParam)
{
    auto hModule = static_cast<HMODULE>(lpParam);
    Log("UnloadThread started");

    for (int retry = 0; retry < 20; ++retry)
    {
        g_hMapFile = OpenFileMappingW(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, L"Local\\MySharedMemory");
        if (g_hMapFile) break;
        Sleep(500);
    }
    if (!g_hMapFile)
    {
        Log("OpenFileMapping failed in UnloadThread");
        return 0;
    }

    g_pShared = static_cast<SharedData*>(MapViewOfFile(g_hMapFile, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0,
                                                       sizeof(SharedData)));
    if (!g_pShared)
    {
        Log("MapViewOfFile failed in UnloadThread");
        CloseHandle(g_hMapFile);
        return 0;
    }
    Log("Shared memory connected in UnloadThread");

    while (g_pShared->unloadRequested != 1)
    {
        Sleep(500);
        if (g_pShared->unloadRequested == 2)
        {
            Log("Already unloaded, exiting UnloadThread");
            goto cleanup_exit;
        }
    }
    Log("Unload request received");

    g_IsUnloading = true;
    Sleep(500);

    ShutdownAndCleanup();

    if (g_pShared)
    {
        g_pShared->unloadRequested = 2;
        Log("Sent unload confirmation (value=2) to injector");
        Sleep(300);
    }

cleanup_exit:
    if (g_pShared) UnmapViewOfFile(g_pShared);
    if (g_hMapFile) CloseHandle(g_hMapFile);
    g_pShared = nullptr;
    g_hMapFile = nullptr;

    Log("UnloadThread exiting, calling FreeLibraryAndExitThread");

    FreeLibraryAndExitThread(hModule, 0);
}

DWORD WINAPI InitThread(LPVOID)
{
    Log("InitThread started");
    Sleep(500);
    InstallHooks();
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason, LPVOID lpReserved)
{
    if (ul_reason == DLL_PROCESS_ATTACH)
    {
        g_hModule = hModule;
        DisableThreadLibraryCalls(hModule);
        CloseHandle(CreateThread(nullptr, 0, InitThread, nullptr, 0, nullptr));
        CloseHandle(CreateThread(nullptr, 0, UnloadThread, hModule, 0, nullptr));
    }
    return TRUE;
}
