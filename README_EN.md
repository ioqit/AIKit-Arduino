# AIKit-Arduino

English | **[简体中文](./README.md)**

A **one-stop AI client library for Arduino**: a single `begin()` call connects you to the OpenAI-compatible Chat Completions APIs of 19 vendors (non-streaming + SSE streaming), plus **cloud speech-to-text** — one-shot HTTP transcription and WebSocket real-time recognition. Built for ESP32 / ESP8266.

```cpp
AIKitClient ai(&Serial);                 // log output target is given at construction
ai.begin(AIKIT_DEEPSEEK, "sk-...");      // one call to onboard a vendor
String reply;
ai.chat("Introduce yourself in one sentence", reply);  // non-streaming
ai.onDelta([](const char* d, size_t n) { Serial.write(d, n); });
ai.chatStream("Tell me a story");        // streaming (typewriter effect)

AIKitASRClient asr(&Serial);             // speech-to-text (one shot)
asr.begin(AIKIT_ASR_DASHSCOPE, "sk-...");
asr.transcribePCM(pcm, len, 16000, 1, text);

AIKitRealtimeASR rt(&Serial);            // real-time speech recognition (WebSocket)
rt.begin(AIKIT_RT_DASHSCOPE, "sk-...");
rt.connect(); rt.sendAudio(pcm, len); rt.loop(); rt.finish();
```

---

## ⚠ Dependencies (important — read this first)

This library is **not** zero-dependency. Everything is built on the libraries below — **missing any one of them breaks the build**:

| Library | Required? | Purpose | How to install |
| --- | --- | --- | --- |
| [ArduinoJson](https://arduinojson.org/) **v7+** | **Required** | Request building / response parsing | Install directly from the Library Manager |
| [StreamHTTPClient](https://github.com/ioqite/StreamHTTPClient) **v1.0+** | **Required** | HTTP/HTTPS transport (sync / async / SSE / gzip) | Not yet in the Library Manager — install from GitHub (see below) |
| [ArduinoWebsockets](https://github.com/gilmaimon/ArduinoWebsockets) **v0.5+** | **Optional** | Only needed for **real-time speech recognition** (WebSocket) | Install directly from the Library Manager |

**All three in three minutes** (arduino-cli):

```bash
arduino-cli lib install ArduinoJson ArduinoWebsockets
git clone https://github.com/ioqite/StreamHTTPClient.git ~/Arduino/libraries/StreamHTTPClient
git clone https://github.com/ioqit/AIKit-Arduino.git    ~/Arduino/libraries/AIKit-Arduino
```

Arduino IDE users: install ArduinoJson / ArduinoWebsockets via the *Library Manager*; for StreamHTTPClient, download the [zip](https://github.com/ioqite/StreamHTTPClient/archive/refs/heads/master.zip) and use *Sketch → Include Library → Add .ZIP Library*.

> **What happens if ArduinoWebsockets is not installed?** Chat, streaming, HTTP transcription and every other feature are **unaffected**; only the `AIKitRealtimeASR` (real-time recognition) methods return `AIKIT_ERR_NOT_SUPPORTED` with a hint in the log. Detection is automatic via `__has_include` — no configuration required. → Details in [docs/en/01_installation.md](docs/en/01_installation.md)

---

## Quick Start

```cpp
#include <AIKit.h>

AIKitClient ai(&Serial);              // logs → Serial; pass nullptr for full silence

void setup() {
  Serial.begin(115200);
  WiFi.begin("ssid", "password");
  while (WiFi.status() != WL_CONNECTED) delay(300);

  ai.begin(AIKIT_DEEPSEEK, "sk-xxx"); // ① one call to onboard (19 presets, see docs/en/02)

  // ② common parameters (optional)
  ai.setSystemPrompt("You are a rigorous embedded assistant.");
  ai.setTemperature(0.7f);
  ai.setMaxTokens(1024);

  // ③ v1.2.0 additions (optional)
  ai.setRetry(3, 1000, 8000);         // automatic retry on failure (exponential backoff)
  // ai.totalUsage()                   // cumulative token usage

  // ④ non-streaming chat
  String reply;
  if (ai.chat("Hello", reply) == AIKIT_OK) Serial.println(reply);
}
void loop() {}
```

**Multi-turn chat** (history maintained automatically; export / import as JSON for persistence):

```cpp
AIKitHistory history;
history.setWindow(8);                  // sliding window, prevents unbounded context growth
ai.chat(history, "My name is Xiao Ming", reply);   // user into history → request → reply into history
ai.chat(history, "What is my name?", reply);       // the second turn remembers the previous one
```

**Speech-to-text**: see [examples/18_ASR_Transcribe](examples/18_ASR_Transcribe/18_ASR_Transcribe.ino) (one shot) and [examples/19_ASR_Realtime](examples/19_ASR_Realtime/19_ASR_Realtime.ino) (real-time streaming).

---

## Documentation (split by topic)

| Document | Contents |
| --- | --- |
| [docs/en/01_installation.md](docs/en/01_installation.md) | Installation & dependencies in depth: three install methods, PlatformIO, the optional-dependency mechanism, and the full table of compile-time configuration macros |
| [docs/en/02_vendors.md](docs/en/02_vendors.md) | Full table of 19 chat vendor presets (endpoints / default models / auth) + a local-deployment topic (llama.cpp / Ollama / vLLM / LM Studio) |
| [docs/en/03_chat.md](docs/en/03_chat.md) | Chat API reference: parameters, custom headers / JSON injection, token usage accounting, automatic retry |
| [docs/en/04_streaming.md](docs/en/04_streaming.md) | The two streaming modes (onDelta callback / Stream line-by-line reading) and platform differences |
| [docs/en/05_asr.md](docs/en/05_asr.md) | Speech recognition in full: 8 HTTP transcription vendors + 7 real-time WebSocket vendors, each protocol and audio requirement |
| [docs/en/06_history.md](docs/en/06_history.md) | Chat history management: dynamic memory, sliding window, JSON export / import |
| [docs/en/07_advanced.md](docs/en/07_advanced.md) | Advanced: TLS certificates, error-code table, memory tuning, CI, roadmap, known limitations |

Documentation in 简体中文: [docs/](docs/)

---

## Examples at a glance (examples/, 21 in total)

| # | Topic | # | Topic |
| --- | --- | --- | --- |
| 01 | Minimal non-streaming chat | 12 | Error handling |
| 02 | Streaming callback | 13 | Logging control |
| 03 | System prompt & parameters | 14 | Full-featured demo |
| 04 | Multi-turn chat | 15 | **Token usage accounting** (new) |
| 05 | Streaming line-by-line reading | 16 | **Automatic retry on failure** (new) |
| 06 | Vendor switching | 17 | **Chat history management** (new) |
| 07 | Custom endpoint | 18 | **HTTP speech transcription** (new) |
| 08 | Custom request headers | 19 | **Real-time speech recognition** (new, WebSocket) |
| 09 | Custom JSON fields | 20 | **Root certificate import** (new) |
| 10 | TLS custom CA | 21 | **New vendor presets** (new — Groq and 7 more) |
| 11 | Reasoning model chain-of-thought | | |

---

## Supported platforms

| Platform | Status | Notes |
| --- | --- | --- |
| **ESP32** (classic / S2 / S3 / C3 / C6) | ✅ Fully supported (primary platform) | Line-by-line streaming reading is driven by a FreeRTOS background task; I2S microphone capture works out of the box |
| **ESP8266** | ✅ Fully supported (mind the memory) | TLS goes through BearSSL (the handshake needs ~25 KB of free heap); prefer the onDelta callback for long replies; for real-time ASR an ESP32 is recommended |
| Pico W / SAMD etc. | ⚠ Architecturally compatible, unverified | Follows StreamHTTPClient's platform support; feedback welcome |

---

## Acknowledgements & License

- [StreamHTTPClient](https://github.com/ioqite/StreamHTTPClient) — the HTTP/HTTPS transport layer
- [ArduinoJson](https://arduinojson.org/) — JSON building & parsing (v7)
- [ArduinoWebsockets](https://github.com/gilmaimon/ArduinoWebsockets) — the WebSocket layer for real-time speech recognition (optional)
- The open APIs and official documentation provided by each vendor

MIT License © Ioqit
