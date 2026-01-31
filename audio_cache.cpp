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

#include "audio_cache.h"
#include "logger.h"
#include "config.h"
#include <Windows.h>
#include <Shlwapi.h>
#include <bcrypt.h>
#include <sstream>
#include <iomanip>
#include <fstream>

#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "shlwapi.lib")

// Global instance - constructor now does nothing (lazy initialization)
AudioCache g_audioCache;

AudioCache::AudioCache(size_t size) : maxSize(size), diskCacheEnabled(false), initialized(false) {}

// Initialize the cache - called on first use
// DLL Best Practices: Deferred initialization to avoid file I/O during static init
void AudioCache::Initialize() {
    bool expected = false;
    if (!initialized.compare_exchange_strong(expected, true)) {
        return;  // Already initialized
    }

    gameDirectory = GetGameDirectory();
    if (gameDirectory.empty()) {
        LOG_ERROR(L"Failed to get game directory");
        return;
    }

    cacheDirectory = gameDirectory + "\\tts_audio_cache";
    diskCacheEnabled = InitializeCacheDirectory();

    if (diskCacheEnabled) {
        LOG_INFO(L"Disk cache initialized at: " + std::wstring(cacheDirectory.begin(), cacheDirectory.end()));
    }
}

std::string AudioCache::GetGameDirectory() {
    wchar_t modulePath[MAX_PATH];
    if (GetModuleFileNameW(NULL, modulePath, MAX_PATH) == 0) {
        LOG_ERROR(L"GetModuleFileNameW failed");
        return "";
    }

    PathRemoveFileSpecW(modulePath);

    // Properly convert wchar_t to UTF-8
    int size = WideCharToMultiByte(CP_UTF8, 0, modulePath, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return "";
    }
    std::string result(size - 1, 0);  // size includes null terminator
    WideCharToMultiByte(CP_UTF8, 0, modulePath, -1, &result[0], size, nullptr, nullptr);
    return result;
}

bool AudioCache::InitializeCacheDirectory() {
    if (CreateDirectoryA(cacheDirectory.c_str(), NULL) || GetLastError() == ERROR_ALREADY_EXISTS) {
        return true;
    }

    LOG_ERROR(L"Failed to create cache directory: " + std::wstring(cacheDirectory.begin(), cacheDirectory.end()));
    return false;
}

std::string AudioCache::GenerateCacheKey(const std::string& text, const std::string& server, const std::string& voice) {
    // Combine: text + "|" + server + "|" + voice
    std::string combined = text + "|" + server + "|" + voice;

    // Hash using BCrypt (SHA-256)
    BCRYPT_ALG_HANDLE hAlg = NULL;
    BCRYPT_HASH_HANDLE hHash = NULL;
    DWORD hashLength = 0;
    DWORD hashObjectSize = 0;
    DWORD cbData = 0;
    PBYTE hashObject = NULL;
    PBYTE hash = NULL;
    std::string result;

    // Declare stringstream at the beginning to avoid jumping over initialization
    std::stringstream ss;

    NTSTATUS status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, NULL, 0);
    if (FAILED(status)) {
        LOG_ERROR(L"BCryptOpenAlgorithmProvider failed: " + std::to_wstring(status));
        goto cleanup;
    }

    // Get hash length
    status = BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (PBYTE)&hashLength, sizeof(hashLength), &cbData, 0);
    if (FAILED(status)) {
        LOG_ERROR(L"BCryptGetProperty for hash length failed: " + std::to_wstring(status));
        goto cleanup;
    }

    // Get hash object size
    status = BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PBYTE)&hashObjectSize, sizeof(hashObjectSize), &cbData, 0);
    if (FAILED(status)) {
        LOG_ERROR(L"BCryptGetProperty for object length failed: " + std::to_wstring(status));
        goto cleanup;
    }

    hashObject = (PBYTE)HeapAlloc(GetProcessHeap(), 0, hashObjectSize);
    hash = (PBYTE)HeapAlloc(GetProcessHeap(), 0, hashLength);
    if (!hashObject || !hash) {
        LOG_ERROR(L"HeapAlloc failed");
        goto cleanup;
    }

    status = BCryptCreateHash(hAlg, &hHash, hashObject, hashObjectSize, NULL, 0, 0);
    if (FAILED(status)) {
        LOG_ERROR(L"BCryptCreateHash failed: " + std::to_wstring(status));
        goto cleanup;
    }

    status = BCryptHashData(hHash, (PBYTE)combined.data(), (ULONG)combined.size(), 0);
    if (FAILED(status)) {
        LOG_ERROR(L"BCryptHashData failed: " + std::to_wstring(status));
        goto cleanup;
    }

    status = BCryptFinishHash(hHash, hash, hashLength, 0);
    if (FAILED(status)) {
        LOG_ERROR(L"BCryptFinishHash failed: " + std::to_wstring(status));
        goto cleanup;
    }

    // Convert hash to hex string
    ss << std::hex << std::setfill('0');
    for (DWORD i = 0; i < hashLength; i++) {
        ss << std::setw(2) << static_cast<int>(hash[i]);
    }
    result = ss.str();

cleanup:
    if (hHash) BCryptDestroyHash(hHash);
    if (hAlg) BCryptCloseAlgorithmProvider(hAlg, 0);
    if (hashObject) HeapFree(GetProcessHeap(), 0, hashObject);
    if (hash) HeapFree(GetProcessHeap(), 0, hash);

    return result;
}

bool AudioCache::LoadFromDisk(const std::string& cacheKey, std::vector<uint8_t>& outData) {
    if (!diskCacheEnabled) {
        return false;
    }

    std::string filePath = cacheDirectory + "\\" + cacheKey + "." + std::string(g_config.format);

    HANDLE hFile = CreateFileA(
        filePath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }

    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile, &fileSize)) {
        CloseHandle(hFile);
        return false;
    }

    outData.resize(fileSize.QuadPart);
    DWORD bytesRead;
    BOOL success = ReadFile(hFile, outData.data(), static_cast<DWORD>(fileSize.QuadPart), &bytesRead, NULL);
    CloseHandle(hFile);

    if (success && bytesRead == fileSize.QuadPart) {
        LOG_DEBUG(L"Loaded from disk cache: " + std::wstring(filePath.begin(), filePath.end()));
        return true;
    }

    return false;
}

bool AudioCache::SaveToDisk(const std::string& cacheKey, const std::vector<uint8_t>& data) {
    if (!diskCacheEnabled) {
        return false;
    }

    std::string filePath = cacheDirectory + "\\" + cacheKey + "." + std::string(g_config.format);

    HANDLE hFile = CreateFileA(
        filePath.c_str(),
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        LOG_ERROR(L"Failed to create cache file: " + std::wstring(filePath.begin(), filePath.end()));
        return false;
    }

    DWORD bytesWritten;
    BOOL success = WriteFile(hFile, data.data(), static_cast<DWORD>(data.size()), &bytesWritten, NULL);
    CloseHandle(hFile);

    if (success && bytesWritten == data.size()) {
        LOG_DEBUG(L"Saved to disk cache: " + std::wstring(filePath.begin(), filePath.end()));
        return true;
    }

    LOG_ERROR(L"Failed to write cache file: " + std::wstring(filePath.begin(), filePath.end()));
    return false;
}

bool AudioCache::Get(const std::string& text, const std::string& server, const std::string& voice, std::vector<uint8_t>& outData) {
    std::string cacheKey = GenerateCacheKey(text, server, voice);

    // Check in-memory cache first
    {
        std::lock_guard<std::mutex> lock(cacheMutex);
        auto it = cache.find(cacheKey);
        if (it != cache.end()) {
            outData = it->second.data;
            it->second.timestamp = std::chrono::steady_clock::now();
            LOG_DEBUG(L"Cache hit (memory) for key: " + std::wstring(cacheKey.begin(), cacheKey.begin() + 16) + L"...");
            return true;
        }
    }

    // Check disk cache
    if (LoadFromDisk(cacheKey, outData)) {
        // Load into memory for faster access next time
        std::lock_guard<std::mutex> lock(cacheMutex);

        // Evict oldest if cache is full
        if (cache.size() >= maxSize) {
            auto oldest = cache.begin();
            for (auto it = cache.begin(); it != cache.end(); ++it) {
                if (it->second.timestamp < oldest->second.timestamp) {
                    oldest = it;
                }
            }
            cache.erase(oldest);
        }

        CacheEntry entry;
        entry.data = outData;
        entry.timestamp = std::chrono::steady_clock::now();
        cache[cacheKey] = std::move(entry);

        LOG_DEBUG(L"Cache hit (disk) for key: " + std::wstring(cacheKey.begin(), cacheKey.begin() + 16) + L"...");
        return true;
    }

    LOG_DEBUG(L"Cache miss for key: " + std::wstring(cacheKey.begin(), cacheKey.begin() + 16) + L"...");
    return false;
}

void AudioCache::Put(const std::string& text, const std::string& server, const std::string& voice, const std::vector<uint8_t>& data) {
    std::string cacheKey = GenerateCacheKey(text, server, voice);

    std::lock_guard<std::mutex> lock(cacheMutex);

    // Evict oldest if cache is full
    if (cache.size() >= maxSize) {
        auto oldest = cache.begin();
        for (auto it = cache.begin(); it != cache.end(); ++it) {
            if (it->second.timestamp < oldest->second.timestamp) {
                oldest = it;
            }
        }
        cache.erase(oldest);
    }

    CacheEntry entry;
    entry.data = data;
    entry.timestamp = std::chrono::steady_clock::now();
    cache[cacheKey] = std::move(entry);

    // Also save to disk (async - don't block if it fails)
    SaveToDisk(cacheKey, data);
}

void AudioCache::Clear() {
    std::lock_guard<std::mutex> lock(cacheMutex);
    cache.clear();
}

void AudioCache::ClearDiskCache() {
    if (!diskCacheEnabled) {
        return;
    }

    std::string searchPath = cacheDirectory + "\\*." + g_config.format;
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);

    if (hFind == INVALID_HANDLE_VALUE) {
        LOG_INFO(L"No cache files to clear");
        return;
    }

    int deletedCount = 0;
    do {
        std::string filePath = cacheDirectory + "\\" + findData.cFileName;
        if (DeleteFileA(filePath.c_str())) {
            deletedCount++;
        }
    } while (FindNextFileA(hFind, &findData));

    FindClose(hFind);
    LOG_INFO(L"Cleared " + std::to_wstring(deletedCount) + L" files from disk cache");
}

void AudioCache::SetMaxSize(size_t size) {
    std::lock_guard<std::mutex> lock(cacheMutex);
    maxSize = size;
}

std::string AudioCache::GetCachedFilePath(const std::string& text, const std::string& server, const std::string& voice) {
    if (!diskCacheEnabled) return "";
    std::string cacheKey = GenerateCacheKey(text, server, voice);
    return cacheDirectory + "\\" + cacheKey + "." + g_config.format;
}
