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

#include "logger.h"

#include <iostream>
#include <windows.h>
#include <Shlwapi.h>

#pragma comment(lib, "shlwapi.lib")

Logger g_logger;

const wchar_t* Logger::LevelToString(LogLevel level) {
    switch (level) {
    case LogLevel::Debug:   return L"DEBUG";
    case LogLevel::Info:    return L"INFO ";
    case LogLevel::Warning: return L"WARN ";
    case LogLevel::Error:   return L"ERROR";
    default:                return L"UNK  ";
    }
}

std::wstring Logger::GetTimestamp() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t buf[32];
    swprintf_s(buf, L"%02d:%02d:%02d.%03d",
        st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    return buf;
}

Logger::Logger() {
    // DLL Best Practices: Don't open files in constructor
    // File opening is deferred to first Log() call
    // This prevents file I/O during static initialization
    fileLoggingInitialized = false;
}

Logger::~Logger() {
    std::lock_guard<std::mutex> lock(logMutex);
    if (logFile.is_open()) {
        logFile.close();
    }
}

// Helper to get the game executable directory
static std::wstring GetGameDirectory() {
    wchar_t modulePath[MAX_PATH];
    if (GetModuleFileNameW(NULL, modulePath, MAX_PATH) == 0) {
        return L"";
    }
    PathRemoveFileSpecW(modulePath);
    return std::wstring(modulePath);
}

// Lazy initialization of log file - called on first Log() call
void Logger::InitializeLogFile() {
    if (!fileLoggingInitialized) {
        // Build absolute path to log file in game directory
        std::wstring logPath = GetGameDirectory();
        if (!logPath.empty()) {
            logPath += L"\\tts_proxy.log";
        } else {
            logPath = L"tts_proxy.log";  // Fallback to relative path
        }

        logFile.open(logPath, std::ios::app);
        fileLoggingInitialized = true;
        if (logFile.is_open() && fileLoggingEnabled) {
            // Write session start marker
            auto timestamp = GetTimestamp();
            logFile << L"[" << timestamp << L"] [SESSION] Logging started" << std::endl;
            logFile.flush();
        }
    }
}

void Logger::SetFileLoggingEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(logMutex);
    fileLoggingEnabled = enabled;
}

void Logger::SetLogLevel(const std::wstring& level) {
    std::wstring lowerLevel = level;
    for (auto& c : lowerLevel) {
        c = towlower(c);
    }

    if (lowerLevel == L"debug") minLogLevel = LogLevel::Debug;
    else if (lowerLevel == L"info") minLogLevel = LogLevel::Info;
    else if (lowerLevel == L"warning" || lowerLevel == L"warn") minLogLevel = LogLevel::Warning;
    else if (lowerLevel == L"error") minLogLevel = LogLevel::Error;
    else minLogLevel = LogLevel::Info;  // default
}

void Logger::Log(LogLevel level, const std::wstring& message) {
    if (level < minLogLevel) {
        return;  // Skip logging if below minimum level
    }

    std::lock_guard<std::mutex> lock(logMutex);

    // Lazy initialization - only open file on first log call
    if (!fileLoggingInitialized) {
        InitializeLogFile();
    }

    auto timestamp = GetTimestamp();
    auto levelStr = LevelToString(level);
    auto logLine = L"[" + timestamp + L"] [" + levelStr + L"] " + message;

    std::wcout << logLine << std::endl;

    if (fileLoggingEnabled && logFile.is_open()) {
        logFile << logLine << std::endl;
        logFile.flush();
    }
}

void Logger::Debug(const std::wstring& msg) { Log(LogLevel::Debug, msg); }
void Logger::Info(const std::wstring& msg) { Log(LogLevel::Info, msg); }
void Logger::Warning(const std::wstring& msg) { Log(LogLevel::Warning, msg); }
void Logger::Error(const std::wstring& msg) { Log(LogLevel::Error, msg); }
