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

#ifndef TTS_STELLARIS_TTS_FETCHER_H
#define TTS_STELLARIS_TTS_FETCHER_H

#include <vector>
#include <string>
#include <windows.h>
#include <wininet.h>

// RAII wrapper for Windows handles
template<typename T, BOOL(WINAPI* Closer)(T)>
class WinHandle {
    T handle;
public:
    WinHandle(T h = nullptr) : handle(h) {}
    ~WinHandle() { if (handle) Closer(handle); }

    WinHandle(const WinHandle&) = delete;
    WinHandle& operator=(const WinHandle&) = delete;

    WinHandle(WinHandle&& other) noexcept : handle(other.handle) {
        other.handle = nullptr;
    }
    WinHandle& operator=(WinHandle&& other) noexcept {
        if (this != &other) {
            if (handle) Closer(handle);
            handle = other.handle;
            other.handle = nullptr;
        }
        return *this;
    }

    operator T() const { return handle; }
    explicit operator bool() const { return handle != nullptr; }
    T* operator&() { return &handle; }
    const T* operator&() const { return &handle; }

    void reset(T h = nullptr) {
        if (handle) Closer(handle);
        handle = h;
    }
};

using InternetHandle = WinHandle<HINTERNET, InternetCloseHandle>;
using FileHandle = WinHandle<HANDLE, CloseHandle>;

// TTS fetching functions
std::vector<uint8_t> FetchTTSAudioWithRetry(const std::string& text, int maxRetries = 3);
std::vector<uint8_t> FetchTTSAudio(const std::string& text);

#endif // TTS_STELLARIS_TTS_FETCHER_H
