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

#ifndef TTS_STELLARIS_PLAYBACK_QUEUE_H
#define TTS_STELLARIS_PLAYBACK_QUEUE_H

#include <map>
#include <vector>
#include <string>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <cstdint>

// Audio item in the playback queue
struct AudioItem {
    uint64_t sequenceNumber;
    std::wstring text;
    std::vector<uint8_t> audioData;
    std::string cachePath;
    bool isReady;
    bool failed;

    AudioItem()
        : sequenceNumber(0)
        , isReady(false)
        , failed(false)
    {}
};

// Thread-safe priority queue for ordered playback
// Ensures items are played in sequence even when fetched out of order
class PlaybackQueue {
private:
    std::map<uint64_t, AudioItem> pendingItems;
    std::mutex queueMutex;
    std::condition_variable cv;
    std::atomic<uint64_t> nextSequenceNumber{1};
    std::atomic<uint64_t> nextToPlay{1};
    std::atomic<bool> shutdownRequested{false};

public:
    PlaybackQueue() = default;
    ~PlaybackQueue();

    // Add a new request to the queue, returns the assigned sequence number
    uint64_t AddRequest(const std::wstring& text);

    // Mark an item as ready with audio data
    void MarkReady(uint64_t seq, const std::vector<uint8_t>& audio, const std::string* cachePath);

    // Mark an item as failed (will be skipped during playback)
    void MarkFailed(uint64_t seq);

    // Wait for the next item in sequence to be ready
    // Returns false if shutdown requested, true if item is ready
    bool WaitForNextReady(AudioItem& outItem);

    // Remove an item from the queue after playback
    void Remove(uint64_t seq);

    // Signal shutdown and wake all waiting threads
    void Shutdown();

    // Get current queue size (for debugging)
    size_t GetSize() const;
};

// Global playback queue instance
extern PlaybackQueue g_playbackQueue;

#endif // TTS_STELLARIS_PLAYBACK_QUEUE_H
