# Garvis

A real-time voice assistant that runs entirely on your local machine. You speak, it listens, thinks, and talks back — no cloud, no API keys, no internet required.

<img width="903" height="732" alt="image" src="https://github.com/user-attachments/assets/2b42b72c-c3d2-4b82-b038-4368478b4ef2" />

## How It Works

```
 You speak        Whisper STT        LLM (llama.cpp)       Piper TTS        You hear
 [F1 hold] -----> transcribe ------> generate reply ------> synthesize ----> speaker
                                                                    \
                                                                     +----> VB-CABLE --> Discord
```

Garvis chains three local AI models into a single voice pipeline:

1. **Speech-to-Text** — [whisper.cpp](https://github.com/ggerganov/whisper.cpp) transcribes your microphone input.
2. **Language Model** — [llama.cpp](https://github.com/ggerganov/llama.cpp) generates a response using a local GGUF model (e.g. Llama 3.1 8B).
3. **Text-to-Speech** — [Piper](https://github.com/rhasspy/piper) speaks the response aloud.

Audio is handled by [miniaudio](https://miniaud.io) and the UI by [raylib](https://www.raylib.com).

## Features

- **Push-to-talk** — hold F1 to speak, release to send. Mic is off otherwise.
- **Stop response** — press F2 or click the STOP button to interrupt Garvis mid-sentence.
- **Multilingual** — toggle between English and Russian via the EN/RU buttons in the footer. Each language has its own Whisper model, Piper voice, and system prompt.
- **Discord loopback** — with [VB-CABLE](https://vb-audio.com/Cable/) installed, Garvis mixes your real microphone + its TTS audio into the virtual cable. Discord friends hear both you and Garvis through "CABLE Output."
- **Audio settings panel** — click AUD in the footer to select microphone, speaker, loopback device, and adjust loopback volume.
- **Streaming TTS** — Garvis starts speaking as soon as the first sentence is generated, not after the full response.
- **Fully offline** — everything runs locally. No data leaves your machine.

## Requirements

- **OS**: Windows 10/11 (WASAPI audio backend)
- **Compiler**: MSVC (Visual Studio 2022) or MinGW with C++17 support
- **CMake**: 3.20+
- **RAM**: ~8 GB free (for a 7-8B parameter model)
- **Optional**: VB-CABLE for Discord loopback

## Models

Download these into `garvis/models/`:

| Component | File | Source |
|-----------|------|--------|
| LLM | `llama.gguf` (or any GGUF chat model) | [HuggingFace](https://huggingface.co/bartowski/Meta-Llama-3.1-8B-Instruct-GGUF) |
| STT (English) | `ggml-base.en.bin` | [whisper.cpp models](https://huggingface.co/ggerganov/whisper.cpp) |
| STT (Russian) | `ggml-small.bin` | [whisper.cpp models](https://huggingface.co/ggerganov/whisper.cpp) |
| TTS (English) | `piper/en_US-amy-low.onnx` + `.json` | [Piper voices](https://huggingface.co/rhasspy/piper-voices) |
| TTS (Russian) | `piper/ru_RU-ruslan-medium.onnx` + `.json` | [Piper voices](https://huggingface.co/rhasspy/piper-voices) |
| TTS engine | `piper/piper.exe` | [Piper releases](https://github.com/rhasspy/piper/releases) |

## Building

```bash
cd garvis
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

GPU acceleration (optional):

```bash
cmake -B build -DGARVIS_GPU_CUDA=ON     # NVIDIA CUDA
```

## Running

```bash
./build/Release/garvis.exe
```

## Controls

| Key / Button | Action |
|-------------|--------|
| **F1** (hold) | Push-to-talk — record while held, send on release |
| **F2** | Stop Garvis mid-response |
| **EN / RU** | Toggle language |
| **AUD** | Open audio device settings |
| **STOP** | Stop button (visible while Garvis is speaking) |

## Discord Setup

1. Install [VB-CABLE](https://vb-audio.com/Cable/).
2. Launch Garvis — it auto-detects VB-CABLE and starts the audio mixer.
3. In Discord: set your input device to **CABLE Output (VB-Audio Virtual Cable)**.
4. Your friends hear your voice + Garvis through the virtual cable.
5. Adjust the loopback volume slider in the AUD settings panel.

## Project Structure

```
garvis/
  src/
    main.cpp                 # Main loop and pipeline orchestration
    config/                  # Application defaults and settings
    ui/                      # Raylib-based GUI (header, chat, footer, settings)
    audio/
      audio_capture.cpp      # Microphone input with PTT
      audio_devices.cpp      # Device enumeration and virtual cable detection
      audio_mixer.cpp        # Duplex mic+TTS mixer for VB-CABLE
      resample.h             # Shared linear interpolation resampler
    stt/                     # Whisper.cpp speech-to-text wrapper
    llm/                     # Llama.cpp inference engine
    tts/
      tts.cpp                # Piper subprocess manager
      audio_player.cpp       # Speaker playback (stereo f32, WASAPI)
    pipeline/
      tts_streamer.cpp       # Streaming synth+playback pipeline
  models/                    # Place AI models here (not tracked by git)
  third_party/miniaudio/     # Single-header audio library
  CMakeLists.txt             # Build configuration
```
