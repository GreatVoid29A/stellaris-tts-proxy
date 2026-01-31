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

#include "thread_pool.h"
#include "logger.h"

#include <chrono>
#include <thread>
#include <windows.h>

// Lock Hierarchy (to prevent deadlocks):
// 1. Loader lock (highest priority - held by OS during DllMain)
// 2. queueMutex (thread pool queue)
// 3. cacheMutex (audio cache)
// 4. g_audioMutex (audio processing)
// 5. logMutex (logger)
//
// Important: Never acquire locks in reverse order!
// Never call any function that might acquire loader lock while holding any other lock.

TTSThreadPool g_ttsThreadPool;

void TTSThreadPool::WorkerLoop() {
    // Initialize COM for this thread (required for Windows audio/text-to-speech APIs)
    // Use STA model for MCI MP3 playback compatibility
    // Note: This is safe here because WorkerLoop is called after lazy initialization,
    // which happens after DllMain has returned and loader lock is released.
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    while (true) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            cv.wait(lock, [this] {
                return stop.load() || !tasks.empty();
            });

            if (stop.load() && tasks.empty()) {
                break;
            }

            if (!tasks.empty()) {
                task = std::move(tasks.front());
                tasks.pop();
                taskRunning = true;  // Set under lock to prevent race condition
            }
        }

        if (task) {
            try {
                task();
            }
            catch (...) {
                LOG_ERROR(L"Exception in TTS task");
            }
            taskRunning = false;
        }
    }

    CoUninitialize();
}

// Lazy initialization - called on first Enqueue
// DLL Best Practices: Thread creation deferred until after DLL is fully loaded
void TTSThreadPool::Initialize() {
    bool expected = false;
    if (initialized.compare_exchange_strong(expected, true)) {
        // Create the worker thread
        worker = std::make_unique<std::thread>(&TTSThreadPool::WorkerLoop, this);
    }
}

TTSThreadPool::~TTSThreadPool() {
    Shutdown();
}

// Graceful shutdown - must be called before DLL unload
void TTSThreadPool::Shutdown() {
    // Only shutdown if we were initialized
    if (!initialized.load()) {
        return;
    }

    // Signal shutdown
    stop.store(true);
    cv.notify_one();

    // Wait for current task to complete (with timeout)
    auto startTime = std::chrono::steady_clock::now();
    while (taskRunning.load()) {
        auto elapsed = std::chrono::steady_clock::now() - startTime;
        if (elapsed > std::chrono::seconds(5)) {
            LOG_WARNING(L"Task still running after 5s during shutdown");
            // Don't detach - just break to avoid deadlock
            // The thread will be cleaned up by process termination
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Join the worker thread if it exists and is joinable
    if (worker && worker->joinable()) {
        worker->join();
    }

    // Clear remaining tasks
    std::lock_guard<std::mutex> lock(queueMutex);
    while (!tasks.empty()) {
        tasks.pop();
    }
}

// Fast shutdown - signals but doesn't wait (for DLL unload)
// The OS will clean up threads when the process terminates
void TTSThreadPool::ShutdownFast() {
    // Only shutdown if we were initialized
    if (!initialized.load()) {
        return;
    }

    // Just signal shutdown and notify
    stop.store(true);
    cv.notify_one();

    // Detach the thread so it can clean itself up
    // Don't join - the OS will handle thread cleanup on process termination
    if (worker && worker->joinable()) {
        worker->detach();
    }
}

void TTSThreadPool::Enqueue(std::function<void()> task) {
    // Lazy initialization - create thread on first use
    if (!initialized.load()) {
        Initialize();
    }

    {
        std::lock_guard<std::mutex> lock(queueMutex);
        if (tasks.size() < 100) {
            tasks.push(std::move(task));
        }
        else {
            LOG_WARNING(L"TTS queue full, dropping request");
            return;
        }
    }
    cv.notify_one();
}

size_t TTSThreadPool::GetQueueSize() const {
    std::lock_guard<std::mutex> lock(queueMutex);
    return tasks.size();
}
