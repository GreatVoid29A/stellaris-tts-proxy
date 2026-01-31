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

#include "config.h"
#include "logger.h"
#include "utils.h"

#include <fstream>
#include <sstream>
#include <cstring>

// DLL Best Practices: Global config now only contains const char* and POD types
// No std::string members means no complex static initialization
TTSConfig g_config;

// Helper to copy string to buffer and update pointer
void TTSConfig::SetString(const char* value, char* buffer, const char*& ptr) {
    if (value && value[0] != '\0') {
        strncpy_s(buffer, MAX_CONFIG_STRING_SIZE, value, _TRUNCATE);
        buffer[MAX_CONFIG_STRING_SIZE - 1] = '\0';  // Ensure null termination
    } else {
        buffer[0] = '\0';
    }
    ptr = buffer;
}

void TTSConfig::SetServer(const char* value) {
    SetString(value, server_buf, server);
}

void TTSConfig::SetModel(const char* value) {
    SetString(value, model_buf, model);
}

void TTSConfig::SetVoice(const char* value) {
    SetString(value, voice_buf, voice);
}

void TTSConfig::SetApiKey(const char* value) {
    SetString(value, api_key_buf, api_key);
}

void TTSConfig::SetFormat(const char* value) {
    SetString(value, format_buf, format);
}

void TTSConfig::SetCancelKey(const char* value) {
    SetString(value, cancel_key_buf, cancel_key);
}

void TTSConfig::SetLogLevel(const char* value) {
    SetString(value, log_level_buf, log_level);
}

void TTSConfig::SetDefaults() {
    SetServer("http://localhost:5050/v1");
    SetModel("tts-1");
    SetVoice("onyx");
    SetApiKey("");
    SetFormat("wav");
    SetCancelKey("F9");
    SetLogLevel("info");

    volume = 90;
    mute_original = true;
    max_cache_size = 50;
    enable_disk_cache = true;
    max_disk_cache_mb = 500;
    show_console = true;
    log_to_file = true;
    max_fetch_threads = 4;
    max_pending_fetches = 20;
}

bool ValidateConfig() {
    bool valid = true;

    if (g_config.volume < 0) {
        LOG_WARNING(L"Volume < 0, setting to 0");
        g_config.volume = 0;
        valid = false;
    }
    if (g_config.volume > 100) {
        LOG_WARNING(L"Volume > 100, setting to 100");
        g_config.volume = 100;
        valid = false;
    }

    if (strncmp(g_config.server, "http://", 7) != 0 &&
        strncmp(g_config.server, "https://", 8) != 0) {
        LOG_ERROR(L"Invalid server URL, must start with http:// or https://");
        g_config.SetServer("http://localhost:5050/v1");
        valid = false;
    }

    if (strcmp(g_config.format, "wav") != 0 && strcmp(g_config.format, "mp3") != 0 &&
        strcmp(g_config.format, "opus") != 0 && strcmp(g_config.format, "aac") != 0 &&
        strcmp(g_config.format, "flac") != 0) {
        LOG_WARNING(L"Unknown format, defaulting to wav");
        g_config.SetFormat("wav");
        valid = false;
    }

    if (!valid) {
        LOG_WARNING(L"Configuration validation failed, some values were corrected");
    }

    return true;
}

bool LoadConfig(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        // Set defaults when config file is not found
        g_config.SetDefaults();
        LOG_WARNING(L"Config file not found, using default settings");
        return false;
    }

    // Initialize with defaults before loading
    g_config.SetDefaults();

    std::string line;
    while (std::getline(file, line)) {
        size_t commentPos = line.find('#');
        if (commentPos != std::string::npos) {
            line = line.substr(0, commentPos);
        }

        line = trim(line);
        if (line.empty()) continue;

        size_t eqPos = line.find('=');
        if (eqPos == std::string::npos) continue;

        std::string key = trim(line.substr(0, eqPos));
        std::string value = trim(line.substr(eqPos + 1));

        if (key == "server") g_config.SetServer(value.c_str());
        else if (key == "model") g_config.SetModel(value.c_str());
        else if (key == "voice") g_config.SetVoice(value.c_str());
        else if (key == "api_key") g_config.SetApiKey(value.c_str());
        else if (key == "format") g_config.SetFormat(value.c_str());
        else if (key == "volume") g_config.volume = std::stoi(value);
        else if (key == "mute_original") g_config.mute_original = (std::stoi(value) != 0);
        else if (key == "cancel_key") g_config.SetCancelKey(value.c_str());
        else if (key == "max_cache_size") g_config.max_cache_size = std::stoi(value);
        else if (key == "log_level") g_config.SetLogLevel(value.c_str());
        else if (key == "show_console") g_config.show_console = (std::stoi(value) != 0);
        else if (key == "log_to_file") g_config.log_to_file = (std::stoi(value) != 0);
        else if (key == "max_fetch_threads") g_config.max_fetch_threads = std::stoi(value);
        else if (key == "max_pending_fetches") g_config.max_pending_fetches = std::stoi(value);
    }

    // Convert config strings to wstring for logging
    std::string server_str(g_config.server);
    std::string model_str(g_config.model);
    std::string voice_str(g_config.voice);
    std::string format_str(g_config.format);
    std::string cancel_key_str(g_config.cancel_key);
    std::string log_level_str(g_config.log_level);

    LOG_INFO(L"Config loaded successfully");
    LOG_INFO(L"  Server: " + std::wstring(server_str.begin(), server_str.end()));
    LOG_INFO(L"  Model: " + std::wstring(model_str.begin(), model_str.end()));
    LOG_INFO(L"  Voice: " + std::wstring(voice_str.begin(), voice_str.end()));
    LOG_INFO(L"  Format: " + std::wstring(format_str.begin(), format_str.end()));
    LOG_INFO(L"  Volume: " + std::to_wstring(g_config.volume) + L"%");
    LOG_INFO(L"  Mute Original: " + std::wstring(g_config.mute_original ? L"Yes" : L"No"));
    LOG_INFO(L"  Cancel Key: " + std::wstring(cancel_key_str.begin(), cancel_key_str.end()));
    LOG_INFO(L"  Max Cache Size: " + std::to_wstring(g_config.max_cache_size));
    LOG_INFO(L"  Log Level: " + std::wstring(log_level_str.begin(), log_level_str.end()));
    LOG_INFO(L"  Log to File: " + std::wstring(g_config.log_to_file ? L"Enabled" : L"Disabled"));
    LOG_INFO(L"  Max Fetch Threads: " + std::to_wstring(g_config.max_fetch_threads));
    LOG_INFO(L"  Max Pending Fetches: " + std::to_wstring(g_config.max_pending_fetches));

    // Set the log level
    g_logger.SetLogLevel(std::wstring(log_level_str.begin(), log_level_str.end()));

    // Set file logging
    g_logger.SetFileLoggingEnabled(g_config.log_to_file);

    ValidateConfig();
    return true;
}
