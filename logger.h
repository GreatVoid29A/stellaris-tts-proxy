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

#ifndef TTS_STELLARIS_LOGGER_H
#define TTS_STELLARIS_LOGGER_H

#include <mutex>
#include <iostream>
#include <fstream>
#include <string>

enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error
};

class Logger {
private:
    std::mutex logMutex;
    std::wofstream logFile;
    bool fileLoggingEnabled = false;
    bool fileLoggingInitialized = false;  // Track if log file has been opened
    LogLevel minLogLevel = LogLevel::Info;

    static const wchar_t* LevelToString(LogLevel level);
    std::wstring GetTimestamp();
    void InitializeLogFile();  // Lazy initialization of log file

public:
    Logger();
    ~Logger();

    void SetLogLevel(const std::wstring& level);
    void SetFileLoggingEnabled(bool enabled);
    void Log(LogLevel level, const std::wstring& message);
    void Debug(const std::wstring& msg);
    void Info(const std::wstring& msg);
    void Warning(const std::wstring& msg);
    void Error(const std::wstring& msg);
};

// Global logger instance
extern Logger g_logger;

// Logging macros
#define LOG_DEBUG(msg) g_logger.Debug(msg)
#define LOG_INFO(msg) g_logger.Info(msg)
#define LOG_WARNING(msg) g_logger.Warning(msg)
#define LOG_ERROR(msg) g_logger.Error(msg)

#endif // TTS_STELLARIS_LOGGER_H
