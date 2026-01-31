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

#ifndef TTS_STELLARIS_UTILS_H
#define TTS_STELLARIS_UTILS_H

#include <string>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <windows.h>
#include "logger.h"

// Utility functions

inline std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

// Validate UTF-8 sequence
inline bool IsValidUTF8(const std::string& str) {
    size_t i = 0;
    while (i < str.length()) {
        unsigned char c = str[i];

        if (c < 0x80) {
            i++;
        }
        else if ((c & 0xE0) == 0xC0) {
            if (i + 1 >= str.length() || (str[i + 1] & 0xC0) != 0x80) return false;
            i += 2;
        }
        else if ((c & 0xF0) == 0xE0) {
            if (i + 2 >= str.length() || (str[i + 1] & 0xC0) != 0x80 || (str[i + 2] & 0xC0) != 0x80) return false;
            i += 3;
        }
        else if ((c & 0xF8) == 0xF0) {
            if (i + 3 >= str.length() || (str[i + 1] & 0xC0) != 0x80 || (str[i + 2] & 0xC0) != 0x80 || (str[i + 3] & 0xC0) != 0x80) return false;
            i += 4;
        }
        else {
            return false;
        }
    }
    return true;
}

inline bool SanitizeText(std::string& text) {
    const size_t MAX_LENGTH = 5000;

    text.erase(std::remove(text.begin(), text.end(), '\0'), text.end());

    // Validate UTF-8
    if (!IsValidUTF8(text)) {
        LOG_WARNING(L"Invalid UTF-8 sequence detected, text may be corrupted");
    }

    if (text.length() > MAX_LENGTH) {
        text.resize(MAX_LENGTH);
        LOG_WARNING(L"Text truncated to " + std::to_wstring(MAX_LENGTH) + L" characters");
    }

    return !text.empty();
}

inline std::string WideToUTF8(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

inline std::string EscapeJSON(const std::string& str) {
    std::ostringstream oss;
    for (size_t i = 0; i < str.length(); ++i) {
        unsigned char c = static_cast<unsigned char>(str[i]);

        switch (c) {
        case '"':  oss << "\\\""; break;
        case '\\': oss << "\\\\"; break;
        case '\b': oss << "\\b"; break;
        case '\f': oss << "\\f"; break;
        case '\n': oss << "\\n"; break;
        case '\r': oss << "\\r"; break;
        case '\t': oss << "\\t"; break;
        default:
            if (c < 0x20) {
                oss << "\\u"
                    << std::hex << std::setw(4) << std::setfill('0')
                    << static_cast<int>(c)
                    << std::dec;
            }
            else {
                oss << c;
            }
        }
    }
    return oss.str();
}

// Escape path for MCI commands
inline std::string EscapeMCIPath(const std::string& path) {
    std::string escaped;
    for (char c : path) {
        if (c == '"') {
            escaped += "\\\"";
        }
        else {
            escaped += c;
        }
    }
    return escaped;
}

// Get human-readable Windows error message
inline std::wstring GetWindowsErrorMessage(DWORD error) {
    wchar_t* messageBuffer = nullptr;
    size_t size = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPWSTR)&messageBuffer, 0, NULL);

    std::wstring message(messageBuffer, size);
    LocalFree(messageBuffer);
    return message;
}

// Better string validation without SEH abuse
inline bool IsValidStringPointer(const WCHAR* text) {
    if (text == nullptr) return false;

    // Check if pointer is in valid address range (above 0x10000 to avoid null page)
    if (reinterpret_cast<uintptr_t>(text) < 0x10000) return false;

    // Use IsBadReadPtr equivalent through VirtualQuery
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(text, &mbi, sizeof(mbi)) == 0) return false;

    if (mbi.State != MEM_COMMIT) return false;
    if (mbi.Protect == PAGE_NOACCESS || mbi.Protect == PAGE_EXECUTE) return false;

    // Try to read first character safely
    __try {
        volatile WCHAR c = text[0];
        return c != L'\0';
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

#endif // TTS_STELLARIS_UTILS_H
