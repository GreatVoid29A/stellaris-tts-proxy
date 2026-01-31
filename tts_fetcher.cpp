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

#include "tts_fetcher.h"
#include "config.h"
#include "utils.h"
#include "logger.h"

#include <sstream>
#include <iomanip>

std::vector<uint8_t> FetchTTSAudioWithRetry(const std::string& text, int maxRetries) {
    std::ostringstream jsonBody;
    jsonBody << "{"
        << "\"model\":\"" << EscapeJSON(g_config.model) << "\","
        << "\"input\":\"" << EscapeJSON(text) << "\","
        << "\"voice\":\"" << EscapeJSON(g_config.voice) << "\","
        << "\"response_format\":\"" << EscapeJSON(g_config.format) << "\""
        << "}";

    std::string jsonString = jsonBody.str();
    std::string fullUrl = std::string(g_config.server) + "/audio/speech";

    for (int attempt = 0; attempt < maxRetries; ++attempt) {
        if (attempt > 0) {
            LOG_INFO(L"Retry attempt " + std::to_wstring(attempt + 1) + L" of " + std::to_wstring(maxRetries));
            // Proper exponential backoff
            DWORD delayMs = 500 * (1 << attempt); // 500ms, 1000ms, 2000ms
            Sleep(delayMs);
        }

        LOG_DEBUG(L"Connecting to: " + std::wstring(fullUrl.begin(), fullUrl.end()));

        InternetHandle hInternet(InternetOpenA("StellarTTS/1.0", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0));
        if (!hInternet) {
            LOG_ERROR(L"Failed to initialize WinINet: " + GetWindowsErrorMessage(GetLastError()));
            continue;
        }

        DWORD dwTimeout = 15000;
        InternetSetOptionA(hInternet, INTERNET_OPTION_CONNECT_TIMEOUT, &dwTimeout, sizeof(DWORD));
        dwTimeout = 30000;
        InternetSetOptionA(hInternet, INTERNET_OPTION_RECEIVE_TIMEOUT, &dwTimeout, sizeof(DWORD));

        URL_COMPONENTSA urlComp = { sizeof(URL_COMPONENTSA) };
        char hostname[256] = { 0 };
        char urlPath[1024] = { 0 };
        urlComp.lpszHostName = hostname;
        urlComp.dwHostNameLength = sizeof(hostname);
        urlComp.lpszUrlPath = urlPath;
        urlComp.dwUrlPathLength = sizeof(urlPath);

        if (!InternetCrackUrlA(fullUrl.c_str(), 0, 0, &urlComp)) {
            LOG_ERROR(L"Failed to parse URL");
            continue;
        }

        InternetHandle hSession(InternetConnectA(hInternet, hostname, urlComp.nPort,
            NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0));

        if (!hSession) {
            LOG_ERROR(L"Failed to create session: " + GetWindowsErrorMessage(GetLastError()));
            continue;
        }

        DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE;
        if (urlComp.nScheme == INTERNET_SCHEME_HTTPS) {
            flags |= INTERNET_FLAG_SECURE;
            // Enable certificate validation
            flags |= INTERNET_FLAG_KEEP_CONNECTION;
        }

        InternetHandle hRequest(HttpOpenRequestA(hSession, "POST", urlPath, NULL, NULL, NULL, flags, 0));

        if (!hRequest) {
            LOG_ERROR(L"Failed to create request: " + GetWindowsErrorMessage(GetLastError()));
            continue;
        }

        std::string headers = "Content-Type: application/json\r\n";
        if (!g_config.ApiKeyEmpty()) {
            headers += "Authorization: Bearer " + std::string(g_config.api_key) + "\r\n";
        }

        BOOL result = HttpSendRequestA(hRequest, headers.c_str(), headers.length(),
            (LPVOID)jsonString.c_str(), jsonString.length());

        if (!result) {
            DWORD error = GetLastError();
            LOG_ERROR(L"Failed to send request: " + GetWindowsErrorMessage(error));
            continue;
        }

        DWORD statusCode = 0;
        DWORD statusCodeSize = sizeof(statusCode);
        HttpQueryInfoA(hRequest, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
            &statusCode, &statusCodeSize, NULL);

        if (statusCode != 200) {
            LOG_ERROR(L"Server returned status code: " + std::to_wstring(statusCode));

            BYTE errorBuffer[1024];
            DWORD errorBytesRead = 0;
            if (InternetReadFile(hRequest, errorBuffer, sizeof(errorBuffer) - 1, &errorBytesRead) && errorBytesRead > 0) {
                errorBuffer[errorBytesRead] = 0;
                LOG_ERROR(L"Server error response: " + std::wstring((char*)errorBuffer, (char*)errorBuffer + errorBytesRead));
            }

            if (statusCode >= 400 && statusCode < 500) {
                break; // Don't retry client errors
            }
            continue;
        }

        // Check Content-Length and reserve space
        DWORD contentLength = 0;
        DWORD contentLengthSize = sizeof(contentLength);
        HttpQueryInfoA(hRequest, HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER,
            &contentLength, &contentLengthSize, NULL);

        std::vector<uint8_t> audioData;
        if (contentLength > 0 && contentLength < 50 * 1024 * 1024) { // Sanity check: < 50MB
            audioData.reserve(contentLength);
        }

        BYTE buffer[4096];
        DWORD bytesRead = 0;

        while (InternetReadFile(hRequest, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
            audioData.insert(audioData.end(), buffer, buffer + bytesRead);
        }

        LOG_INFO(L"Downloaded " + std::to_wstring(audioData.size()) + L" bytes of audio");
        return audioData;
    }

    LOG_ERROR(L"Failed to fetch audio after " + std::to_wstring(maxRetries) + L" attempts");
    return {};
}

std::vector<uint8_t> FetchTTSAudio(const std::string& text) {
    return FetchTTSAudioWithRetry(text, 3);
}
