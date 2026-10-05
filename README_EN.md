# AIKit-Arduino

English | **[简体中文](./README.md)**

A **chat-completions client library for Arduino** that talks to OpenAI-compatible LLM APIs from major Chinese and international providers with a single `begin()` call. Supports both **non-streaming** and **SSE streaming** modes, built for ESP32 / ESP8266.

```cpp
AIKitClient ai(&Serial);                 // log target passed at construction
ai.begin(AIKIT_DEEPSEEK, "sk-...");      // one call to pick a vendor
String reply;
ai.chat("Introduce yourself in one sentence", reply);  // non-streaming
ai.onDelta([](const char* d, size_t n) { Serial.write(d, n); });
ai.chatStream("Tell me a story");                       // streaming
```

Built on top of [StreamHTTPClient](https://github.com/ioqite/StreamHTTPClient) (HTTP/HTTPS transport) and [ArduinoJson v7](https://arduinojson.org/) (JSON build/parse).

---

## Table of Contents

- [Features](#features)
- [Vendor API Specification Table](#vendor-api-specification-table)
- [Supported Platforms](#supported-platforms)
- [Installation](#installation)
- [Quick Start](#quick-start)
- [API Reference](#api-reference)
- [Two Ways to Read a Streaming Response](#two-ways-to-read-a-streaming-response)
- [Error Codes](#error-codes)
- [Compile-time Configuration Macros](#compile-time-configuration-macros)
- [Memory Guidelines](#memory-guidelines)
- [Roadmap](#roadmap)
- [Known Limitations](#known-limitations)
- [Acknowledgements](#acknowledgements)

---

## Features

| Feature | Details |
| --- | --- |
| One-call vendor onboarding | `begin(vendorPreset, apiKey)` — 11 built-in presets (CN + global endpoints) |
| Any compatible endpoint | `beginCustom(host, port, path, https, key)` for self-hosted gateways / vLLM / Ollama / relays |
| Non-streaming chat | `chat()` — single message, multi-turn history, full result (content / reasoning / finish_reason / token usage) |
| Streaming mode A | `chatStream()` + `onDelta()` callback for incremental text (plus `onReasoningDelta()` for chain-of-thought) |
| Streaming mode B | `chatStreamTo()` + `AIKitStreamResponse` (an Arduino `Stream` subclass) — read raw SSE lines one by one |
| Common parameters | system prompt / temperature / top_p / max_tokens / max_completion_tokens |
| Custom HTTP headers | `addHeader(name, value, replace)` — override to a single line, or append as duplicate lines |
| Custom JSON fields | `addJsonField(key, jsonValue, replace)` — override or append into the final request JSON (pass through `tools` etc.) |
| TLS | custom root CA (`setCACert`) + `insecure` switch |
| Log routing | output target given at construction (`AIKitClient ai(&Serial0)`), any `Print`; four levels + full silence |
| Robustness | no exceptions, no RTTI, no crash paths; argument validation, graceful OOM/overflow degradation, cross-core critical sections |

---

## Vendor API Specification Table

All endpoints below were verified against the **official documentation** of each provider (checked 2026-10), with documentation links attached. Providers iterate quickly — if anything differs from what you see, trust the official docs (both endpoints and models can be overridden via `setModel()` / `beginCustom()`).

### Summary

| Preset | Provider | Base URL | Chat Completions path | Default model | Auth |
| --- | --- | --- | --- | --- | --- |
| `AIKIT_DEEPSEEK` | DeepSeek | `https://api.deepseek.com` | `/chat/completions` | `deepseek-flash` | Bearer |
| `AIKIT_MOONSHOT` | Moonshot AI (Kimi) | `https://api.moonshot.cn` | `/v1/chat/completions` | `kimi-k3` | Bearer |
| `AIKIT_MOONSHOT_INTL` | Moonshot (global) | `https://api.moonshot.ai` | `/v1/chat/completions` | `kimi-k3` | Bearer |
| `AIKIT_QWEN_DASHSCOPE` | Alibaba Bailian (Qwen) | `https://dashscope.aliyuncs.com/compatible-mode` | `/v1/chat/completions` | `qwen-plus` | Bearer |
| `AIKIT_QWEN_DASHSCOPE_INTL` | Bailian (global) | `https://dashscope-intl.aliyuncs.com/compatible-mode` | `/v1/chat/completions` | `qwen-plus` | Bearer |
| `AIKIT_ZHIPU` | Zhipu GLM (bigmodel.cn) | `https://open.bigmodel.cn/api` | `/paas/v4/chat/completions` | `glm-4.7-flash` | Bearer |
| `AIKIT_ZHIPU_ZAI` | Z.ai (Zhipu global) | `https://api.z.ai/api` | `/paas/v4/chat/completions` | `glm-4.7-flash` | Bearer |
| `AIKIT_SILICONFLOW` | SiliconFlow | `https://api.siliconflow.cn` | `/v1/chat/completions` | `deepseek-ai/DeepSeek-V4-Flash` | Bearer |
| `AIKIT_MINIMAX` | MiniMax (CN) | `https://api.minimax.cn` | `/v1/text/chatcompletion_v2` | `MiniMax-M2.7` | Bearer |
| `AIKIT_MINIMAX_INTL` | MiniMax (global) | `https://api.minimaxi.com` | `/v1/text/chatcompletion_v2` | `MiniMax-M2.7` | Bearer |
| `AIKIT_OPENAI` | OpenAI | `https://api.openai.com` | `/v1/chat/completions` | `gpt-5-mini` | Bearer |

All presets are HTTPS (443) + `Authorization: Bearer <key>`; their SSE streaming formats are mutually compatible (`data: {json}` lines + a `data: [DONE]` sentinel).

### Per-vendor notes (checked against official docs)

**DeepSeek** ([docs](https://api-docs.deepseek.com/))
- Base URL has **no `/v1`** (the officially recommended form; the legacy `/v1` prefix still works).
- Models: `deepseek-flash` (recommended default; legacy names such as `deepseek-v4-flash` still accepted), `deepseek-v4-pro`.
- `temperature` range 0~2; both streaming and non-streaming supported.

**Moonshot AI / Kimi** ([docs](https://platform.moonshot.cn/docs/api/chat))
- CN: `api.moonshot.cn`; global: `api.moonshot.ai`. Keys are not interchangeable between the two sites.
- Models: `kimi-k3` (flagship), `kimi-k2.6`, `kimi-k2.7-code`.
- ⚠️ `max_tokens` is **deprecated** — use `setMaxCompletionTokens()` (writes `max_completion_tokens`).

**Alibaba Bailian / Qwen** ([docs](https://help.aliyun.com/zh/model-studio/developer-reference/compatibility-of-openai-with-dashscope))
- The full OpenAI-compatible prefix is `/compatible-mode/v1` — do not omit it.
- Alibaba is migrating to `https://{WorkspaceId}.cn-beijing.maas.aliyuncs.com/compatible-mode/v1` (attach via `beginCustom()`); the current domain remains available.
- Models: `qwen-plus` (balanced default), `qwen-max`, `qwen-turbo`, `qwen-long`, `qwen3.8-max`, etc.
- Global site (Singapore/Virginia) uses the `dashscope-intl` preset; CN and global keys differ.

**Zhipu GLM** ([docs](https://docs.bigmodel.cn/))
- Distinctive path prefix: `/api/paas/v4/chat/completions`.
- Models: GLM-5.3 / GLM-4.7 / GLM-4.6 families; **`*-flash` models are free** (e.g. `glm-4.7-flash`, the default).
- Thinking mode: `addJsonField("thinking", "{\"type\":\"enabled\"}")`.
- Global site is Z.ai (`https://api.z.ai/api/paas/v4/chat/completions`).

**SiliconFlow** ([docs](https://docs.siliconflow.cn/cn/api-reference/chat-completions/chat-completions))
- Aggregator platform with 200+ models; **model names must carry the `vendor/` prefix** (e.g. `deepseek-ai/DeepSeek-V4-Flash`, `Qwen/Qwen3-8B`, `moonshotai/Kimi-K2.7-Code`).
- Free credits on signup — handy for development.

**MiniMax** ([docs](https://platform.minimaxi.com/docs))
- ⚠️ Path is `/v1/text/chatcompletion_v2`, **not** `/v1/chat/completions` (the preset handles this).
- CN domain migrated to `api.minimax.cn` (legacy `api.minimax.chat` still works); global: `api.minimaxi.com`.
- Models: `MiniMax-M2.7` (default), `MiniMax-M2.5`, `MiniMax-M3`, `MiniMax-Text-01`, etc.
- ⚠️ `max_tokens` is deprecated — use `setMaxCompletionTokens()`.

**OpenAI** ([docs](https://platform.openai.com/docs/api-reference/chat))
- Models: `gpt-5-mini` (default), GPT-5.6 family (current flagship), etc.
- ⚠️ o-series reasoning models **only accept** `max_completion_tokens` and require `temperature` fixed at 1 — override via custom JSON fields.
- Chat Completions remains a stable, long-term API.

> Any provider not listed that exposes an OpenAI-compatible endpoint (Groq / OpenRouter / Together / Mistral / vLLM / Ollama / OneAPI …) can be attached with a single `beginCustom()` call.

---

## Supported Platforms

Chip support and execution model follow [StreamHTTPClient](https://github.com/ioqite/StreamHTTPClient) (the transport layer); this library is verified on top of it:

| Platform | Status | Notes |
| --- | --- | --- |
| **ESP32** (classic / S2 / S3 / C3 / C6… Arduino-ESP32 core) | ✅ Full support (**primary platform**) | Line-mode streaming is driven by a FreeRTOS background task — `readLine()` is truly incremental; compile-verified for esp32 / esp32s3 / esp32c3 |
| **ESP8266** (Arduino-ESP8266 core) | ✅ Full support (mind your RAM) | TLS via BearSSL, handshake needs ~25 KB free heap; line mode is "execute synchronously + queue" (bounded) — prefer the onDelta callback for long replies; compile-verified for nodemcuv2 |
| Raspberry Pi Pico W / SAMD (as supported by StreamHTTPClient) | ⚠️ Architecturally compatible, unverified | Uses the single-context code path; feedback welcome |

---

## Installation

This library depends on two libraries — install them first:

1. **ArduinoJson v7** (available in the Library Manager)
2. **StreamHTTPClient** (not yet in the Library Manager — install from GitHub):
   - download the [zip](https://github.com/ioqite/StreamHTTPClient/archive/refs/heads/master.zip), then Arduino IDE: *Sketch → Include Library → Add .ZIP Library*;
   - or `git clone https://github.com/ioqite/StreamHTTPClient.git ~/Arduino/libraries/StreamHTTPClient`

Then install AIKit-Arduino itself the same way. arduino-cli users:

```bash
arduino-cli lib install ArduinoJson
git clone https://github.com/ioqite/StreamHTTPClient.git ~/Arduino/libraries/StreamHTTPClient
git clone https://github.com/ioqite/AIKit-Arduino.git ~/Arduino/libraries/AIKit-Arduino
```

---

## Quick Start

```cpp
#include <AIKit.h>

AIKitClient ai(&Serial);              // logs → Serial; pass nullptr for silence

void setup() {
  Serial.begin(115200);
  WiFi.begin("ssid", "password");
  while (WiFi.status() != WL_CONNECTED) delay(300);

  ai.begin(AIKIT_DEEPSEEK, "sk-xxx"); // ① one call onboarding

  // ② common parameters (optional)
  ai.setSystemPrompt("You are a rigorous embedded assistant");
  ai.setTemperature(0.7f);
  ai.setMaxTokens(1024);

  // ③ non-streaming
  String reply;
  if (ai.chat("Hello", reply) == AIKIT_OK) {
    Serial.println(reply);
  }
}
void loop() {}
```

Streaming (typewriter effect):

```cpp
ai.onDelta([](const char* delta, size_t len) {
  Serial.write((const uint8_t*)delta, len);
});
ai.chatStream("Count from 1 to 10");
```

See `examples/` for the full set (**organized by feature**, 14 examples): basic chat / stream callback / system+params / multi-turn / line reader / vendor switching / custom endpoint / custom headers / custom JSON fields / TLS cert / reasoning chain / error handling / logging control / full-featured.

---

## API Reference

### Lifecycle (begin + setter style)

| Method | Description |
| --- | --- |
| `AIKitClient(Print* log = nullptr)` | log target at construction (any `Print`/`Stream`) |
| `bool begin(AIKitVendor vendor, const char* apiKey)` | onboard a built-in vendor; callable repeatedly to switch (model resets to the new vendor default) |
| `bool beginCustom(host, port, path, https, apiKey, defaultModel)` | attach any OpenAI-compatible endpoint |
| `void end()` | finish the current request/stream and free the connection (config kept) |
| `bool isReady()` | ready to make requests? |

### Request parameter setters

| Method | Description |
| --- | --- |
| `setModel(m)` | model name (see spec table) |
| `setSystemPrompt(s)` / `clearSystemPrompt()` | system message (auto-prepended; skipped if your message array already starts with system) |
| `setTemperature(t)` | 0~2 |
| `setTopP(p)` | 0~1 |
| `setMaxTokens(v)` | writes `max_tokens` |
| `setMaxCompletionTokens(v)` | writes `max_completion_tokens` (Moonshot/OpenAI current spec) |
| `setTimeouts(connectMs, tcpMs)` | connect/read timeouts (default 10s/30s, reasoning-model friendly) |

### TLS

| Method | Description |
| --- | --- |
| `setInsecure(bool)` | skip-certificate switch (**default true** for out-of-box use on cert-less setups; turn it off in production) |
| `setCACert(pem)` | custom root CA (PEM text; on ESP8266 use `setCACert_P(PSTR(...))` for PROGMEM strings) |

### Custom HTTP headers

| Method | Description |
| --- | --- |
| `addHeader(name, value, replace=true)` | `replace=true`: override same-name header to a single line (can override the library's `Content-Type`/`Accept`); `replace=false`: append as duplicate lines (multi-value headers) |
| `removeHeader(name)` / `clearHeaders()` | removal |

### Custom JSON fields

| Method | Description |
| --- | --- |
| `addJsonField(key, jsonValue, replace=true)` | value must be valid JSON text (number / quoted string / object / array / bool); `replace=true` overrides a field generated by the library, otherwise only appends missing fields. Validated at set time |
| `addJsonField(key, float/int32_t/bool, ...)` | numeric/bool convenience overloads |
| `addJsonFieldString(key, str, ...)` | plain-text convenience (auto-quotes and escapes) |
| `removeJsonField(key)` / `clearJsonFields()` | removal |

> This is the escape hatch: any parameter without a dedicated setter (`tools`, `response_format`, `thinking`, vendor thinking switches…) can be passed through — see example `09_CustomJsonFields`.

### Chat

| Method | Description |
| --- | --- |
| `int chat(userText, String& outContent)` | single message → reply text |
| `int chat(userText, AIKitChatResult& result)` | single message → full result (`content` / `reasoningContent` / `finishReason` / `usage`) |
| `int chat(messages, count, ...)` | multi-turn / custom roles |
| `int chatStream(userText)` / `chatStream(messages, count)` | streaming (onDelta callback mode) |
| `int chatStreamTo(userText, AIKitStreamResponse&)` | streaming (line-reader mode) |
| `onDelta(cb)` / `onReasoningDelta(cb)` | incremental callbacks `void(const char* delta, size_t len)` |

### State queries

| Method | Description |
| --- | --- |
| `lastStatusCode()` | last HTTP status (0 = none received) |
| `lastTransportError()` | underlying StreamHTTPClient error code (pair with `StreamHTTPClient::errorToString()`) |
| `lastErrorBody()` | response body kept on non-2xx (≤384 bytes by default) |
| `lastFinishReason()` / `lastUsage()` | recorded for both streaming and non-streaming |
| `streamActive()` | is a stream in flight? (new chat* calls return `AIKIT_ERR_BUSY` meanwhile) |

---

## Two Ways to Read a Streaming Response

### Mode A: onDelta callback

```cpp
ai.onDelta([](const char* d, size_t n) { Serial.write((const uint8_t*)d, n); });
ai.chatStream("Tell me a story");   // incremental callbacks during the call; return = stream finished
```

Behavior is identical on both platforms (callbacks fire while the request executes) — **the recommended way to handle long replies on ESP8266**.

### Mode B: Stream line reading (AIKitStreamResponse)

```cpp
AIKitStreamResponse resp;
int err = ai.chatStreamTo("Tell me a story", resp);
String line, piece;
while (resp.readLine(line)) {                        // blocks until a new line / end
  if (AIKitStreamResponse::isDoneSentinel(line)) break;
  if (AIKitStreamResponse::extractDelta(line, piece)) Serial.print(piece);
  // `line` is the raw SSE line ("data: {...}") — parse it yourself with ArduinoJson if needed
}
resp.close();
```

`AIKitStreamResponse` derives from the Arduino `Stream` class, so `available()/read()/peek()/readStringUntil()` all work.

**Platform differences** (due to StreamHTTPClient's execution model):

| | ESP32 | ESP8266 etc. |
| --- | --- | --- |
| Request execution | FreeRTOS background task; `chatStreamTo()` returns immediately | Executes synchronously inside `chatStreamTo()` |
| Reading experience | `readLine()` is truly incremental (ring buffer + backpressure) | Lines are queued first (bounded by `AIKIT_LINE_QUEUE_MAX_LINES` / `MAX_BYTES`), then read out |
| Long replies | unlimited (within RAM) | use Mode A; or raise the caps via build macros |

Both modes expose the same raw SSE lines and HTTP status/error information; exceeding the caps never crashes — it sets `AIKIT_ERR_BUFFER_OVERFLOW`, queryable via `resp.lastError()`.

---

## Error Codes

`chat*` functions return `AIKIT_OK(0)` or a negative error code; non-2xx HTTP is uniformly mapped to `AIKIT_ERR_HTTP_STATUS`, with the exact status and body available via `lastStatusCode()` / `lastErrorBody()`.

| Code | Value | Meaning / triage |
| --- | --- | --- |
| `AIKIT_OK` | 0 | success |
| `AIKIT_ERR_CONFIG` | -101 | `begin()` not called / key missing |
| `AIKIT_ERR_NULL_ARG` | -102 | null pointer argument |
| `AIKIT_ERR_TOO_MANY` | -103 | custom headers/fields over capacity (see macros) |
| `AIKIT_ERR_BAD_JSON` | -104 | invalid custom JSON value (reported at set time) |
| `AIKIT_ERR_CONNECT` | -105 | connection/send failed → check `lastTransportError()` |
| `AIKIT_ERR_HTTP_STATUS` | -106 | non-2xx HTTP → check `lastStatusCode()` + `lastErrorBody()` |
| `AIKIT_ERR_PARSE` | -107 | response JSON parse failed |
| `AIKIT_ERR_OOM` | -108 | out of memory (JSON build / task creation) |
| `AIKIT_ERR_TIMEOUT` | -109 | timeout |
| `AIKIT_ERR_BUSY` | -110 | a request/stream is active (call `resp.close()` or `end()` first) |
| `AIKIT_ERR_LINE_TOO_LONG` | -111 | SSE line exceeded `AIKIT_SSE_LINE_MAX` (line dropped, stream continues) |
| `AIKIT_ERR_BUFFER_OVERFLOW` | -112 | line buffer over the limit (see platform differences) |
| `AIKIT_ERR_STREAM_CLOSED` | -113 | stream already closed |
| `AIKIT_ERR_NOT_SUPPORTED` | -114 | unsupported on this platform/config |

`aikitErrorToString(err)` returns a readable description.

---

## Compile-time Configuration Macros

Define them (`-D`) before including `AIKit.h` to override:

| Macro | Default | Description |
| --- | --- | --- |
| `AIKIT_MAX_CUSTOM_HEADERS` | 8 | custom header capacity |
| `AIKIT_MAX_JSON_FIELDS` | 12 | custom JSON field capacity |
| `AIKIT_SSE_LINE_MAX` | 2048 | SSE line cap (oversized lines dropped & resynchronized) |
| `AIKIT_LINE_RING_SLOTS` | 16 | [ESP32] line ring buffer slots |
| `AIKIT_LINE_PUSH_TIMEOUT_MS` | 30000 | [ESP32] producer wait when buffer full |
| `AIKIT_LINE_QUEUE_MAX_LINES` | 96 | [non-ESP32] line queue line cap |
| `AIKIT_LINE_QUEUE_MAX_BYTES` | 32768 | [non-ESP32] line queue byte cap |
| `AIKIT_ERRBODY_MAX` | 384 | error body retention length |
| `AIKIT_LOG_BUF` | 160 | single log line cap |
| `AIKIT_DEFAULT_CONNECT_TIMEOUT_MS` | 10000 | default connect timeout |
| `AIKIT_DEFAULT_TCP_TIMEOUT_MS` | 30000 | default read timeout |

---

## Memory Guidelines

- **ESP32**: non-streaming responses are fully buffered (by StreamHTTPClient); replies of a few thousand tokens are typically tens of KB — no concern. Line-mode streaming peak ≈ slots × line length (default 16 × ~1 KB).
- **ESP8266**: ~40 KB user heap; BearSSL handshake needs ~25 KB free. Recommendations:
  - use the **onDelta callback** for long replies (process while receiving, nothing stays resident);
  - cap reply length with `setMaxTokens()`;
  - watch the `AIKIT_LINE_QUEUE_*` caps in line mode;
  - avoid coexisting with libraries that hold large buffers.
- All error paths degrade gracefully with error codes — no OOM crashes or reboots.

---

## Roadmap

The following capabilities have their **slots reserved** and are planned by priority (raise/vote in Issues):

- [ ] **First-class Tool Calls API**: request building + streaming `tool_calls` delta aggregation (meanwhile pass `tools` through via `addJsonField()` — see example 09)
- [ ] **Vision**: convenience builders for `content` arrays (image_url)
- [ ] **Embeddings**: `/v1/embeddings` endpoint
- [ ] **Model listing**: fetch & parse `/v1/models`
- [ ] **Usage accounting**: cross-request token/cost accumulation
- [ ] **Automatic retry & rate limiting**: configurable backoff (see example 12 for a manual version today)
- [ ] **Pico W / SAMD verification** and official support statement
- [ ] **Keep-alive connection pool** (concurrent multi-vendor instances)

Architectural reservations: `AIKitMessage` keeps lightweight pointer semantics so multimodal `content` can be attached later; the custom JSON channel is the universal passthrough for every feature not yet landed; `AIKIT_ERR_NOT_SUPPORTED` is the reserved return value for future interface stubs.

---

## Known Limitations

- `insecure=true` by default (TLS certificate validation skipped) — a usability trade-off for environments without root certs/NTP. **For production, call `setCACert()` and `setInsecure(false)`** (see example 10).
- Non-streaming responses are fully loaded into memory before parsing (use streaming for very long replies).
- Conversation history is not managed by the library (a memory decision) — you own the message array.
- Line-mode streaming on ESP8266 is buffered (see platform differences above).
- The API key travels in clear text within requests — do not publish DEBUG logs containing your key.

---

## Acknowledgements

- [StreamHTTPClient](https://github.com/ioqite/StreamHTTPClient) — HTTP/HTTPS transport (sync/async/SSE)
- [ArduinoJson](https://arduinojson.org/) — JSON build & parse (v7)
- The OpenAI-compatible open interfaces provided by all the vendors
