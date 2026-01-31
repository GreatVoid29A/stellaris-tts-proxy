#include "audio_player.h"
#include "config.h"
#include "utils.h"
#include "logger.h"

#include <windows.h>
#include <mmsystem.h>
#include <string>
#include <vector>

#pragma comment(lib, "winmm.lib")

std::atomic<bool> g_isPlaying{ false };
std::atomic<bool> g_shouldCancel{ false };

// Helper to construct a header if raw PCM is received
void AddWavHeader(std::vector<uint8_t>& data, int sampleRate, int channels, int bitsPerSample) {
    uint32_t dataSize = static_cast<uint32_t>(data.size());
    uint32_t totalSize = dataSize + 36;
    uint32_t byteRate = sampleRate * channels * (bitsPerSample / 8);
    uint16_t blockAlign = channels * (bitsPerSample / 8);

    std::vector<uint8_t> header(44);

    // RIFF Chunk
    memcpy(&header[0], "RIFF", 4);
    memcpy(&header[4], &totalSize, 4);
    memcpy(&header[8], "WAVE", 4);

    // fmt Subchunk
    memcpy(&header[12], "fmt ", 4);
    uint32_t fmtSize = 16;
    memcpy(&header[16], &fmtSize, 4);
    uint16_t audioFormat = 1; // PCM
    memcpy(&header[20], &audioFormat, 2);
    memcpy(&header[22], &channels, 2);
    memcpy(&header[24], &sampleRate, 4);
    memcpy(&header[28], &byteRate, 4);
    memcpy(&header[32], &blockAlign, 2);
    memcpy(&header[34], &bitsPerSample, 2);

    // data Subchunk
    memcpy(&header[36], "data", 4);
    memcpy(&header[40], &dataSize, 4);

    data.insert(data.begin(), header.begin(), header.end());
}

// Helper to fix malformed sizes in existing WAV headers
void RepairWavHeader(std::vector<uint8_t>& data) {
    if (data.size() < 44) return;

    // Check if it's actually a RIFF file
    if (memcmp(data.data(), "RIFF", 4) != 0) return;

    // 1. Fix the main RIFF chunk size (File Size - 8 bytes)
    uint32_t fileSize = static_cast<uint32_t>(data.size());
    uint32_t riffChunkSize = fileSize - 8;
    memcpy(data.data() + 4, &riffChunkSize, 4);

    // 2. Locate the "data" subchunk. 
    // Usually at offset 36, but sometimes extra metadata exists (LIST info), so we search.
    // We search the first 200 bytes or end of file.
    size_t searchLimit = (data.size() < 200) ? data.size() : 200;

    for (size_t i = 12; i < searchLimit - 8; i++) {
        if (memcmp(data.data() + i, "data", 4) == 0) {
            // Found data tag. The next 4 bytes are the size.
            // Calculate correct data size: Total File Size - (offset of 'data' + 8 bytes for tag+size)
            uint32_t dataSubchunkPos = static_cast<uint32_t>(i);
            uint32_t actualDataSize = fileSize - (dataSubchunkPos + 8);

            memcpy(data.data() + i + 4, &actualDataSize, 4);
            LOG_DEBUG(L"Repaired WAV header sizes.");
            return;
        }
    }
}

void PlayAudioFromMemory(const std::vector<uint8_t>& inputAudioData, const std::string* cachedFilePath) {
    if (inputAudioData.empty()) {
        LOG_ERROR(L"No audio data to play");
        return;
    }

    // Work on a mutable copy so we can fix headers
    std::vector<uint8_t> audioData = inputAudioData;
    char tempFile[MAX_PATH];
    bool useCachedFile = false;

    // --- 1. DATA PREPARATION & HEADER FIXING ---

    bool isWav = g_config.FormatEquals("wav");

    // If it claims to be a WAV, we must ensure the header is valid for MCI
    if (isWav) {
        // Check if "RIFF" tag exists
        if (audioData.size() >= 4 && memcmp(audioData.data(), "RIFF", 4) == 0) {
            // It has a header, but sizes might be wrong (ffmpeg: "Ignoring maximum wav data size")
            RepairWavHeader(audioData);
        }
        else {
            // No Header found? It's raw PCM.
            // Based on your FFmpeg output: 24000 Hz, Mono (1 ch), s16 (16 bit)
            LOG_WARNING(L"Raw PCM data detected. Adding 24kHz Mono Header.");
            AddWavHeader(audioData, 24000, 1, 16);
        }
    }

    // --- 2. FILE CREATION ---

    if (!useCachedFile) {
        char tempPath[MAX_PATH];
        GetTempPathA(MAX_PATH, tempPath);

        std::string extension = "." + std::string(g_config.format);
        std::string tempFileName = std::string(tempPath) + "stellaris_tts_" + std::to_string(GetTickCount()) + extension;
        strncpy_s(tempFile, tempFileName.c_str(), MAX_PATH - 1);

        HANDLE hFile = CreateFileA(tempFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, NULL);
        if (hFile == INVALID_HANDLE_VALUE) {
            LOG_ERROR(L"Failed to create temp file");
            return;
        }

        DWORD bytesWritten;
        WriteFile(hFile, audioData.data(), audioData.size(), &bytesWritten, NULL);
        FlushFileBuffers(hFile);
        CloseHandle(hFile);
    }

    // --- 3. MCI PLAYBACK ---

    g_isPlaying = true;
    g_shouldCancel = false;

    // Explicitly use 'waveaudio' for wav files to avoid codec issues
    std::string deviceType = (isWav) ? "waveaudio" : "mpegvideo";
    std::string aliasName = "tts_" + std::to_string(GetTickCount());

    // Escape path for MCI: C:\Temp -> C:\\Temp
    std::string escapedPath = tempFile;
    size_t pos = 0;
    while ((pos = escapedPath.find('\\', pos)) != std::string::npos) {
        escapedPath.replace(pos, 1, "\\\\");
        pos += 2;
    }

    std::string mciCmd = "open \"" + escapedPath + "\" type " + deviceType + " alias " + aliasName;

    char errorBuf[256];
    MCIERROR err = mciSendStringA(mciCmd.c_str(), NULL, 0, NULL);

    if (err != 0) {
        mciGetErrorStringA(err, errorBuf, sizeof(errorBuf));
        LOG_ERROR(L"MCI Open Error: " + std::wstring(errorBuf, errorBuf + strlen(errorBuf)));
    }
    else {
        // Set volume (0-1000)
        int vol = static_cast<int>(g_config.volume * 10);
        mciSendStringA(("setaudio " + aliasName + " volume to " + std::to_string(vol)).c_str(), NULL, 0, NULL);

        err = mciSendStringA(("play " + aliasName).c_str(), NULL, 0, NULL);

        if (err == 0) {
            // Get duration to prevent infinite hanging
            char durBuf[128] = { 0 };
            mciSendStringA(("status " + aliasName + " length").c_str(), durBuf, sizeof(durBuf), NULL);
            DWORD duration = atoi(durBuf);
            if (duration == 0) duration = 5000;

            DWORD start = GetTickCount();

            // Wait loop
            while (!g_shouldCancel) {
                char modeBuf[128] = { 0 };
                mciSendStringA(("status " + aliasName + " mode").c_str(), modeBuf, sizeof(modeBuf), NULL);
                std::string mode = modeBuf;

                if (mode != "playing" && mode != "paused") break;
                if (GetTickCount() - start > duration + 2000) break; // Timeout

                Sleep(50);
            }

            if (g_shouldCancel) mciSendStringA(("stop " + aliasName).c_str(), NULL, 0, NULL);
        }
        else {
            mciGetErrorStringA(err, errorBuf, sizeof(errorBuf));
            LOG_ERROR(L"MCI Play Error: " + std::wstring(errorBuf, errorBuf + strlen(errorBuf)));
        }

        mciSendStringA(("close " + aliasName).c_str(), NULL, 0, NULL);
    }

    g_isPlaying = false;

    if (!useCachedFile) DeleteFileA(tempFile);
}