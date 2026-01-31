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

#ifndef TTS_STELLARIS_AUDIO_CACHE_H
#define TTS_STELLARIS_AUDIO_CACHE_H

#include <vector>
#include <unordered_map>
#include <mutex>
#include <string>
#include <chrono>
#include <atomic>

// Simple LRU Cache for audio with persistent disk storage
class AudioCache {
private:
    struct CacheEntry {
        std::vector<uint8_t> data;
        std::chrono::steady_clock::time_point timestamp;
    };

    std::unordered_map<std::string, CacheEntry> cache;
    std::mutex cacheMutex;
    size_t maxSize;
    std::string cacheDirectory;
    bool diskCacheEnabled;
    std::atomic<bool> initialized;  // Track if cache has been initialized
    std::string gameDirectory;

    std::string GenerateCacheKey(const std::string& text, const std::string& server, const std::string& voice);
    bool InitializeCacheDirectory();
    bool LoadFromDisk(const std::string& cacheKey, std::vector<uint8_t>& outData);
    bool SaveToDisk(const std::string& cacheKey, const std::vector<uint8_t>& data);
    std::string GetGameDirectory();

public:
    AudioCache(size_t size = 50);

    void Initialize();
    void Clear();
    void ClearDiskCache();
    void SetMaxSize(size_t size);

    bool Get(const std::string& text, const std::string& server, const std::string& voice, std::vector<uint8_t>& outData);
    void Put(const std::string& text, const std::string& server, const std::string& voice, const std::vector<uint8_t>& data);
    std::string GetCachedFilePath(const std::string& text, const std::string& server, const std::string& voice);
};

// Global audio cache instance
extern AudioCache g_audioCache;

#endif // TTS_STELLARIS_AUDIO_CACHE_H
