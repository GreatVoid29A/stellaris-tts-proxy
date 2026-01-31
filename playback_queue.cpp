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

#include "playback_queue.h"
#include "logger.h"

// Global playback queue instance
PlaybackQueue g_playbackQueue;

PlaybackQueue::~PlaybackQueue() {
    Shutdown();
}

uint64_t PlaybackQueue::AddRequest(const std::wstring& text) {
    std::lock_guard<std::mutex> lock(queueMutex);

    uint64_t seq = nextSequenceNumber.fetch_add(1);
    AudioItem item;
    item.sequenceNumber = seq;
    item.text = text;
    item.isReady = false;
    item.failed = false;

    pendingItems[seq] = std::move(item);

    LOG_DEBUG(L"Enqueued TTS request #" + std::to_wstring(seq) + L": " + text);
    return seq;
}

void PlaybackQueue::MarkReady(uint64_t seq, const std::vector<uint8_t>& audio, const std::string* cachePath) {
    std::lock_guard<std::mutex> lock(queueMutex);

    auto it = pendingItems.find(seq);
    if (it != pendingItems.end()) {
        it->second.audioData = audio;
        if (cachePath) {
            it->second.cachePath = *cachePath;
        }
        it->second.isReady = true;

        LOG_DEBUG(L"Marked request #" + std::to_wstring(seq) + L" as ready");
        cv.notify_all();
    } else {
        LOG_WARNING(L"Attempted to mark unknown request #" + std::to_wstring(seq) + L" as ready");
    }
}

void PlaybackQueue::MarkFailed(uint64_t seq) {
    std::lock_guard<std::mutex> lock(queueMutex);

    auto it = pendingItems.find(seq);
    if (it != pendingItems.end()) {
        it->second.failed = true;
        it->second.isReady = true;  // Mark as ready so it can be skipped

        LOG_WARNING(L"Marked request #" + std::to_wstring(seq) + L" as failed");
        cv.notify_all();
    }
}

bool PlaybackQueue::WaitForNextReady(AudioItem& outItem) {
    std::unique_lock<std::mutex> lock(queueMutex);

    while (true) {
        // Check for shutdown
        if (shutdownRequested.load()) {
            LOG_INFO(L"PlaybackQueue shutdown requested");
            return false;
        }

        // Look for the next item in sequence
        uint64_t expectedSeq = nextToPlay.load();
        auto it = pendingItems.find(expectedSeq);

        if (it != pendingItems.end()) {
            // Item exists - check if it's ready
            if (it->second.isReady) {
                outItem = std::move(it->second);
                pendingItems.erase(it);
                return true;
            }
            // Item exists but not ready yet - wait
        } else {
            // Item doesn't exist yet - wait for it to be added
        }

        // Wait for notification
        cv.wait(lock);
    }
}

void PlaybackQueue::Remove(uint64_t seq) {
    std::lock_guard<std::mutex> lock(queueMutex);

    // Advance the sequence pointer
    uint64_t currentNext = nextToPlay.load();
    if (seq == currentNext) {
        nextToPlay.store(seq + 1);
        LOG_DEBUG(L"Advanced playback pointer to #" + std::to_wstring(seq + 1));
    }

    // Clean up any old items that might have been missed
    // (this shouldn't happen in normal operation)
    auto it = pendingItems.begin();
    while (it != pendingItems.end()) {
        if (it->first < seq) {
            LOG_WARNING(L"Removing stale item #" + std::to_wstring(it->first));
            it = pendingItems.erase(it);
        } else {
            ++it;
        }
    }
}

void PlaybackQueue::Shutdown() {
    LOG_INFO(L"Shutting down PlaybackQueue");
    shutdownRequested.store(true);
    cv.notify_all();
}

size_t PlaybackQueue::GetSize() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(queueMutex));
    return pendingItems.size();
}
