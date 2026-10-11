# 07 - Advanced: Error Codes, Memory, CI & Roadmap

English | **[简体中文](../07_advanced.md)**

## Complete error-code table

Calls such as `chat*` / `append*` return `AIKIT_OK(0)` or a negative `AIKIT_ERR_*`; non-2xx HTTP is uniformly mapped to `AIKIT_ERR_HTTP_STATUS`, with the concrete status code and body available via `lastStatusCode()` / `lastErrorBody()`. The value range (starting at -101) is **deliberately disjoint** from the underlying StreamHTTPClient's `SHC_ERROR_*` (-1 ~ -20), so the two are distinguishable at a glance in logs; the underlying error is preserved verbatim in `lastTransportError()`. `aikitErrorToString(err)` returns a readable description (a static string, no allocation).

| Error code | Value | Meaning / where to look |
| --- | --- | --- |
| `AIKIT_OK` | 0 | Success |
| `AIKIT_ERR_CONFIG` | -101 | No `begin()` / missing API key / endpoint not configured |
| `AIKIT_ERR_NULL_ARG` | -102 | Null pointer passed (or degenerate arguments such as an empty role) |
| `AIKIT_ERR_TOO_MANY` | -103 | Custom headers / JSON fields over capacity (see the macro table in [01_installation.md](01_installation.md)) |
| `AIKIT_ERR_BAD_JSON` | -104 | A custom JSON field value is not valid JSON (reported at set time) |
| `AIKIT_ERR_CONNECT` | -105 | Connection / send failure → check `lastTransportError()` |
| `AIKIT_ERR_HTTP_STATUS` | -106 | HTTP non-2xx → check `lastStatusCode()` + `lastErrorBody()` |
| `AIKIT_ERR_PARSE` | -107 | Response JSON parsing failed or the structure mismatches |
| `AIKIT_ERR_OOM` | -108 | Out of memory (JSON building / task creation) |
| `AIKIT_ERR_TIMEOUT` | -109 | Timeout (default read timeout 30 s, see `setTimeouts`) |
| `AIKIT_ERR_BUSY` | -110 | A request / stream is already in flight → call `resp.close()` or `end()` first |
| `AIKIT_ERR_LINE_TOO_LONG` | -111 | A single SSE line exceeded `AIKIT_SSE_LINE_MAX` (that line is dropped, the stream resynchronizes and continues) |
| `AIKIT_ERR_BUFFER_OVERFLOW` | -112 | Line-reading buffer overflow (platform differences in [04_streaming.md](04_streaming.md)) |
| `AIKIT_ERR_STREAM_CLOSED` | -113 | Stream already closed |
| `AIKIT_ERR_NOT_SUPPORTED` | -114 | Not supported on this platform / configuration (e.g. real-time ASR without ArduinoWebsockets installed) |
| `AIKIT_ERR_ASR_AUDIO` | -115 | *v1.2.0* Audio data empty / format or parameters rejected by this vendor |
| `AIKIT_ERR_ASR_PROTOCOL` | -116 | *v1.2.0* ASR response protocol parsing failed (structure other than expected) |
| `AIKIT_ERR_WS` | -117 | *v1.2.0* WebSocket connection / handshake / communication failure |
| `AIKIT_ERR_NOT_CONNECTED` | -118 | *v1.2.0* WebSocket not connected (call `connect()` first) |
| `AIKIT_ERR_INFLATE` | -119 | *v1.2.0* gzip response decompression failed (Volcengine binary protocol) |
| `AIKIT_ERR_HISTORY_LIMIT` | -120 | *v1.2.0* Chat history exceeded a capacity safety valve (see [06_history.md](06_history.md)) |
| `AIKIT_ERR_PAYLOAD_TOO_BIG` | -121 | *v1.2.0* Audio / request body above the sendable cap (`AIKIT_ASR_MAX_UPLOAD_BYTES`) |

```cpp
int err = ai.chat("Hello", reply);
if (err != AIKIT_OK) {
  Serial.printf("[AIKit] %d: %s\n", err, aikitErrorToString(err));
  if (err == AIKIT_ERR_HTTP_STATUS) {
    Serial.printf("HTTP %d, body: %s\n", ai.lastStatusCode(), ai.lastErrorBody().c_str());
  }
}
```

See [examples/12_ErrorHandling](../../examples/12_ErrorHandling/12_ErrorHandling.ino) for a complete error-handling demonstration.

## Memory guidelines

| Platform | Recommendation |
| --- | --- |
| **ESP32** | Non-streaming responses are buffered in full by the transport layer — a several-thousand-token reply is a few tens of KB, usually nothing to worry about. Peak usage of line-by-line reading ≈ slots × line length (default 16 × ~1 KB); with PSRAM there is even more headroom |
| **ESP8266** | The user heap is ~40 KB, and the BearSSL TLS handshake alone takes ~25 KB of free heap. For long replies **use the onDelta callback** (emit while receiving, nothing resident in memory, see [04_streaming.md](04_streaming.md)); bound reply length with `setMaxTokens()`; watch the `AIKIT_LINE_QUEUE_*` caps in line-reading mode; avoid coexisting with large-buffer libraries |
| **ASR (audio upload)** | The request body is **assembled whole** on the heap (multipart / base64), capped by `AIKIT_ASR_MAX_UPLOAD_BYTES` (default 64 KB). **On ESP8266 keep audio ≤12 KB** (about 6 seconds of 16 kHz PCM16) or lower the macro at compile time; on ESP32 raise it as heap allows. Exceeding the cap returns `AIKIT_ERR_PAYLOAD_TOO_BIG` instead of an OOM crash |
| **Chat history** | Two safety valves: default 128 messages / 48 KB ([06_history.md](06_history.md)); monitor with `totalBytes()`, control growth with `setWindow()` |

Every error path degrades gracefully (an error code is returned); **there is no OOM-crash or reboot path**. For compile-time overrides of the related macros see [01_installation.md](01_installation.md).

## GitHub Actions CI (manual compile matrix)

This repository ships a **manual-trigger-only** compile-verification workflow (`.github/workflows/compile.yml`) — no push / PR / schedule triggers; run it on demand without burning public runner quota:

| Item | Configuration |
| --- | --- |
| Trigger | `workflow_dispatch`, **manual** (no push / PR / schedule) |
| Matrix | 4 boards: `esp32` / `esp32s3` / `esp32c3` / `nodemcuv2` (ESP8266) × **all examples** (21) |
| Dependencies | ArduinoJson + ArduinoWebsockets from the Library Manager; StreamHTTPClient cloned from GitHub |
| Artifacts | One `compile-report-<board>` report per board (PASS/FAIL and size per example); any failure fails the job |

**Usage**: repository page → *Actions* tab → select **Compile Matrix** on the left → **Run workflow** on the right → optionally tick `Also compile ESP8266 (nodemcuv2)` (ticked by default; untick to skip ESP8266 and run only the three ESP32 boards, saving time) → press the green button.

Good for: verifying all-platform compilability after changing library sources / adding examples; or verifying your own changes after forking.

## Roadmap

The following capabilities listed on the v0.1-era roadmap **have been delivered in v1.2.0** and removed from the backlog: ✅ usage accounting (`totalUsage`), ✅ automatic retry (`setRetry`), ✅ chat history management (`AIKitHistory`), ✅ ASR (HTTP transcription + real-time recognition), ✅ CI compile matrix.

Current backlog (by priority — feature requests and votes in Issues are welcome):

| Direction | Description | Interim workaround today |
| --- | --- | --- |
| **First-class Tool Calls API** | Request building + streaming `tool_calls` delta aggregation | Pass through via `addJsonField("tools", ...)`, see [examples/09_CustomJsonFields](../../examples/09_CustomJsonFields/09_CustomJsonFields.ino) |
| **Vision multimodal** | Convenience builder for `content` arrays (`image_url`) | Same as above — custom JSON field pass-through |
| **Embeddings** | `/v1/embeddings` endpoint | `beginCustom()` + issue the requests yourself |
| **Model listing** | Fetch and parse `/v1/models` | — |
| **Pico W / SAMD validation** | Architecturally compatible (follows StreamHTTPClient); produce an official support statement | Feedback with real-world test results welcome |
| **Keep-Alive connection pool** | Concurrent connection reuse across vendor instances | One connection per instance |
| **TTS speech synthesis** | Text-to-speech endpoints | — |

Architectural reservations: `AIKitMessage` keeps lightweight pointer semantics so a multimodal `content` can be attached later; the custom JSON channel is the universal pass-through for every feature not yet landed; `AIKIT_ERR_NOT_SUPPORTED` is the uniform return value reserved for future interfaces.

## Known limitations

| Limitation | Notes / mitigation |
| --- | --- |
| `insecure=true` by default | TLS certificate validation is skipped by default (a trade-off for quick starts without NTP / root certificates). **In production always `setCACert()` + `setInsecure(false)`** — see [examples/10_TLS_CustomCert](../../examples/10_TLS_CustomCert/10_TLS_CustomCert.ino) and [examples/20_CertificateImport](../../examples/20_CertificateImport/20_CertificateImport.ino) |
| Non-streaming loads fully into memory | Non-streaming responses are buffered completely before parsing (see the memory guidelines above); for very long replies switch to streaming |
| ESP8266 line reading is buffered | The request completes synchronously inside `chatStreamTo()` and lines are queued before being read (with caps); for true incremental behavior use the onDelta callback, see [04_streaming.md](04_streaming.md) |
| Mistral real-time ASR is experimental | The upstream event protocol was implemented isomorphic to OpenAI's and is unverified on real hardware (details in [05_asr.md](05_asr.md)) |
| API keys travel in cleartext | The key is plaintext inside requests — **do not publish DEBUG-level logs containing keys**; local-deployment presets (Ollama etc.) are unaffected |

---

More topics: [01 Installation & Dependencies](01_installation.md) · [02 Vendor Presets](02_vendors.md) · [03 Chat API](03_chat.md) · [04 Streaming](04_streaming.md) · [05 Speech Recognition](05_asr.md) · [06 Chat History](06_history.md)
