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

#include "fetch_thread_pool.h"
#include "logger.h"
#include <windows.h>
#include <objbase.h>

// Global fetch thread pool instance
FetchThreadPool g_fetchThreadPool;

FetchThreadPool::FetchThreadPool(size_t threads, size_t maxPending)
    : maxThreads(threads)
    , maxPendingTasks(maxPending)
{
    // Lazy initialization - don't create threads yet
    LOG_DEBUG(L"FetchThreadPool created (lazy initialization)");
}

FetchThreadPool::~FetchThreadPool() {
    Shutdown();
}

void FetchThreadPool::WorkerLoop() {
    // Initialize COM for this thread (required for WinINet)
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        LOG_ERROR(L"Failed to initialize COM in fetch worker thread");
        return;
    }

    LOG_DEBUG(L"Fetch worker thread started");

    while (true) {
        std::function<void()> task;

        {
            std::unique_lock<std::mutex> lock(poolMutex);

            // Wait for task or stop signal
            cv.wait(lock, [this] {
                return stop.load() || !tasks.empty();
            });

            // Check for shutdown
            if (stop.load() && tasks.empty()) {
                break;
            }

            // Get next task
            if (!tasks.empty()) {
                task = std::move(tasks.front());
                tasks.pop();
            }
        }

        // Execute task (without holding lock)
        if (task) {
            try {
                task();
            } catch (const std::exception& e) {
                LOG_ERROR(L"Exception in fetch worker: " + std::wstring(e.what(), e.what() + strlen(e.what())));
            } catch (...) {
                LOG_ERROR(L"Unknown exception in fetch worker");
            }
        }
    }

    CoUninitialize();
    LOG_DEBUG(L"Fetch worker thread stopped");
}

bool FetchThreadPool::Enqueue(std::function<void()> task) {
    // Initialize on first use (lazy initialization)
    if (!initialized.load()) {
        std::lock_guard<std::mutex> lock(poolMutex);

        // Double-check after acquiring lock
        if (!initialized.load()) {
            LOG_INFO(L"Initializing FetchThreadPool with " + std::to_wstring(maxThreads) + L" threads");

            // Create worker threads
            for (size_t i = 0; i < maxThreads; ++i) {
                workers.emplace_back(std::make_unique<std::thread>(&FetchThreadPool::WorkerLoop, this));
            }

            initialized.store(true);
            LOG_INFO(L"FetchThreadPool initialized successfully");
        }
    }

    std::lock_guard<std::mutex> lock(poolMutex);

    // Check queue size
    if (tasks.size() >= maxPendingTasks) {
        LOG_WARNING(L"FetchThreadPool queue full (" + std::to_wstring(tasks.size()) +
                   L" >= " + std::to_wstring(maxPendingTasks) + L"), dropping task");
        return false;
    }

    tasks.push(std::move(task));
    cv.notify_one();

    return true;
}

void FetchThreadPool::Shutdown() {
    if (!initialized.load()) {
        return;  // Never initialized, nothing to shut down
    }

    LOG_INFO(L"Shutting down FetchThreadPool");

    {
        std::lock_guard<std::mutex> lock(poolMutex);
        stop.store(true);
    }

    cv.notify_all();

    // Wait for all workers to finish
    for (auto& worker : workers) {
        if (worker && worker->joinable()) {
            worker->join();
        }
    }

    workers.clear();
    LOG_INFO(L"FetchThreadPool shutdown complete");
}

size_t FetchThreadPool::GetQueueSize() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(poolMutex));
    return tasks.size();
}
