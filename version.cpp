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

#include <windows.h>
#include <atomic>

#include "hooks.h"

// Macro to forward exports to the real version.dll
#define PROXY_EXPORT(func) __pragma(comment(linker, "/export:" #func "=C:\\Windows\\System32\\version." #func))

// Common version.dll exports
PROXY_EXPORT(GetFileVersionInfoA)
PROXY_EXPORT(GetFileVersionInfoByHandle)
PROXY_EXPORT(GetFileVersionInfoExA)
PROXY_EXPORT(GetFileVersionInfoExW)
PROXY_EXPORT(GetFileVersionInfoSizeA)
PROXY_EXPORT(GetFileVersionInfoSizeExA)
PROXY_EXPORT(GetFileVersionInfoSizeExW)
PROXY_EXPORT(GetFileVersionInfoSizeW)
PROXY_EXPORT(GetFileVersionInfoW)
PROXY_EXPORT(VerFindFileA)
PROXY_EXPORT(VerFindFileW)
PROXY_EXPORT(VerInstallFileA)
PROXY_EXPORT(VerInstallFileW)
PROXY_EXPORT(VerLanguageNameA)
PROXY_EXPORT(VerLanguageNameW)
PROXY_EXPORT(VerQueryValueA)
PROXY_EXPORT(VerQueryValueW)

// Track initialization state
static std::atomic<bool> g_initTriggered{false};
static std::atomic<bool> g_shutdownTriggered{false};

// Log debug messages to file for troubleshooting
static void LogDebug(const char* msg) {
    HANDLE hFile = CreateFileA(
        "C:\\mylogs\\tts_stellaris_debug.log",
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD written;
        WriteFile(hFile, msg, (DWORD)strlen(msg), &written, nullptr);
        CloseHandle(hFile);
    }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    (void)hModule;
    (void)lpReserved;

    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        LogDebug("[DllMain] DLL_PROCESS_ATTACH\r\n");

        // Call SetupHooks DIRECTLY in DllMain (against best practices, but launcher requires it)
        {
            bool expected = false;
            if (g_initTriggered.compare_exchange_strong(expected, true)) {
                LogDebug("[DllMain] Calling SetupHooks directly\r\n");
                __try {
                    SetupHooks();
                    LogDebug("[DllMain] SetupHooks completed successfully\r\n");
                } __except(EXCEPTION_EXECUTE_HANDLER) {
                    LogDebug("[DllMain] EXCEPTION in SetupHooks!\r\n");
                }
            }
        }
        LogDebug("[DllMain] Returning TRUE\r\n");
        break;

    case DLL_PROCESS_DETACH:
        LogDebug("[DllMain] DLL_PROCESS_DETACH\r\n");
        {
            bool expected = false;
            if (g_shutdownTriggered.compare_exchange_strong(expected, true)) {
                MH_Uninitialize();
                ShutdownHooks();
            }
        }
        break;

    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;
    }

    return TRUE;
}
