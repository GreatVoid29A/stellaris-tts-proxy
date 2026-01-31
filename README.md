# Stellaris TTS Proxy

A Windows DLL proxy that replaces Stellaris's built-in text-to-speech with high-quality TTS from any OpenAI-compatible server. Get lifelike, expressive voices - almost like a professional narrator reading your events and tooltips.

## Important – Alpha Status

This is an **alpha version**. It hasn't been extensively tested and may contain bugs, fail to work in some situations, cause crashes, or behave unpredictably.

## Disclaimer

**This involves placing custom DLL files (`version.dll` and `tts_settings.txt`) in your game folder, which can sometimes cause crashes, instability, or other weird issues in Stellaris. I take no responsibility whatsoever for any problems, crashes, save corruption, bans in multiplayer, or anything else that might happen. Use at your own risk - back up your saves and game files first.**

**When using OpenAI's servers, TTS costs real money (it uses your API credits).** I don't take responsibility if this uses up your OpenAI credits or costs you money. **Strongly recommend** using prepaid credits only, or set strict monthly spending limits in your OpenAI account settings right away. Check your usage often!

**Want zero cost?** Use a local OpenAI-compatible TTS server instead (see [Advanced](#-advanced-use-any-openai-compatible-tts-server-freelocal-options) section below).

This is a third-party mod. Not affiliated with or endorsed by Paradox Interactive.

## Features

- **Interceptor Hooks** - Captures game TTS through Windows SAPI hooking
- **OpenAI-Compatible API** - Works with OpenAI or any compatible endpoint
- **Multi-Language Support** - Supports all languages OpenAI TTS can handle (English, German, Korean, and more)
- **Non-Blocking** - Game continues immediately while TTS downloads in background
- **Audio Caching** - Disk caching for instant replay
- **Fully Configurable** - Simple config file for server, voice, volume, and format
- **Cancellable Playback** - Press hotkey (default F9) to stop audio
- **Non-Intrusive** - Works as DLL proxy without modifying game files

## Installation

1. Download the two files you need from the [GitHub Releases](https://github.com/GreatVoid29A/stellaris-tts-proxy/releases): `version.dll` and `tts_settings.txt`.
2. Copy both files directly into your Stellaris installation folder - the one that has `Stellaris.exe` in it.
   - Steam example: `C:\Program Files (x86)\Steam\steamapps\common\Stellaris`
3. To remove/uninstall later, just delete those two files. Easy.

**Note:** Some antivirus might flag the DLL (common with injection-style workarounds). If it does, whitelist the Stellaris folder.

## Configuration

### 1. Get an API Key

Go to the [OpenAI API Keys page](https://platform.openai.com/api-keys) and create a new secret key.

### 2. Edit tts_settings.txt

Open `tts_settings.txt` with Notepad (or any text editor). Here's a sample with the best defaults:

```ini
# Recommended settings for top quality
model=gpt-4o-mini-tts
voice=marin  # Or cedar for best quality
api_key=sk-your-actual-api-key-here
```

Replace `sk-your-actual-api-key-here` with your real key (if using OpenAI):

**Never share this key!**

### Full Configuration Options

```ini
# ==================== SERVER SETTINGS ====================

# OpenAI-compatible TTS server URL (without trailing slash)
server=https://api.openai.com/v1

# TTS model to use
# Options: tts-1, tts-1-hd, gpt-4o-mini-tts
# Recommended for OpenAI: gpt-4o-mini-tts
model=gpt-4o-mini-tts

# Voice to use
# Options: alloy, ash, bald, coral, echo, fable, nova, onyx, sage, shimmer, verse, marin, cedar
# Recommended for best quality: marin, cedar
voice=marin

# API key (leave empty if not required, or use dummy key)
api_key=sk-your-actual-api-key-here

# ==================== AUDIO SETTINGS ====================

# Audio format: wav, mp3, opus, aac, flac
format=mp3

# Volume level (0-100)
volume=100

# ==================== GAME SETTINGS ====================

# Mute original game TTS (1 = mute, 0 = play both)
mute_original=1

# Hotkey to cancel current audio playback
# Options: F1-F12, ESC, SPACE, ENTER, TAB, A-Z, 0-9
# Default: F9
cancel_key=F9

# ==================== LOGGING SETTINGS ====================

# Log level: debug, info, warning, error
log_level=info

# Show console window (1 = show, 0 = hide)
show_console=1

# Enable file logging to tts_proxy.log (1 = enabled, 0 = disabled)
log_to_file=1
```

## Billing & Costs

When using OpenAI servers:
- **gpt-4o-mini-tts**: approx. $0.015/min. Much cheaper/faster than older models.
- Older: `tts-1` approx. $15/1M chars | `tts-1-hd` approx. $30/1M chars

Add $5 credit at [OpenAI Billing](https://platform.openai.com/account/billing/overview), then **set a hard monthly limit** (e.g. $10 max) or use prepaid only. Monitor usage regularly.

## Enabling TTS In-Game

1. Launch Stellaris.
2. Main menu → **Settings** → **Accessibility**.
3. Turn **ON** both:
   - Text to Speech
   - Text to Speech (Tooltips)
4. Hit **Apply** and close the menu.
5. **Restart the game** (important - don't skip).

Load a game. Click the speaker icon in events, anomalies, or tooltips. You should hear super natural OpenAI voices. Press **F9** (default) to cancel playback if needed.

## Uninstallation

Delete these files from the Stellaris directory:
- `version.dll`
- `tts_settings.txt`

## Troubleshooting

| Problem | Solution |
|---------|----------|
| No TTS audio | Check console/logs, verify TTS server is running, verify API key is correct |
| Original TTS plays | Set `mute_original=1` in config |
| Console hidden | Set `show_console=1` in config |
| Cancel key not working | Check console for "Hotkey registered" message |
| Antivirus flags DLL | Whitelist the Stellaris folder |

See `tts_proxy.log` in Stellaris directory for detailed diagnostics.

## 🛠️ Advanced: Use Any OpenAI-Compatible TTS Server (Free/Local Options!)

Want **zero cost** and **offline TTS**? Point the config to a local OpenAI-compatible server.

### Popular Free/Local Servers

- **Edge TTS** (Microsoft Edge voices, free): https://github.com/travisvn/openai-edge-tts
- **Kokoro-FastAPI** (local AI models, GPU/CPU): https://github.com/remsky/Kokoro-FastAPI

### Setup

1. Run the server (follow their README). They usually listen on something like `http://localhost:8880/v1`.

2. Update `tts_settings.txt`:
   ```ini
   # No trailing slash
   server=http://localhost:8880/v1
   model=tts-1
   voice=af_heart
   api_key=dummy_key
   ```

3. Restart Stellaris → TTS now uses your local server. Completely free, private, and unlimited.

## License

MIT License - see [LICENSE](LICENSE) file.

### Third-Party Licenses

- **MinHook** - BSD 2-Clause License (see [LICENSES/third-party.txt](LICENSES/third-party.txt))

## Credits

- **MinHook** by Tsuda Kageyu
- **Paradox Interactive** - Stellaris

---

**Enjoy high-quality TTS in Stellaris!**
