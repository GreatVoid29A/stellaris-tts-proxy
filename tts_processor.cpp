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

#include "tts_processor.h"
#include "config.h"
#include "utils.h"
#include "logger.h"
#include "audio_cache.h"
#include "tts_fetcher.h"
#include "audio_player.h"
#include "playback_queue.h"
#include "fetch_thread_pool.h"
#include <thread>
#include <atomic>
#include <memory>
#include <mutex>

std::mutex g_audioMutex;

// Global instances for parallel processing
// Use unique_ptr for lazy initialization (DLL safety)
std::unique_ptr<std::thread> g_playbackCoordinatorThread;
std::atomic<bool> g_playbackCoordinatorRunning{false};
std::atomic<bool> g_playbackCoordinatorInitialized{false};
std::mutex g_playbackCoordinatorMutex;

// ============================================================
// PARALLEL TTS PROCESSING FUNCTIONS
// ============================================================

// Entry point for parallel TTS processing - non-blocking
void ProcessTTSRequest(const std::wstring& text) {
    // Lazy initialization: start playback coordinator on first use
    if (!g_playbackCoordinatorInitialized.load()) {
        std::lock_guard<std::mutex> lock(g_playbackCoordinatorMutex);

        // Double-check after acquiring lock
        if (!g_playbackCoordinatorInitialized.load()) {
            LOG_INFO(L"Lazy initializing PlaybackCoordinator thread");
            g_playbackCoordinatorRunning.store(true);
            g_playbackCoordinatorThread = std::make_unique<std::thread>(PlaybackCoordinator);
            g_playbackCoordinatorInitialized.store(true);
            LOG_INFO(L"PlaybackCoordinator thread started");
        }
    }

    uint64_t seq = g_playbackQueue.AddRequest(text);
    std::string utf8Text = WideToUTF8(text);

    if (!g_fetchThreadPool.Enqueue([utf8Text, seq]() {
        FetchAndEnqueueForPlayback(utf8Text, seq);
    })) {
        LOG_WARNING(L"Failed to enqueue fetch task for: " + text);
        g_playbackQueue.MarkFailed(seq);
    }
}

// Fetch worker - runs in parallel thread
void FetchAndEnqueueForPlayback(const std::string& text, uint64_t sequenceNumber) {
    LOG_DEBUG(L"Fetching audio for request #" + std::to_wstring(sequenceNumber));

    std::vector<uint8_t> audioData;
    std::string cachePath;

    // Check cache first
    if (g_audioCache.Get(text, g_config.server, g_config.voice, audioData)) {
        LOG_DEBUG(L"Cache hit for request #" + std::to_wstring(sequenceNumber));
        cachePath = g_audioCache.GetCachedFilePath(text, g_config.server, g_config.voice);
        g_playbackQueue.MarkReady(sequenceNumber, audioData, &cachePath);
        return;
    }

    // Sanitize text before fetching
    std::string sanitizedText = text;
    if (!SanitizeText(sanitizedText)) {
        LOG_WARNING(L"Text sanitization failed for request #" + std::to_wstring(sequenceNumber));
        g_playbackQueue.MarkFailed(sequenceNumber);
        return;
    }

    // Fetch from server (parallel!)
    LOG_DEBUG(L"Fetching from server for request #" + std::to_wstring(sequenceNumber));
    audioData = FetchTTSAudio(sanitizedText);

    if (!audioData.empty()) {
        // Cache the result
        g_audioCache.Put(text, g_config.server, g_config.voice, audioData);
        cachePath = g_audioCache.GetCachedFilePath(text, g_config.server, g_config.voice);

        LOG_DEBUG(L"Fetch complete for request #" + std::to_wstring(sequenceNumber));
        g_playbackQueue.MarkReady(sequenceNumber, audioData, &cachePath);
    } else {
        LOG_ERROR(L"Fetch failed for request #" + std::to_wstring(sequenceNumber));
        g_playbackQueue.MarkFailed(sequenceNumber);
    }
}

// Playback coordinator - runs in dedicated thread, ensures sequential playback
void PlaybackCoordinator() {
    LOG_INFO(L"PlaybackCoordinator thread started");

    // Initialize COM for this thread
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        LOG_ERROR(L"Failed to initialize COM in PlaybackCoordinator");
        return;
    }

    while (g_playbackCoordinatorRunning.load()) {
        AudioItem item;

        // Wait for next item in sequence (blocking)
        if (!g_playbackQueue.WaitForNextReady(item)) {
            break;  // Shutdown requested
        }

        // Skip failed items
        if (item.failed) {
            LOG_WARNING(L"Skipping failed item #" + std::to_wstring(item.sequenceNumber));
            g_playbackQueue.Remove(item.sequenceNumber);
            continue;
        }

        // Play audio (only holds g_audioMutex during playback)
        LOG_INFO(L"Playing item #" + std::to_wstring(item.sequenceNumber) + L": " + item.text);

        {
            std::lock_guard<std::mutex> lock(g_audioMutex);
            PlayAudioFromMemory(item.audioData,
                item.cachePath.empty() ? nullptr : &item.cachePath);
        }

        LOG_DEBUG(L"Finished playing item #" + std::to_wstring(item.sequenceNumber));
        g_playbackQueue.Remove(item.sequenceNumber);
    }

    CoUninitialize();
    LOG_INFO(L"PlaybackCoordinator thread stopped");
}

// Initialize the parallel TTS system (no thread creation - lazy init)
void InitializeParallelSystem() {
    LOG_INFO(L"Parallel TTS system ready (lazy initialization on first use)");
}

// Shutdown the parallel TTS system
void ShutdownParallelSystem() {
    // Only shutdown if initialized
    if (!g_playbackCoordinatorInitialized.load()) {
        LOG_INFO(L"Parallel system not initialized, skipping shutdown");
        return;
    }

    LOG_INFO(L"Shutting down parallel TTS system");

    // Signal shutdown
    g_playbackCoordinatorRunning.store(false);
    g_playbackQueue.Shutdown();
    g_fetchThreadPool.Shutdown();

    // Detach the thread (fast shutdown for DLL unload)
    if (g_playbackCoordinatorThread && g_playbackCoordinatorThread->joinable()) {
        g_playbackCoordinatorThread->detach();
    }

    LOG_INFO(L"Parallel TTS system shutdown complete");
}
