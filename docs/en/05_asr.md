# 05 - Speech Recognition (ASR)

English | **[简体中文](../05_asr.md)**

As of v1.2.0, AIKit-Arduino provides two **cloud** speech-to-text capabilities (explicitly not on-device recognition — audio is uploaded to the vendor API over the network):

| Capability | Class | Channel | Best for |
| --- | --- | --- | --- |
| **One-shot transcription** | `AIKitASRClient` | HTTP (multipart / JSON upload) | A short, already captured clip (seconds to tens of seconds) |
| **Real-time recognition** | `AIKitRealtimeASR` | Bidirectional WebSocket stream | Text while you speak (walkie-talkie / voice assistant / live captions) |

---

## Vendor support matrix

A frequently asked question is whether the recently released Chinese ASR models can be used on Arduino. The table below was verified vendor by vendor (2026-10, checked against each official doc) — **every vendor with an API is connected; those offering only open weights and no cloud service are marked accordingly**:

| Vendor / model | One-shot transcription | Real-time (WebSocket) | Notes |
| --- | --- | --- | --- |
| **Seed-ASR / Doubao** (ByteDance Volcengine) | — | ✅ `AIKIT_RT_VOLC` | `sauc` large-model streaming edition; binary protocol; responses auto-gunzipped |
| **SenseVoice** (Alibaba Tongyi family) | ✅ `AIKIT_ASR_DASHSCOPE` (`fun-asr-flash`, of the SenseVoice lineage) | — (use paraformer real-time, see below) | DashScope synchronous multimodal-generation API; Bailian has **no** dedicated SenseVoice endpoint — fun-asr-flash is its same-lineage successor |
| **Qwen-ASR** (Alibaba Qwen) | ✅ via `setModel("qwen3-asr-flash")` | — | Same API as fun-asr-flash; the newer `qwen3-asr-flash-realtime` uses a conversational protocol that is unfriendly to MCUs, so it is not integrated for now |
| **Alibaba real-time recognition** (paraformer-realtime-v2) | — | ✅ `AIKIT_RT_DASHSCOPE` | run-task full-duplex inference protocol, covering the Tongyi family's real-time scenarios |
| **FireRedASR** (Xiaohongshu) | ❌ | ❌ | **Open model weights only** (GitHub FireRedTeam/FireRedASR), no official cloud API; self-host it and connect via `beginCustom()` / the FunASR channel |
| **Kimi-Audio** (Moonshot AI) | ❌ | ❌ | The official platform explicitly **does not provide an ASR API yet** (per their FAQ); open weights only |
| **MiMo-Audio / MiMo-V2.5-ASR** (Xiaomi) | ✅ `AIKIT_ASR_MIMO` | — | Uses chat-completions + base64 audio; the open-source MiMo-Audio-7B has no cloud API |
| **Fun-ASR** (FunASR) | ✅ self-hosted (OpenAI-compatible service) | ✅ `AIKIT_RT_FUNASR` (self-hosted 2pass) | Self-hosted = your own server, no official managed API; real-time via the runtime WebSocket |
| **StepAudio** (StepFun) | ✅ `AIKIT_ASR_STEPFUN` | ✅ `AIKIT_RT_STEPFUN` | HTTP via JSON+SSE; the real-time protocol is isomorphic to OpenAI's |
| **Baidu Speech** (real-time) | — | ✅ `AIKIT_RT_BAIDU` | START/MID_TEXT/FIN_TEXT protocol, appid+appkey sent directly |
| OpenAI | ✅ `AIKIT_ASR_OPENAI` | ✅ `AIKIT_RT_OPENAI` | Real-time via Realtime Transcription (the library automatically resamples 16k→24k) |
| Groq | ✅ `AIKIT_ASR_GROQ` | — | whisper-large-v3(-turbo), OpenAI-compatible multipart |
| Mistral | ✅ `AIKIT_ASR_MISTRAL` | ✅ `AIKIT_RT_MISTRAL` (experimental) | The client-upstream events of the real-time protocol are unverified on real hardware — marked experimental |
| Zhipu GLM-ASR | ✅ `AIKIT_ASR_ZHIPU` | — | glm-asr-2512, ≤30 seconds per clip (wav/mp3) |

> If the vendor you want ships only open weights (FireRedASR / Kimi-Audio), deploy it on a GPU machine and it lands on the same API surface via the "FunASR self-hosted channel" or `beginCustom()`.

---

## One-shot transcription (AIKitASRClient)

### Basic usage

```cpp
AIKitASRClient asr(&Serial);
asr.begin(AIKIT_ASR_DASHSCOPE, "sk-...");   // 8 presets, see the table below
asr.setLanguage("zh");                       // optional

// Raw PCM16 → WAV header wrapped automatically on upload → text returned
String text;
int err = asr.transcribePCM(pcm, pcmLen, /*sampleRate=*/16000, /*channels=*/1, text);

// Or upload ready-made wav / mp3 file bytes
asr.transcribe(wavBytes, wavLen, "clip.wav", "audio/wav", text);
```

### Vendor preset table

| Preset | Vendor | Protocol style | Default model | Notes |
| --- | --- | --- | --- | --- |
| `AIKIT_ASR_OPENAI` | OpenAI | multipart | `gpt-transcribe` | ≤25MB; whisper-1 also works |
| `AIKIT_ASR_GROQ` | Groq | multipart | `whisper-large-v3-turbo` | Fully OpenAI-compatible |
| `AIKIT_ASR_MISTRAL` | Mistral | multipart (`x-api-key`) | `voxtral-mini-latest` | Audio up to 3 hours |
| `AIKIT_ASR_ZHIPU` | Zhipu | multipart | `glm-asr-2512` | **≤30 seconds per clip**, wav/mp3 |
| `AIKIT_ASR_DASHSCOPE` | Alibaba Bailian | JSON + base64 | `fun-asr-flash` | `setModel("qwen3-asr-flash")` switches to Qwen-ASR; ≤5 min |
| `AIKIT_ASR_STEPFUN` | StepFun | JSON + SSE | `stepaudio-3-asr-max` | JSON body (not multipart) |
| `AIKIT_ASR_MIMO` | Xiaomi | chat + base64 | `mimo-v2.5-asr` | mp3 / wav only |
| `AIKIT_ASR_FUNASR` | FunASR self-hosted | multipart | `sensevoice` | `asr.beginFunASR("192.168.1.10")`, no auth |

### Memory constraints (read this)

The request body is **assembled whole** on the heap (multipart or base64 JSON), capped by `AIKIT_ASR_MAX_UPLOAD_BYTES` (default 64 KB):

- **ESP32**: tens of KB of audio are no problem (even more headroom with PSRAM);
- **ESP8266**: keep audio ≤12 KB (about 6 seconds of 16 kHz PCM16), or lower the macro at compile time;
- Exceeding the cap returns `AIKIT_ERR_PAYLOAD_TOO_BIG` — no OOM crash.

### Common methods

| Method | Description |
| --- | --- |
| `setModel(m)` | Switch models (e.g. qwen3-asr-flash / whisper-1) |
| `setLanguage("zh")` | Language hint (the field is not sent when unset) |
| `setPrompt(p)` | Context prompt (supported by OpenAI / Zhipu) |
| `setInsecure(bool)` / `setCACert(pem)` | TLS (insecure=true by default; configure a CA for production) |
| `setTimeouts(connectMs, tcpMs)` | Transcription is slow overall — the default read timeout is 90 s |
| `lastStatusCode()` / `lastErrorBody()` | Troubleshooting |

---

## Real-time recognition (AIKitRealtimeASR)

### ⚠ Optional dependency

Requires [ArduinoWebsockets](https://github.com/gilmaimon/ArduinoWebsockets) (by gilmaimon, v0.5+):

```bash
arduino-cli lib install "ArduinoWebsockets"
```

When not installed, all methods of this class return `AIKIT_ERR_NOT_SUPPORTED`; the rest of the library is unaffected.

### Basic usage

```cpp
AIKitRealtimeASR rt(&Serial);
rt.begin(AIKIT_RT_DASHSCOPE, "sk-...");     // see the table below; Volcengine/Baidu use dedicated begin
rt.setLanguage("zh");
rt.onPartial([](const char* t) { /* text of this sentence so far (may change) */ });
rt.onFinal([](const char* t) { /* a finalized sentence, no longer changes */ });
rt.onError([](int e, const char* m) { /* errors */ });

rt.connect();                               // blocking: handshake + start instruction
// Main loop:
rt.sendAudio(pcm, 3200);                    // 16k/16bit/mono, one 100 ms frame (3200 B)
rt.loop();                                  // must be called frequently (receiving + keep-alive)
rt.finish(8000);                            // end signal + wait for the final result + close
```

### Vendor preset table

| Preset | begin method | Endpoint (protocol) | Notes |
| --- | --- | --- | --- |
| `AIKIT_RT_VOLC` | `beginVolc(appId, accessToken)` or `begin(..., key1, key2)` | `wss://openspeech.bytedance.com/api/v3/sauc/bigmodel` (binary frames + gzip responses) | Resource-Id defaults to `volc.bigasr.sauc.duration`; no heartbeat — the library pings automatically during silence |
| `AIKIT_RT_DASHSCOPE` | `begin(v, apiKey)` | `wss://dashscope.aliyuncs.com/api-ws/v1/inference` (run-task full duplex) | Default `paraformer-realtime-v2`; `heartbeat:true` keep-alive |
| `AIKIT_RT_BAIDU` | `beginBaidu(appId, appKey)` | `wss://vop.baidu.com/realtime_asr?sn=...` | `setBaiduDevPid()` selects the language (default 15372, Mandarin with enhanced punctuation); the connection drops if no audio is sent for 5 seconds — the library sends HEARTBEAT automatically when idle |
| `AIKIT_RT_FUNASR` | `beginFunASR(host, port)` | `ws://host:10095` (self-hosted) | 2pass mode (streaming intermediate results + offline-corrected final at sentence end); no auth |
| `AIKIT_RT_OPENAI` | `begin(v, apiKey)` | `wss://api.openai.com/v1/realtime?intent=transcription` | Default `gpt-live-transcribe`; **audio is 24 kHz** — the library resamples 16k→24k linearly + base64 |
| `AIKIT_RT_STEPFUN` | `begin(v, apiKey)` | `wss://api.stepfun.com/v1/realtime/asr/stream` | Default `stepaudio-2.5-asr-stream`; the event stream is isomorphic to OpenAI's |
| `AIKIT_RT_MISTRAL` | `begin(v, apiKey)` | `wss://api.mistral.ai/v1/audio/transcriptions/realtime?model=...` | **Experimental**: the upstream event protocol is implemented isomorphic to OpenAI's and is unverified on real hardware |

### Callback semantics (uniform across vendors)

| Callback | Semantics |
| --- | --- |
| `onPartial(text)` | **The complete text of the current sentence so far** (recognition in progress; later corrections possible). Incremental-style vendors (FunASR online / OpenAI / Mistral) are accumulated internally before the callback, so the semantics match the full-text vendors |
| `onFinal(text)` | A finalized sentence (sentence-level); the text never changes again; fires once per sentence |
| `onError(err, msg)` | Protocol / network / server rejection; the connection is usually already closed by then |
| `onConnected()` / `onClosed()` | Ready (audio can be sent) / connection closed (including the normal `finish` shutdown) |

### API summary

| Method | Description |
| --- | --- |
| `connect()` | Opens the connection + sends the start instruction (DashScope/OpenAI block until the ready confirmation) |
| `sendAudio(pcm, len)` | Sends one frame of 16k/16bit/mono PCM; 100–200 ms per frame (3200–6400 B) recommended |
| `commitTurn()` | Manually commits a turn (manual-turn mode of OpenAI / StepFun / Mistral only) |
| `finish(waitMs)` | Sends the end signal → blocks for the final result → closes the connection; returns `AIKIT_OK` / `AIKIT_ERR_TIMEOUT` |
| `loop()` | **Must be called frequently from the main loop**: drives receiving and idle keep-alive |
| `close()` | Closes immediately (does not wait for results) |
| `setModel / setLanguage / setBaiduDevPid` | Vendor parameters |
| `setInsecure / setCACert` | TLS (insecure=true by default) |

### Audio capture suggestions

- **ESP32**: an I2S digital microphone (INMP441 / MSM261, about ¥10); full wiring and driver code in [examples/19](../../examples/19_ASR_Realtime/19_ASR_Realtime.ino);
- **ESP8266**: no convenient I2S option — ADC + software sampling or an external codec is required; **for real-time recognition, use an ESP32** (the BearSSL TLS handshake alone needs ~22 KB of heap);
- All real-time vendors uniformly require **16 kHz / 16 bit / mono PCM** (OpenAI's 24 kHz is handled by the library's internal resampling).

### Per-vendor protocol details (what the implementation was verified against)

- **Volcengine**: 4-byte binary header (`0x11 0x11/0x21/0x23 …`) + big-endian seq/size + payload; the first packet is a full client request (JSON), audio frames are audio-only, the last packet carries a negative seq; responses may be gzip (the library reuses StreamHTTPClient's built-in miniz decompression); error frames have message_type=0xF.
- **DashScope**: `run-task` (`header.action`) → `task-started` → binary audio frames → `result-generated` (`payload.output.sentence`, `sentence_end` distinguishes intermediate/final) → `finish-task` → `task-finished`; `heartbeat:true` enables keep-alive empty packets.
- **Baidu**: `START` (appid/appkey/dev_pid/cuid/format/sample) → raw PCM binary frames → `MID_TEXT` (intermediate) / `FIN_TEXT` (final or error, see `err_no`) → `FINISH`; the server sends `HEARTBEAT` every ~5 s.
- **FunASR**: config JSON (`mode:"2pass"`, `chunk_size:[5,10,5]`…) → raw PCM → `{"is_speaking":false}` to finish; the response `mode` distinguishes `2pass-online` (incremental) / `2pass-offline` (final).
- **OpenAI**: `session.created` → `session.update` (nested GA structure) → `input_audio_buffer.append` (base64 PCM24k) → `commit` → `...transcription.delta / .completed`; no application-layer heartbeat — it relies on protocol-level pings.
