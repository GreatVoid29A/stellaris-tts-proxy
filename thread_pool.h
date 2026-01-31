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

#ifndef TTS_STELLARIS_THREAD_POOL_H
#define TTS_STELLARIS_THREAD_POOL_H

#include <functional>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <thread>
#include <memory>

// Thread-safe queue for TTS processing
// DLL Best Practices: Uses lazy initialization to avoid thread creation during static init
class TTSThreadPool {
private:
    std::queue<std::function<void()>> tasks;
    mutable std::mutex queueMutex;  // mutable for const GetQueueSize()
    std::condition_variable cv;
    std::atomic<bool> stop{ false };
    std::atomic<bool> taskRunning{ false };
    std::unique_ptr<std::thread> worker;  // Use unique_ptr for lazy initialization
    std::atomic<bool> initialized{ false };

    void WorkerLoop();
    void Initialize();  // Lazy initialization

public:
    TTSThreadPool() = default;  // Default constructor - no thread created
    ~TTSThreadPool();

    // Enqueue a task - initializes thread pool on first call if needed
    void Enqueue(std::function<void()> task);
    size_t GetQueueSize() const;

    // Shutdown the thread pool gracefully (waits for tasks)
    void Shutdown();

    // Fast shutdown - signals but doesn't wait (for DLL unload)
    void ShutdownFast();
};

// Global TTS thread pool instance - constructed but not initialized
extern TTSThreadPool g_ttsThreadPool;

#endif // TTS_STELLARIS_THREAD_POOL_H
