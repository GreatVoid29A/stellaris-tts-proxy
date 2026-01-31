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

#include "hotkey.h"
#include "config.h"
#include "audio_player.h"
#include "logger.h"

#include <cctype>
#include <windows.h>

// Hidden window for hotkey messages
static HWND g_hwndHotkey = nullptr;
static int g_registeredHotkeyId = 0;
static const int HOTKEY_ID = 1;

int GetVirtualKeyCode(const std::string& keyName) {
    if (keyName == "F1") return VK_F1;
    if (keyName == "F2") return VK_F2;
    if (keyName == "F3") return VK_F3;
    if (keyName == "F4") return VK_F4;
    if (keyName == "F5") return VK_F5;
    if (keyName == "F6") return VK_F6;
    if (keyName == "F7") return VK_F7;
    if (keyName == "F8") return VK_F8;
    if (keyName == "F9") return VK_F9;
    if (keyName == "F10") return VK_F10;
    if (keyName == "F11") return VK_F11;
    if (keyName == "F12") return VK_F12;

    if (keyName == "ESC" || keyName == "ESCAPE") return VK_ESCAPE;
    if (keyName == "SPACE") return VK_SPACE;
    if (keyName == "ENTER") return VK_RETURN;
    if (keyName == "TAB") return VK_TAB;
    if (keyName == "BACKSPACE") return VK_BACK;

    if (keyName.length() == 1) {
        char c = toupper(keyName[0]);
        if (c >= 'A' && c <= 'Z') return c;
        if (c >= '0' && c <= '9') return c;
    }

    return 0;
}

// Window procedure for hotkey messages
static LRESULT CALLBACK HotkeyWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    (void)hwnd;
    (void)lParam;

    if (msg == WM_HOTKEY && wParam == HOTKEY_ID) {
        if (g_isPlaying) {
            LOG_INFO(L"Cancel key pressed (via RegisterHotKey)!");
            g_shouldCancel = true;
        }
        return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

DWORD WINAPI HotkeyMonitorThread(LPVOID lpParam) {
    (void)lpParam;
    int cancelKey = GetVirtualKeyCode(g_config.cancel_key);

    if (cancelKey == 0) {
        std::wstring cancelKeyWide(g_config.cancel_key, g_config.cancel_key + strlen(g_config.cancel_key));
        LOG_WARNING(L"Invalid cancel key configured: " + cancelKeyWide);
        LOG_WARNING(L"Hotkey monitoring disabled");
        return 0;
    }

    std::wstring cancelKeyWide(g_config.cancel_key, g_config.cancel_key + strlen(g_config.cancel_key));
    LOG_INFO(L"Registering hotkey: " + cancelKeyWide + L" (using RegisterHotKey API)");

    // Create window class for hotkey handling
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = HotkeyWindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"TTSStellarisHotkeyWindow";

    RegisterClassW(&wc);

    // Create message-only window
    g_hwndHotkey = CreateWindowW(
        L"TTSStellarisHotkeyWindow",
        L"TTS Hotkey",
        0,
        0, 0, 0, 0,
        HWND_MESSAGE,
        NULL,
        GetModuleHandle(NULL),
        NULL
    );

    if (!g_hwndHotkey) {
        LOG_ERROR(L"Failed to create hotkey window");
        return 0;
    }

    // Register the hotkey
    if (!RegisterHotKey(g_hwndHotkey, HOTKEY_ID, 0, cancelKey)) {
        LOG_ERROR(L"Failed to register hotkey");
        DestroyWindow(g_hwndHotkey);
        g_hwndHotkey = nullptr;
        return 0;
    }

    LOG_INFO(L"Hotkey registered successfully - Press " + cancelKeyWide + L" to cancel audio");

    // Message loop - waits for hotkey messages
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0) > 0) {
        if (msg.message == WM_HOTKEY) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    // Cleanup
    if (g_hwndHotkey) {
        UnregisterHotKey(g_hwndHotkey, HOTKEY_ID);
        DestroyWindow(g_hwndHotkey);
        g_hwndHotkey = nullptr;
    }

    LOG_INFO(L"Hotkey monitor shutting down");
    return 0;
}

// Signal the hotkey thread to exit gracefully
void SignalHotkeyThreadShutdown() {
    if (g_hwndHotkey) {
        // Post quit message to break the message loop
        PostMessage(g_hwndHotkey, WM_QUIT, 0, 0);
    }
}
