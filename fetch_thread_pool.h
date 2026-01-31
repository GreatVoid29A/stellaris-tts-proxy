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

#ifndef TTS_STELLARIS_FETCH_THREAD_POOL_H
#define TTS_STELLARIS_FETCH_THREAD_POOL_H

#include <functional>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>
#include <memory>

// Thread pool for parallel TTS fetching
// Uses bounded queue to prevent memory exhaustion
class FetchThreadPool {
private:
    std::vector<std::unique_ptr<std::thread>> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex poolMutex;
    std::condition_variable cv;
    std::atomic<bool> stop{false};
    std::atomic<bool> initialized{false};
    size_t maxThreads;
    size_t maxPendingTasks;

    void WorkerLoop();

public:
    FetchThreadPool(size_t maxThreads = 4, size_t maxPending = 20);
    ~FetchThreadPool();

    // Enqueue a task for parallel execution
    // Returns false if queue is full (task dropped)
    bool Enqueue(std::function<void()> task);

    // Shutdown the thread pool gracefully
    void Shutdown();

    // Get current queue size (for debugging)
    size_t GetQueueSize() const;

    // Check if pool is initialized
    bool IsInitialized() const { return initialized.load(); }
};

// Global fetch thread pool instance
extern FetchThreadPool g_fetchThreadPool;

#endif // TTS_STELLARIS_FETCH_THREAD_POOL_H
