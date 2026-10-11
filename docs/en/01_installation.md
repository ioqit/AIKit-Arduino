# 01 - Installation & Dependencies

English | **[简体中文](../01_installation.md)**

This library is **not** zero-dependency: the core features (chat / streaming / history management / HTTP transcription) rely on two required libraries, and real-time speech recognition adds one optional library. With the dependencies in place, your first example runs within 5 minutes.

## Dependency overview

| Library | Required? | Purpose | How to get it |
| --- | --- | --- | --- |
| [ArduinoJson](https://arduinojson.org/) **v7+** | **Required** | Request JSON building / response parsing | Install directly from the Library Manager |
| [StreamHTTPClient](https://github.com/ioqit/StreamHTTPClient) **v1.0+** | **Required** | HTTP/HTTPS transport layer (sync / async / SSE / gzip) | **Not in the Library Manager** — install from GitHub |
| [ArduinoWebsockets](https://github.com/gilmaimon/ArduinoWebsockets) **v0.5+** | **Optional** | Only needed for real-time speech recognition (WebSocket) | Install directly from the Library Manager |

Each library has its own role:

- **StreamHTTPClient** — handles all HTTP/HTTPS traffic: synchronous requests for non-streaming calls, chunked callbacks / line-by-line push for SSE streaming, gzip decompression, and TLS (BearSSL on ESP8266). It also dictates this library's execution model (background task on ESP32 / synchronous on ESP8266) — see the platform-difference table in [04_streaming.md](04_streaming.md).
- **ArduinoJson** — builds request bodies and parses responses (the v7 `JsonDocument` API). A missing or outdated install fails the build outright.
- **ArduinoWebsockets** — used only by `AIKitRealtimeASR` (real-time recognition); see "Optional dependency mechanism" below and [05_asr.md](05_asr.md).

## Method 1: Arduino IDE

1. Install ArduinoJson: *Tools → Manage Libraries*, search for `ArduinoJson` and install (v7+ required).
2. Install StreamHTTPClient: it is **not yet indexed in the Library Manager** — download the [zip package](https://github.com/ioqit/StreamHTTPClient/archive/refs/heads/master.zip), then use *Sketch → Include Library → Add .ZIP Library*.
3. Install AIKit-Arduino itself: likewise download the [zip](https://github.com/ioqit/AIKit-Arduino/archive/refs/heads/master.zip) and go through *Add .ZIP Library*; when upgrading, delete the old library folder first and re-import.
4. (Optional, only for real-time ASR) search for `ArduinoWebsockets` in the Library Manager and install it.

## Method 2: arduino-cli

```bash
# Required dependencies (ArduinoWebsockets installed as well, covering the real-time ASR examples)
arduino-cli lib install ArduinoJson ArduinoWebsockets

# StreamHTTPClient is not indexed — git clone into the user library directory
git clone https://github.com/ioqit/StreamHTTPClient.git ~/Arduino/libraries/StreamHTTPClient

# This library
git clone https://github.com/ioqit/AIKit-Arduino.git ~/Arduino/libraries/AIKit-Arduino
```

On Windows the user library directory is usually `Documents\Arduino\libraries\`.

## Method 3: git clone (for tracking upstream / contributing)

```bash
cd ~/Arduino/libraries
git clone https://github.com/ioqite/StreamHTTPClient.git
git clone https://github.com/ioqit/AIKit-Arduino.git
cd AIKit-Arduino && git checkout v1.2.0    # pin to a release tag (optional)
```

## PlatformIO users

`lib_deps` for your `platformio.ini` (all three libraries in one go):

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
lib_deps =
  bblanchon/ArduinoJson@^7
  https://github.com/ioqit/StreamHTTPClient.git
  gilmaimon/ArduinoWebsockets@^0.5
```

- Dependencies given as direct GitHub URLs (StreamHTTPClient) run `git clone` on every build — a slow first build is normal;
- If you don't need real-time speech recognition, drop the `ArduinoWebsockets` line; every other feature keeps working;
- For ESP8266 projects simply swap `platform`/`board` to `espressif8266` / `nodemcuv2` (mind the memory, see [07_advanced.md](07_advanced.md)).

## Optional dependency mechanism (ArduinoWebsockets)

Real-time ASR uses an "**enabled automatically when installed, degrades gracefully when not**" detection scheme — no manual configuration of any kind:

| Stage | Behavior |
| --- | --- |
| Compile-time detection | `AIKitConfig.h` probes for the library with `__has_include(<ArduinoWebsockets.h>)`: installed → `AIKIT_HAS_RTASR=1`; not installed → `=0` |
| When not installed | Every other feature of the library (chat / streaming / history / HTTP transcription) is **completely unaffected**; only `AIKitRealtimeASR` methods uniformly return `AIKIT_ERR_NOT_SUPPORTED`, with a hint in the log |
| Forced override | Define `AIKIT_FORCE_RTASR=1` (force on) or `AIKIT_FORCE_RTASR=0` (force off) at compile time to override auto-detection — e.g. the library is installed but you want the real-time ASR code path off for a while |

```ini
; PlatformIO example: force-disable real-time ASR
build_flags = -D AIKIT_FORCE_RTASR=0
```

## Complete table of compile-time configuration macros

Every macro can be overridden via `-D` or `build_flags` before `AIKit.h` is included (`#ifndef`-guarded, so no library source edits are needed). Unless noted otherwise, a macro applies to both platforms.

### Basic macros (since v0.1)

| Macro | Default | Purpose |
| --- | --- | --- |
| `AIKIT_MAX_CUSTOM_HEADERS` | 8 | Capacity for custom HTTP headers (`addHeader`); beyond it returns `AIKIT_ERR_TOO_MANY` |
| `AIKIT_MAX_JSON_FIELDS` | 12 | Capacity for custom JSON fields (`addJsonField`) |
| `AIKIT_SSE_LINE_MAX` | 2048 | Maximum bytes per SSE line; an overlong line is dropped and the parser resynchronizes at the next newline (sets `AIKIT_ERR_LINE_TOO_LONG`). Raise it when a model emits very large single chunks |
| `AIKIT_LINE_RING_SLOTS` | 16 | [ESP32 only] Number of ring-buffer slots in line-by-line reading mode. When the consumer is slow the producer back-pressures and waits, so it need not be large |
| `AIKIT_LINE_PUSH_TIMEOUT_MS` | 30000 | [ESP32 only] Maximum wait (ms) for the producer when the ring buffer is full; on timeout sets `AIKIT_ERR_BUFFER_OVERFLOW` and degrades gracefully |
| `AIKIT_LINE_QUEUE_MAX_LINES` | 96 | [non-ESP32 only] Line-count cap of the line queue in line-by-line reading mode |
| `AIKIT_LINE_QUEUE_MAX_BYTES` | 32768 | [non-ESP32 only] Byte cap of the line queue (keeps long replies from exhausting the ESP8266 heap) |
| `AIKIT_ERRBODY_MAX` | 384 | Length of the error response body kept on non-2xx (`lastErrorBody()`) |
| `AIKIT_LOG_BUF` | 160 | Maximum length of a single log entry (prefix included) |
| `AIKIT_DEFAULT_CONNECT_TIMEOUT_MS` | 10000 | Default connect timeout (when `setTimeouts` has not been called) |
| `AIKIT_DEFAULT_TCP_TIMEOUT_MS` | 30000 | Default read timeout (first bytes from reasoning models can take tens of seconds, hence the generous value) |

### Macros added in v1.2.0

| Macro | Default | Purpose |
| --- | --- | --- |
| `AIKIT_DEFAULT_MAX_RETRIES` | 2 | Default number of automatic retries when `setRetry()` has not been called |
| `AIKIT_RETRY_BASE_DELAY_MS` | 1000 | Initial wait of the exponential backoff (retry n waits `base << n` ms) |
| `AIKIT_RETRY_MAX_DELAY_MS` | 30000 | Cap of the exponential backoff wait |
| `AIKIT_HISTORY_MAX_MESSAGES` | 128 | Message-count safety valve for chat history (`AIKitHistory`); beyond it `append` returns `AIKIT_ERR_HISTORY_LIMIT` |
| `AIKIT_HISTORY_MAX_BYTES` | 48 KB | Byte-count safety valve for the total role+content of chat history |
| `AIKIT_ASR_HTTP_TIMEOUT_MS` | 90000 | Overall timeout of one-shot HTTP transcription (audio upload + inference) |
| `AIKIT_ASR_MAX_UPLOAD_BYTES` | 65536 | Build cap for HTTP transcription request bodies (multipart / base64); beyond it returns `AIKIT_ERR_PAYLOAD_TOO_BIG`. On ESP8266 consider lowering it (e.g. 16384) |
| `AIKIT_RTASR_RECV_BUF` | 4096 | Receive / decompress buffer cap per frame for real-time recognition |
| `AIKIT_RTASR_CONNECT_MS` | 10000 | WebSocket connect timeout for real-time recognition |
| `AIKIT_RTASR_PING_MS` | 15000 | Protocol-level ping interval during idle periods of real-time recognition (keep-alive) |
| `AIKIT_FORCE_RTASR` | (undefined) | Force-enable / disable the real-time ASR code path, overriding `__has_include` auto-detection — see above |

There are also read-only version macros `AIKIT_VERSION` (`"1.2.0"`) and `AIKIT_VERSION_MAJOR/MINOR/PATCH` for conditional compilation.

## Verifying the installation

Once everything is installed, compile a minimal example (i.e. [examples/01_ChatBasic](../../examples/01_ChatBasic/01_ChatBasic.ino)):

```cpp
#include <AIKit.h>

AIKitClient ai(&Serial);

void setup() {
  Serial.begin(115200);
  // ...after connecting to WiFi...
  ai.begin(AIKIT_DEEPSEEK, "sk-xxx");   // 19 presets — see docs/en/02_vendors.md
  String reply;
  ai.chat("Hello", reply);
}
void loop() {}
```

- If you get `AIKit.h: No such file or directory` → this library was installed to the wrong location;
- If you get `StreamHTTPClient.h: No such file or directory` → the transport dependency is missing (note that it is **not** in the Library Manager index);
- If you get ArduinoJson-related template errors → check that ArduinoJson is v7+ (the v6 API is different).

Next step: pick a vendor in [02_vendors.md](02_vendors.md), or read the complete API in [03_chat.md](03_chat.md).
