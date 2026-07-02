#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <dxgi1_2.h>
#include <dwmapi.h>

#include <atomic>
#include <chrono>

#include "MinHook.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_dx11.h"

#include "features.h"
#include "memory.h"
#include "menu.h"
#include "../Shared/shared.h"

#pragma comment(lib, "MinHook.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "user32.lib")

static ID3D11Device           *g_pd3dDevice         = nullptr;
static ID3D11DeviceContext    *g_pd3dDeviceContext   = nullptr;
static ID3D11RenderTargetView *g_pMainRTV            = nullptr;
static bool                    g_ImGuiInit           = false;
static HWND                    g_hFrameWnd           = nullptr;
static HWND                    g_hCoreContentWnd     = nullptr;
static DWORD                   g_Pid                 = 0;

static HMODULE       g_hModule      = nullptr;
static std::atomic<bool> g_IsUnloading{false};

static HANDLE      g_hMapFile  = nullptr;
static SharedData *g_pShared   = nullptr;

static HHOOK                g_hMouseHook          = nullptr;
static std::atomic   g_MouseWheelAccum{0.0f};
static std::atomic    g_MouseHookQuit{false};
static DWORD                g_dwMouseHookThreadId = 0;

static LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION && wParam == WM_MOUSEWHEEL) {
        const auto *p = reinterpret_cast<MSLLHOOKSTRUCT *>(lParam);
        g_MouseWheelAccum.fetch_add(
            static_cast<float>(GET_WHEEL_DELTA_WPARAM(p->mouseData))
                / static_cast<float>(WHEEL_DELTA));
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

static DWORD WINAPI MouseHookThread(LPVOID)
{
    g_hMouseHook = SetWindowsHookExW(WH_MOUSE_LL, LowLevelMouseProc,
                                      g_hModule, 0);
    if (!g_hMouseHook)
        return 1;

    g_dwMouseHookThreadId = GetCurrentThreadId();

    MSG msg;
    while (!g_MouseHookQuit.load() && GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (g_hMouseHook) {
        UnhookWindowsHookEx(g_hMouseHook);
        g_hMouseHook = nullptr;
    }
    return 0;
}

static void InitMouseHook()
{
    CloseHandle(CreateThread(nullptr, 0, MouseHookThread, nullptr, 0, nullptr));
}

static void ShutdownMouseHook()
{
    g_MouseHookQuit.store(true);
    if (g_dwMouseHookThreadId)
        PostThreadMessageW(g_dwMouseHookThreadId, WM_QUIT, 0, 0);
    for (int i = 0; i < 10 && g_hMouseHook; ++i)
        Sleep(50);
}

static void ApplyMouseWheel()
{
    const float delta = g_MouseWheelAccum.exchange(0.0f);
    if (delta != 0.0f)
        ImGui::GetIO().MouseWheel += delta;
}

using Present_t       = HRESULT (WINAPI *)(IDXGISwapChain *, UINT, UINT);
using ResizeBuffers_t = HRESULT (WINAPI *)(IDXGISwapChain *, UINT, UINT, UINT,
                                           DXGI_FORMAT, UINT);

static Present_t        OriginalPresent        = nullptr;
static ResizeBuffers_t  OriginalResizeBuffers  = nullptr;

static auto g_BackBufferSize = ImVec2(0.0f, 0.0f);

static constexpr LONG RectArea(const RECT &r) noexcept {
    return (r.right - r.left) * (r.bottom - r.top);
}

static BOOL CALLBACK FindFrameWindowProc(const HWND hWnd, const LPARAM lp) {
    WCHAR cls[128];
    if (!GetClassNameW(hWnd, cls, 128))
        return TRUE;
    if (wcscmp(cls, L"ApplicationFrameWindow") != 0)
        return TRUE;

    RECT r;
    if (!GetWindowRect(hWnd, &r))
        return TRUE;

    const LONG area = RectArea(r);
    if (area < 10000)
        return TRUE;

    WCHAR title[256];
    GetWindowTextW(hWnd, title, 255);

    if (wcsstr(title, L"Modern Combat 5") == nullptr &&
        wcsstr(title, L"Modern Combat") == nullptr)
        return TRUE;

    auto *const result = reinterpret_cast<HWND *>(lp);
    if (*result == nullptr) {
        *result = hWnd;
        return TRUE;
    }

    RECT existing;
    if (GetWindowRect(*result, &existing)) {
        if (area > RectArea(existing))
            *result = hWnd;
    }
    return TRUE;
}

static HWND FindGameFrameWindow() {
    HWND result = nullptr;
    EnumWindows(FindFrameWindowProc, reinterpret_cast<LPARAM>(&result));

    if (result)
        return result;

    struct FallbackCtx {
        HWND best = nullptr;
        LONG bestArea = 0;
    } ctx;

    EnumWindows([](const HWND hWnd, const LPARAM lp) -> BOOL {
        auto *const c = reinterpret_cast<FallbackCtx *>(lp);

        WCHAR cls[128];
        if (!GetClassNameW(hWnd, cls, 128))
            return TRUE;
        if (wcscmp(cls, L"ApplicationFrameWindow") != 0)
            return TRUE;

        RECT r;
        if (!GetWindowRect(hWnd, &r))
            return TRUE;

        const LONG area = RectArea(r);
        if (area > c->bestArea) {
            c->best = hWnd;
            c->bestArea = area;
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&ctx));

    return ctx.best;
}

struct FindCoreCtx {
    HWND bestHwnd = nullptr;
    LONG bestArea = 0;
    RECT frameBounds{};
};

static BOOL CALLBACK FindCoreChildProc(const HWND hWnd, const LPARAM lp) {
    auto *const ctx = reinterpret_cast<FindCoreCtx *>(lp);

    WCHAR cls[128];
    if (!GetClassNameW(hWnd, cls, 128))
        return TRUE;

    const bool isCoreClass = (wcsstr(cls, L"CoreWindow") != nullptr ||
                              wcsstr(cls, L"Windows.UI.Core") != nullptr);

    RECT r;
    if (!GetWindowRect(hWnd, &r))
        return TRUE;

    const LONG w = r.right - r.left;
    const LONG h = r.bottom - r.top;
    if (w <= 0 || h <= 0)
        return TRUE;

    const LONG relTop = r.top - ctx->frameBounds.top;
    if (relTop < 0)
        return TRUE;

    const LONG area = w * h;

    if (isCoreClass) {
        if (area > ctx->bestArea) {
            ctx->bestHwnd = hWnd;
            ctx->bestArea = area;
        }
    } else if (ctx->bestHwnd == nullptr && relTop > 0 && relTop < 200) {
        if (area > ctx->bestArea) {
            ctx->bestHwnd = hWnd;
            ctx->bestArea = area;
        }
    }
    return TRUE;
}

static void InitCoreContentWindow() {
    if (!g_hFrameWnd)
        return;
    if (g_hCoreContentWnd && IsWindow(g_hCoreContentWnd))
        return;

    RECT frameBounds;
    if (FAILED(DwmGetWindowAttribute(g_hFrameWnd,
        DWMWA_EXTENDED_FRAME_BOUNDS,
        &frameBounds, sizeof(frameBounds)))) {
        GetWindowRect(g_hFrameWnd, &frameBounds);
    }

    FindCoreCtx ctx;
    ctx.frameBounds = frameBounds;
    EnumChildWindows(g_hFrameWnd, FindCoreChildProc, reinterpret_cast<LPARAM>(&ctx));

    if (ctx.bestHwnd && IsWindow(ctx.bestHwnd))
        g_hCoreContentWnd = ctx.bestHwnd;
}

static ImVec2 GetCorrectedMousePos() {
    if (!g_hCoreContentWnd || !IsWindow(g_hCoreContentWnd))
        InitCoreContentWindow();

    POINT cursor;
    GetCursorPos(&cursor);

    if (g_hCoreContentWnd) {
        RECT cr;
        if (GetWindowRect(g_hCoreContentWnd, &cr)) {
            const LONG cw = cr.right - cr.left;
            const LONG ch = cr.bottom - cr.top;

            if (cw > 0 && ch > 0 && g_BackBufferSize.x > 0.0f && g_BackBufferSize.y > 0.0f) {
                const auto rx = static_cast<float>(cursor.x - cr.left);
                const auto ry = static_cast<float>(cursor.y - cr.top);
                return {
                    rx * (g_BackBufferSize.x / static_cast<float>(cw)),
                    ry * (g_BackBufferSize.y / static_cast<float>(ch))
                };
            }
        }
    }

    if (g_hFrameWnd) {
        RECT bounds;
        if (FAILED(DwmGetWindowAttribute(g_hFrameWnd,
            DWMWA_EXTENDED_FRAME_BOUNDS, &bounds, sizeof(bounds)))) {
            GetWindowRect(g_hFrameWnd, &bounds);
        }

        const auto rx = static_cast<float>(cursor.x - bounds.left);
        const auto ry = static_cast<float>(cursor.y - bounds.top);
        const auto bw = static_cast<float>(bounds.right - bounds.left);

        if (const auto bh = static_cast<float>(bounds.bottom - bounds.top);
            bw > 0.0f && bh > 0.0f && g_BackBufferSize.x > 0.0f && g_BackBufferSize.y > 0.0f)
            return {
                rx * (g_BackBufferSize.x / bw),
                ry * (g_BackBufferSize.y / bh)
            };
    }

    return {-FLT_MAX, -FLT_MAX};
}

static void UpdateMouse(ImGuiIO &io) {
    static bool lastDown = false;

    const uintptr_t mouseFlagsAddr = Memory::ResolveAddress(MOUSE_STATUS_EXPR);
    if (!mouseFlagsAddr) {
        io.MouseDown[0] = lastDown;
        io.AddMouseButtonEvent(0, lastDown);
        return;
    }

    const bool curDown = (*reinterpret_cast<int *>(mouseFlagsAddr)) != 0;
    if (curDown != lastDown) {
        io.AddMouseButtonEvent(0, curDown);
        lastDown = curDown;
    }
    io.MouseDown[0] = curDown;
}

static void UpdateInput() {
    ImGuiIO &io = ImGui::GetIO();

    ApplyMouseWheel();

    io.MousePos = GetCorrectedMousePos();
    UpdateMouse(io);

    if (ImGui::IsMouseDragging(0)) {
        io.WantCaptureMouse = true;
        g_ImGuiHasFocus = true;
        g_TargetAlpha = g_FocusedAlpha;
    } else {
        const bool wasFocused = g_ImGuiHasFocus;
        g_ImGuiHasFocus = io.WantCaptureMouse &&
                          (ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) ||
                           ImGui::IsAnyItemActive());
        if (g_ImGuiHasFocus != wasFocused)
            g_TargetAlpha = g_ImGuiHasFocus ? g_FocusedAlpha : g_UnfocusedAlpha;
    }

    static auto last = std::chrono::high_resolution_clock::now();
    const auto now = std::chrono::high_resolution_clock::now();
    io.DeltaTime = std::chrono::duration<float>(now - last).count();
    last = now;
}

static void UpdateWindowTransparency() {
    if (fabs(g_WindowAlpha - g_TargetAlpha) > 0.001f) {
        g_WindowAlpha += (g_TargetAlpha - g_WindowAlpha)
                * g_AlphaTransitionSpeed * ImGui::GetIO().DeltaTime;
        g_WindowAlpha = fmaxf(0.0f, fminf(1.0f, g_WindowAlpha));
        ImGui::GetStyle().Alpha = g_WindowAlpha;
    }
}

template<int VtableIdx>
static void *GetSwapChainVtableFunc() {
    ID3D11Device        *pDevice     = nullptr;
    ID3D11DeviceContext *pContext    = nullptr;
    IDXGISwapChain1     *pSwapChain1 = nullptr;
    IDXGIFactory2       *pFactory    = nullptr;

    const WNDCLASSEXW wc = {
        sizeof(WNDCLASSEXW), CS_CLASSDC, DefWindowProcW, 0L, 0L,
        GetModuleHandleW(nullptr), nullptr, nullptr, nullptr, nullptr,
        L"D3D11_VTable", nullptr
    };
    RegisterClassExW(&wc);
    HWND hWnd = CreateWindowExW(0, wc.lpszClassName, L"", WS_OVERLAPPEDWINDOW,
                                CW_USEDEFAULT, CW_USEDEFAULT, 100, 100,
                                nullptr, nullptr, wc.hInstance, nullptr);
    if (!hWnd)
        return nullptr;

    D3D_FEATURE_LEVEL fl;
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        nullptr, 0, D3D11_SDK_VERSION,
        &pDevice, &fl, &pContext))) {
        DestroyWindow(hWnd);
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return nullptr;
    }

    IDXGIDevice *pDXGI = nullptr;
    IDXGIAdapter *pAdapter = nullptr;
    pDevice->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void **>(&pDXGI));
    pDXGI->GetAdapter(&pAdapter);
    pDXGI->Release();
    pAdapter->GetParent(__uuidof(IDXGIFactory2), reinterpret_cast<void **>(&pFactory));
    pAdapter->Release();

    if (!pFactory) {
        pDevice->Release();
        pContext->Release();
        DestroyWindow(hWnd);
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return nullptr;
    }

    DXGI_SWAP_CHAIN_DESC1 desc = {};
    desc.Width      = 0;
    desc.Height     = 0;
    desc.Format     = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage      = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount      = 2;
    desc.Scaling          = DXGI_SCALING_NONE;
    desc.SwapEffect       = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    pFactory->CreateSwapChainForHwnd(pDevice, hWnd, &desc, nullptr, nullptr, &pSwapChain1);
    pFactory->Release();

    if (!pSwapChain1) {
        pDevice->Release();
        pContext->Release();
        DestroyWindow(hWnd);
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return nullptr;
    }

    IDXGISwapChain *pSC = nullptr;
    pSwapChain1->QueryInterface(__uuidof(IDXGISwapChain), reinterpret_cast<void **>(&pSC));
    void *r = nullptr;
    if (pSC) {
        void **const v = *reinterpret_cast<void ***>(pSC);
        r = v[VtableIdx];
        pSC->Release();
    }
    pSwapChain1->Release();
    pDevice->Release();
    pContext->Release();
    DestroyWindow(hWnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return r;
}

static HRESULT WINAPI HookedResizeBuffers(IDXGISwapChain *const pSwapChain,
                                          const UINT bc, const UINT w, const UINT h,
                                          const DXGI_FORMAT fmt, const UINT flags) {
    if (g_pMainRTV) {
        g_pMainRTV->Release();
        g_pMainRTV = nullptr;
    }
    if (g_ImGuiInit)
        ImGui_ImplDX11_InvalidateDeviceObjects();
    return OriginalResizeBuffers(pSwapChain, bc, w, h, fmt, flags);
}

static HRESULT WINAPI HookedPresent(IDXGISwapChain *const pSwapChain,
                                    const UINT SyncInterval, const UINT Flags) {
    if (g_IsUnloading)
        return OriginalPresent(pSwapChain, SyncInterval, Flags);

    if (!g_hFrameWnd) {
        g_hFrameWnd = FindGameFrameWindow();
        if (g_hFrameWnd)
            InitCoreContentWindow();
    }

    if (!g_ImGuiInit) {
        if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D11Device),
            reinterpret_cast<void **>(&g_pd3dDevice)))) {
            g_pd3dDevice->GetImmediateContext(&g_pd3dDeviceContext);

            g_WindowAlpha           = 1.0f;
            g_TargetAlpha           = 1.0f;
            g_FocusedAlpha          = 1.0f;
            g_UnfocusedAlpha        = 0.5f;
            g_AlphaTransitionSpeed  = 5.0f;

            ImGui::CreateContext();
            ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);
            UIMenu::Initialize();
            g_ImGuiInit = true;

            DXGI_SWAP_CHAIN_DESC sd;
            pSwapChain->GetDesc(&sd);
            if (sd.OutputWindow)
                GetWindowThreadProcessId(sd.OutputWindow, &g_Pid);
        }
        if (!g_ImGuiInit)
            return OriginalPresent(pSwapChain, SyncInterval, Flags);
    }

    ID3D11Texture2D *pBB = nullptr;
    if (FAILED(pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D),
        reinterpret_cast<void **>(&pBB))))
        return OriginalPresent(pSwapChain, SyncInterval, Flags);

    D3D11_TEXTURE2D_DESC bbDesc;
    pBB->GetDesc(&bbDesc);
    g_BackBufferSize = ImVec2(static_cast<float>(bbDesc.Width),
                              static_cast<float>(bbDesc.Height));

    if (!g_pMainRTV)
        g_pd3dDevice->CreateRenderTargetView(pBB, nullptr, &g_pMainRTV);
    pBB->Release();

    if (!g_pMainRTV)
        return OriginalPresent(pSwapChain, SyncInterval, Flags);

    ImGuiIO &io = ImGui::GetIO();
    io.DisplaySize = g_BackBufferSize;
    UpdateInput();
    UpdateWindowTransparency();
    ImGui_ImplDX11_NewFrame();
    ImGui::NewFrame();
    UIMenu::Render();
    ImGui::Render();

    ID3D11RenderTargetView   *oldRTV = nullptr;
    ID3D11DepthStencilView   *oldDSV = nullptr;
    g_pd3dDeviceContext->OMGetRenderTargets(1, &oldRTV, &oldDSV);

    ID3D11BlendState *oldBS = nullptr;
    FLOAT bf[4];
    UINT  sm;
    g_pd3dDeviceContext->OMGetBlendState(&oldBS, bf, &sm);

    UINT nvp = 1;
    D3D11_VIEWPORT ovp;
    g_pd3dDeviceContext->RSGetViewports(&nvp, &ovp);

    g_pd3dDeviceContext->OMSetRenderTargets(1, &g_pMainRTV, nullptr);

    const D3D11_VIEWPORT vp = {
        0.0f, 0.0f,
        g_BackBufferSize.x, g_BackBufferSize.y,
        0.0f, 1.0f
    };
    g_pd3dDeviceContext->RSSetViewports(1, &vp);

    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    g_pd3dDeviceContext->OMSetRenderTargets(1, &oldRTV, oldDSV);
    g_pd3dDeviceContext->RSSetViewports(1, &ovp);
    g_pd3dDeviceContext->OMSetBlendState(oldBS, bf, sm);

    if (oldRTV) oldRTV->Release();
    if (oldDSV) oldDSV->Release();
    if (oldBS)  oldBS->Release();

    return OriginalPresent(pSwapChain, SyncInterval, Flags);
}

static bool InstallHooks() {
    const auto pPres   = reinterpret_cast<Present_t>(GetSwapChainVtableFunc<8>());
    const auto pResize = reinterpret_cast<ResizeBuffers_t>(GetSwapChainVtableFunc<13>());

    if (!pPres)
        return false;
    if (MH_Initialize() != MH_OK)
        return false;
    if (MH_CreateHook(reinterpret_cast<void *>(pPres),
                      reinterpret_cast<void *>(&HookedPresent),
                      reinterpret_cast<void **>(&OriginalPresent)) != MH_OK)
        return false;
    MH_EnableHook(reinterpret_cast<void *>(pPres));

    if (pResize) {
        if (MH_CreateHook(reinterpret_cast<void *>(pResize),
                          reinterpret_cast<void *>(&HookedResizeBuffers),
                          reinterpret_cast<void **>(&OriginalResizeBuffers)) == MH_OK)
            MH_EnableHook(reinterpret_cast<void *>(pResize));
    }
    return true;
}

static void ShutdownAndCleanup() {
    g_IsUnloading = true;

    ShutdownMouseHook();

    if (g_pShared) {
        g_pShared->unloadRequested = 2;
        Sleep(500);
    }
    MH_DisableHook(nullptr);
    MH_Uninitialize();

    if (g_ImGuiInit) {
        UIMenu::Shutdown();
        ImGui_ImplDX11_Shutdown();
        ImGui::DestroyContext();
        g_ImGuiInit = false;
    }
    if (g_pMainRTV) {
        g_pMainRTV->Release();
        g_pMainRTV = nullptr;
    }
    if (g_pd3dDeviceContext) {
        g_pd3dDeviceContext->Release();
        g_pd3dDeviceContext = nullptr;
    }
    if (g_pd3dDevice) {
        g_pd3dDevice->Release();
        g_pd3dDevice = nullptr;
    }
}

static DWORD WINAPI UnloadThread(LPVOID lp) {
    const auto hMod = static_cast<HMODULE>(lp);

    for (int i = 0; i < 20; ++i) {
        g_hMapFile = OpenFileMappingW(FILE_MAP_READ | FILE_MAP_WRITE,
                                      FALSE, L"Local\\MySharedMemory");
        if (g_hMapFile) break;
        Sleep(500);
    }
    if (!g_hMapFile)
        return 0;

    g_pShared = static_cast<SharedData *>(MapViewOfFile(
        g_hMapFile, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, sizeof(SharedData)));
    if (!g_pShared) {
        CloseHandle(g_hMapFile);
        return 0;
    }

    while (g_pShared->unloadRequested != 1) {
        Sleep(500);
        if (g_pShared->unloadRequested == 2)
            goto exit;
    }

    g_IsUnloading = true;
    Sleep(500);
    ShutdownAndCleanup();

    if (g_pShared)
        g_pShared->unloadRequested = 2;
    Sleep(300);

exit:
    if (g_pShared)
        UnmapViewOfFile(g_pShared);
    if (g_hMapFile)
        CloseHandle(g_hMapFile);
    g_pShared   = nullptr;
    g_hMapFile  = nullptr;
    FreeLibraryAndExitThread(hMod, 0);
}

static DWORD WINAPI InitThread(LPVOID) {
    Sleep(500);
    InstallHooks();
    InitMouseHook();
    return 0;
}

BOOL APIENTRY DllMain(const HMODULE hMod, const DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_hModule = hMod;
        DisableThreadLibraryCalls(hMod);
        CloseHandle(CreateThread(nullptr, 0, InitThread,   nullptr, 0, nullptr));
        CloseHandle(CreateThread(nullptr, 0, UnloadThread, hMod,    0, nullptr));
    }
    return TRUE;
}