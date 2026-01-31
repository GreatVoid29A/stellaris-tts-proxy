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

#ifndef TTS_STELLARIS_CONFIG_H
#define TTS_STELLARIS_CONFIG_H

#include <string>
#include <cstring>
#include <string.h>
#include "logger.h"

// Maximum string sizes for config values
constexpr size_t MAX_CONFIG_STRING_SIZE = 256;

// DLL Best Practices: Use const char* and fixed buffers instead of std::string
// in global struct to avoid complex static initialization issues
struct TTSConfig {
    // Public string pointers (point to internal storage)
    const char* server;
    const char* model;
    const char* voice;
    const char* api_key;
    const char* format;
    const char* cancel_key;
    const char* log_level;

    // Non-string members
    int volume;
    bool mute_original;
    int max_cache_size;
    bool enable_disk_cache;
    int max_disk_cache_mb;
    bool show_console;
    bool log_to_file;
    int max_fetch_threads;
    int max_pending_fetches;

    // Internal storage buffers (private - use setters to modify)
    char server_buf[MAX_CONFIG_STRING_SIZE];
    char model_buf[MAX_CONFIG_STRING_SIZE];
    char voice_buf[MAX_CONFIG_STRING_SIZE];
    char api_key_buf[MAX_CONFIG_STRING_SIZE];
    char format_buf[MAX_CONFIG_STRING_SIZE];
    char cancel_key_buf[MAX_CONFIG_STRING_SIZE];
    char log_level_buf[MAX_CONFIG_STRING_SIZE];

    // Set string value (copies to internal buffer and updates pointer)
    void SetServer(const char* value);
    void SetModel(const char* value);
    void SetVoice(const char* value);
    void SetApiKey(const char* value);
    void SetFormat(const char* value);
    void SetCancelKey(const char* value);
    void SetLogLevel(const char* value);

    // Initialize with default values
    void SetDefaults();

    // Helper comparison methods (for convenience with const char*)
    bool ServerEquals(const char* value) const { return strcmp(server, value) == 0; }
    bool FormatEquals(const char* value) const { return strcmp(format, value) == 0; }
    bool ApiKeyEmpty() const { return api_key == nullptr || api_key[0] == '\0'; }

private:
    // Helper to copy string to buffer and update pointer
    void SetString(const char* value, char* buffer, const char*& ptr);
};

// Global configuration instance (safe - only has const char* and POD types)
extern TTSConfig g_config;

// Configuration functions
bool ValidateConfig();
bool LoadConfig(const std::string& filename);

#endif // TTS_STELLARIS_CONFIG_H
