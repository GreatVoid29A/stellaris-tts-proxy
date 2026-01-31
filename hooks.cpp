// MIT License
//
// Copyright (c) 2026 4byssEcho
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "hooks.h"
#include "config.h"
#include "logger.h"
#include "utils.h"
#include "tts_processor.h"
#include "thread_pool.h"
#include "audio_cache.h"
#include "hotkey.h"
#include <Windows.h>
#include <io.h>
#include <fcntl.h>
#include <Shlwapi.h>

#pragma comment(lib, "libMinHook.x64.lib")

Speak_t oSpeak = nullptr;
SpeakStream_t oSpeakStream = nullptr;

// Initialization state
static std::atomic<bool> g_hooksInitialized{false};
static std::atomic<bool> g_shutdownRequested{false};
static std::atomic<bool> g_sapiHooksCreated{false};
static std::atomic<bool> g_hotkeyThreadCreated{false};
static HANDLE g_hotkeyThreadHandle = nullptr;
static HWND g_hwndTimer = nullptr;  // Hidden window for timer messages

// Forward declarations
static void CreateSAPIHooks();
static void CreateHotkeyThread();
static LRESULT CALLBACK TimerWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Helper to get the game executable directory
static std::string GetGameDirectory() {
    wchar_t modulePath[MAX_PATH];
    if (GetModuleFileNameW(NULL, modulePath, MAX_PATH) == 0) {
        return "";
    }
    PathRemoveFileSpecW(modulePath);

    // Properly convert wchar_t to UTF-8
    int size = WideCharToMultiByte(CP_UTF8, 0, modulePath, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return "";
    }
    std::string result(size - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, modulePath, -1, &result[0], size, nullptr, nullptr);
    return result;
}

// ---------------------------------------------------------
// HOOKED FUNCTIONS
// ---------------------------------------------------------

HRESULT __stdcall hkSpeak(ISpVoice* This, const WCHAR* pwcs, DWORD dwFlags, ULONG* pulStreamNumber) {
    (void)This;
    (void)dwFlags;

    // If hooks aren't ready yet, just pass through to original
    if (!g_sapiHooksCreated.load() || !oSpeak) {
        return oSpeak ? oSpeak(This, pwcs, dwFlags, pulStreamNumber) : S_OK;
    }

    if (IsValidStringPointer(pwcs)) {
        std::wstring textCopy(pwcs);
        // Parallel mode: non-blocking, multiple fetches happen concurrently
        ProcessTTSRequest(textCopy);
    }
    else {
        LOG_WARNING(L"Invalid string pointer in hkSpeak");
    }

    if (g_config.mute_original) {
        if (pulStreamNumber) {
            *pulStreamNumber = 0;
        }
        return S_OK;
    }
    else {
        return oSpeak(This, pwcs, dwFlags, pulStreamNumber);
    }
}

HRESULT __stdcall hkSpeakStream(ISpVoice* This, IStream* pStream, DWORD dwFlags, ULONG* pulStreamNumber) {
    return oSpeakStream(This, pStream, dwFlags, pulStreamNumber);
}

// ---------------------------------------------------------
// TIMER-BASED DEFERRED INITIALIZATION
// ---------------------------------------------------------

static void CreateSAPIHooks() {
    if (g_sapiHooksCreated.load()) {
        return;  // Already created
    }

    LOG_INFO(L"Creating SAPI hooks (deferred via timer)...");

    HRESULT hr = CoInitialize(NULL);
    if (FAILED(hr)) {
        LOG_ERROR(L"CoInitialize failed for deferred hook creation");
        return;
    }

    ISpVoice* pDummyVoice = nullptr;
    hr = CoCreateInstance(CLSID_SpVoice, NULL, CLSCTX_ALL, IID_ISpVoice, (void**)&pDummyVoice);

    if (FAILED(hr) || !pDummyVoice) {
        LOG_ERROR(L"Failed to create SAPI voice for deferred hook creation");
        return;
    }

    void** vtable = *(void***)pDummyVoice;
    void* pSpeakAddr = vtable[VTABLE_INDEX_SPEAK];
    void* pSpeakStreamAddr = vtable[VTABLE_INDEX_SPEAKSTREAM];

    if (MH_CreateHook(pSpeakAddr, &hkSpeak, (LPVOID*)&oSpeak) == MH_OK) {
        MH_EnableHook(pSpeakAddr);
        LOG_INFO(L"Speak hook installed (deferred)");
    }

    if (MH_CreateHook(pSpeakStreamAddr, &hkSpeakStream, (LPVOID*)&oSpeakStream) == MH_OK) {
        MH_EnableHook(pSpeakStreamAddr);
        LOG_INFO(L"SpeakStream hook installed (deferred)");
    }

    pDummyVoice->Release();
    g_sapiHooksCreated.store(true);
    LOG_INFO(L"SAPI hooks created successfully - TTS interception active!");
}

// Create hotkey thread in a deferred manner (safe for launcher)
static void CreateHotkeyThread() {
    if (g_hotkeyThreadCreated.load()) {
        return;
    }

    LOG_INFO(L"Creating hotkey monitor thread (deferred via timer)...");
    g_hotkeyThreadHandle = CreateThread(nullptr, 0, HotkeyMonitorThread, nullptr, 0, nullptr);

    if (g_hotkeyThreadHandle) {
        g_hotkeyThreadCreated.store(true);
        LOG_INFO(L"Hotkey thread created successfully");
    } else {
        LOG_ERROR(L"Failed to create hotkey thread");
    }
}

// Window procedure for hidden timer window
static LRESULT CALLBACK TimerWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    (void)hwnd;
    (void)lParam;

    if (msg == WM_TIMER && wParam == 1) {
        KillTimer(hwnd, 1);
        CreateSAPIHooks();
        CreateHotkeyThread();  // Create hotkey thread after SAPI is ready
        return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// ---------------------------------------------------------
// INITIALIZATION
// ---------------------------------------------------------

void SetupHooks() {
    Sleep(500);

    g_config.SetDefaults();

    // Console setup - BEFORE any logging (required for std::wcout to work)
    if (g_config.show_console) {
        AllocConsole();
        FILE* f;
        freopen_s(&f, "CONOUT$", "w", stdout);
        freopen_s(&f, "CONOUT$", "w", stderr);
        _setmode(_fileno(stdout), _O_U8TEXT);
        std::wcout.clear();
    }

    LOG_INFO(L"Initializing Stellaris TTS Replacement...");

    std::string gameDir = GetGameDirectory();
    if (!gameDir.empty()) {
        std::string configPath = gameDir + "\\tts_settings.txt";
        LoadConfig(configPath);
    } else {
        LoadConfig("tts_settings.txt");
    }

    g_audioCache.SetMaxSize(g_config.max_cache_size);
    g_audioCache.Initialize();

    // Initialize parallel TTS system if enabled
    InitializeParallelSystem();

    if (MH_Initialize() != MH_OK) {
        LOG_ERROR(L"MinHook Init Failed");
        return;
    }

    // Create hidden window for timer-based deferred SAPI initialization
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = TimerWindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"TTSStellarisTimerWindow";

    RegisterClassW(&wc);
    g_hwndTimer = CreateWindowW(
        L"TTSStellarisTimerWindow",
        L"TTS Timer",
        0,
        0, 0, 0, 0,
        HWND_MESSAGE,
        NULL,
        GetModuleHandle(NULL),
        NULL
    );

    if (g_hwndTimer) {
        // Set timer to create SAPI hooks after 3 seconds
        SetTimer(g_hwndTimer, 1, 3000, NULL);
        LOG_INFO(L"Timer set for deferred SAPI hook creation (3 seconds)");
    } else {
        LOG_ERROR(L"Failed to create timer window");
    }

    LOG_INFO(L"SetupHooks complete - SAPI hooks will be created after game starts");

    // NOTE: Hotkey thread will be created via timer (deferred initialization)
    // This avoids launcher crash from thread creation during DllMain

    g_hooksInitialized.store(true);
}

// Shutdown function - called from DLL_PROCESS_DETACH
void ShutdownHooks() {
    LOG_INFO(L"Shutting down hooks...");

    g_shutdownRequested.store(true);

    // Clean up timer window
    if (g_hwndTimer) {
        KillTimer(g_hwndTimer, 1);
        DestroyWindow(g_hwndTimer);
        g_hwndTimer = nullptr;
    }

    // Signal hotkey thread to shutdown (non-blocking)
    if (g_hotkeyThreadCreated.load()) {
        SignalHotkeyThreadShutdown();
    }

    // Fast shutdown: signal thread pool but don't wait
    // The OS will clean up threads when process terminates
    g_ttsThreadPool.ShutdownFast();

    // Shutdown parallel TTS system
    ShutdownParallelSystem();

    // Close hotkey thread handle without waiting (fast shutdown)
    if (g_hotkeyThreadHandle) {
        CloseHandle(g_hotkeyThreadHandle);
        g_hotkeyThreadHandle = nullptr;
    }

    LOG_INFO(L"Shutdown complete");
}
