# 05 - 语音识别（ASR）

**[English](../en/05_asr.md)** | 简体中文

AIKit-Arduino v1.2.0 起提供两类**云端**语音转文字能力（明确不是端侧识别——音频通过网络上传到厂商 API）：

| 能力 | 类 | 通道 | 适用 |
| --- | --- | --- | --- |
| **一句话转写** | `AIKitASRClient` | HTTP（multipart / JSON 上传） | 已采集好的一小段音频（数秒~数十秒） |
| **实时识别** | `AIKitRealtimeASR` | WebSocket 双向流 | 边说边出字（对讲机 / 语音助手 / 同步字幕） |

---

## 常见厂商支持矩阵

很多同学关心国内新出的 ASR 模型能否在 Arduino 上用。下表逐家核实（2026-10，官方文档逐一比对）——**能走 API 的都接了，仅开源权重无云端服务的如实标注**：

| 厂商 / 模型 | 一句话转写 | 实时识别（WebSocket） | 说明 |
| --- | --- | --- | --- |
| **Seed-ASR / 豆包**（字节跳动·火山引擎） | — | ✅ `AIKIT_RT_VOLC` | `sauc` 大模型流式版，二进制协议，响应自动 gunzip |
| **SenseVoice**（阿里通义系） | ✅ `AIKIT_ASR_DASHSCOPE`（`fun-asr-flash`，即 SenseVoice 系） | —（走 paraformer 实时，见下） | DashScope 同步 multimodal-generation 接口；百炼**无** SenseVoice 专用端点，fun-asr-flash 为同源演进 |
| **Qwen-ASR**（阿里 Qwen） | ✅ `setModel("qwen3-asr-flash")` | — | 与 fun-asr-flash 同接口；新一代 `qwen3-asr-flash-realtime` 会话式协议对 MCU 不友好，暂未接 |
| **阿里实时识别**（paraformer-realtime-v2） | — | ✅ `AIKIT_RT_DASHSCOPE` | run-task 双工推理协议，覆盖通义系实时场景 |
| **FireRedASR**（小红书） | ❌ | ❌ | **仅开源模型权重**（GitHub FireRedTeam/FireRedASR），官方无云端 API；可自行部署后用 `beginCustom()` / FunASR 通道对接 |
| **Kimi-Audio**（月之暗面） | ❌ | ❌ | 官方平台明确**暂不提供 ASR API**（FAQ 原文）；仅开源权重 |
| **MiMo-Audio / MiMo-V2.5-ASR**（小米） | ✅ `AIKIT_ASR_MIMO` | — | 走 chat-completions + base64 音频；开源版 MiMo-Audio-7B 无云端 API |
| **Fun-ASR**（FunASR） | ✅ 自部署（OpenAI 兼容服务） | ✅ `AIKIT_RT_FUNASR`（自部署 2pass） | 自部署 = 自己的服务器，官方无托管 API；实时走 runtime WebSocket |
| **StepAudio**（阶跃星辰） | ✅ `AIKIT_ASR_STEPFUN` | ✅ `AIKIT_RT_STEPFUN` | HTTP 走 JSON+SSE；实时协议与 OpenAI 同构 |
| **百度语音**（实时） | — | ✅ `AIKIT_RT_BAIDU` | START/MID_TEXT/FIN_TEXT 协议，appid+appkey 直传 |
| OpenAI | ✅ `AIKIT_ASR_OPENAI` | ✅ `AIKIT_RT_OPENAI` | 实时为 Realtime Transcription（库内自动 16k→24k 重采样） |
| Groq | ✅ `AIKIT_ASR_GROQ` | — | whisper-large-v3(-turbo)，OpenAI 兼容 multipart |
| Mistral | ✅ `AIKIT_ASR_MISTRAL` | ✅ `AIKIT_RT_MISTRAL`（实验性） | 实时协议的客户端上行事件未经真机验证，标注实验性 |
| 智谱 GLM-ASR | ✅ `AIKIT_ASR_ZHIPU` | — | glm-asr-2512，单条 ≤30 秒（wav/mp3） |

> 你要的厂商如果只有开源权重（FireRedASR / Kimi-Audio），在带 GPU 的机器上部署后，都能通过「FunASR 自部署通道」或 `beginCustom()` 落到同一套 API 上。

---

## 一句话转写（AIKitASRClient）

### 基本用法

```cpp
AIKitASRClient asr(&Serial);
asr.begin(AIKIT_ASR_DASHSCOPE, "sk-...");   // 8 家预设见下表
asr.setLanguage("zh");                       // 可选

// 裸 PCM16 → 自动包 WAV 头上传 → 返回文本
String text;
int err = asr.transcribePCM(pcm, pcmLen, /*sampleRate=*/16000, /*channels=*/1, text);

// 或上传现成的 wav / mp3 文件字节
asr.transcribe(wavBytes, wavLen, "clip.wav", "audio/wav", text);
```

### 厂商预设表

| 预设 | 厂商 | 协议风格 | 默认模型 | 备注 |
| --- | --- | --- | --- | --- |
| `AIKIT_ASR_OPENAI` | OpenAI | multipart | `gpt-transcribe` | ≤25MB；whisper-1 亦可用 |
| `AIKIT_ASR_GROQ` | Groq | multipart | `whisper-large-v3-turbo` | 完全 OpenAI 兼容 |
| `AIKIT_ASR_MISTRAL` | Mistral | multipart（`x-api-key`） | `voxtral-mini-latest` | 最长 3 小时音频 |
| `AIKIT_ASR_ZHIPU` | 智谱 | multipart | `glm-asr-2512` | **单条 ≤30 秒**，wav/mp3 |
| `AIKIT_ASR_DASHSCOPE` | 阿里百炼 | JSON + base64 | `fun-asr-flash` | `setModel("qwen3-asr-flash")` 切 Qwen-ASR；≤5min |
| `AIKIT_ASR_STEPFUN` | 阶跃星辰 | JSON + SSE | `stepaudio-3-asr-max` | body 为 JSON（非 multipart） |
| `AIKIT_ASR_MIMO` | 小米 | chat + base64 | `mimo-v2.5-asr` | 仅 mp3 / wav |
| `AIKIT_ASR_FUNASR` | FunASR 自部署 | multipart | `sensevoice` | `asr.beginFunASR("192.168.1.10")`，无鉴权 |

### 内存约束（务必阅读）

请求体在堆上**整体拼装**（multipart 或 base64 JSON），上限宏 `AIKIT_ASR_MAX_UPLOAD_BYTES`（默认 64 KB）：

- **ESP32**：几十 KB 音频无压力（有 PSRAM 更宽裕）；
- **ESP8266**：建议音频 ≤12 KB（16 kHz PCM16 约 6 秒），或编译期调小该宏；
- 超限返回 `AIKIT_ERR_PAYLOAD_TOO_BIG`，不会 OOM 崩溃。

### 常用方法

| 方法 | 说明 |
| --- | --- |
| `setModel(m)` | 换模型（如 qwen3-asr-flash / whisper-1） |
| `setLanguage("zh")` | 语言提示（未设置则不发送该字段） |
| `setPrompt(p)` | 上下文提示词（OpenAI / 智谱支持） |
| `setInsecure(bool)` / `setCACert(pem)` | TLS（默认 insecure=true，生产请配 CA） |
| `setTimeouts(connectMs, tcpMs)` | 转写整体较慢，默认 90s 读超时 |
| `lastStatusCode()` / `lastErrorBody()` | 排错 |

---

## 实时识别（AIKitRealtimeASR）

### ⚠ 可选依赖

需要 [ArduinoWebsockets](https://github.com/gilmaimon/ArduinoWebsockets)（gilmaimon，v0.5+）：

```bash
arduino-cli lib install "ArduinoWebsockets"
```

未安装时本类全部方法返回 `AIKIT_ERR_NOT_SUPPORTED`，库其余功能不受影响。

### 基本用法

```cpp
AIKitRealtimeASR rt(&Serial);
rt.begin(AIKIT_RT_DASHSCOPE, "sk-...");     // 见下表；火山/百度用专用 begin
rt.setLanguage("zh");
rt.onPartial([](const char* t) { /* 本句迄今文本（会变） */ });
rt.onFinal([](const char* t) { /* 一句话定稿，不再变 */ });
rt.onError([](int e, const char* m) { /* 错误 */ });

rt.connect();                               // 阻塞：握手 + 启动指令
// 主循环：
rt.sendAudio(pcm, 3200);                    // 16k/16bit/mono，100ms 一帧（3200B）
rt.loop();                                  // 必须高频调用（收包 + 保活）
rt.finish(8000);                            // 结束信号 + 等最终结果 + 关连接
```

### 厂商预设表

| 预设 | begin 方式 | 端点（协议） | 备注 |
| --- | --- | --- | --- |
| `AIKIT_RT_VOLC` | `beginVolc(appId, accessToken)` 或 `begin(..., key1, key2)` | `wss://openspeech.bytedance.com/api/v3/sauc/bigmodel`（二进制帧 + gzip 响应） | Resource-Id 默认 `volc.bigasr.sauc.duration`；无心跳，静音期由库自动 ping |
| `AIKIT_RT_DASHSCOPE` | `begin(v, apiKey)` | `wss://dashscope.aliyuncs.com/api-ws/v1/inference`（run-task 双工） | 默认 `paraformer-realtime-v2`；`heartbeat:true` 保活 |
| `AIKIT_RT_BAIDU` | `beginBaidu(appId, appKey)` | `wss://vop.baidu.com/realtime_asr?sn=...` | `setBaiduDevPid()` 选语言（默认 15372 普通话加强标点）；5 秒不发音频断连，空闲时库自动发 HEARTBEAT |
| `AIKIT_RT_FUNASR` | `beginFunASR(host, port)` | `ws://host:10095`（自部署） | 2pass 模式（流式中间结果 + 句尾离线纠错定稿）；无鉴权 |
| `AIKIT_RT_OPENAI` | `begin(v, apiKey)` | `wss://api.openai.com/v1/realtime?intent=transcription` | 默认 `gpt-live-transcribe`；**音频 24 kHz**，库自动 16k→24k 线性重采样 + base64 |
| `AIKIT_RT_STEPFUN` | `begin(v, apiKey)` | `wss://api.stepfun.com/v1/realtime/asr/stream` | 默认 `stepaudio-2.5-asr-stream`；事件流与 OpenAI 同构 |
| `AIKIT_RT_MISTRAL` | `begin(v, apiKey)` | `wss://api.mistral.ai/v1/audio/transcriptions/realtime?model=...` | **实验性**：上行事件协议按 OpenAI 同构实现，未经真机验证 |

### 回词语义（跨厂商统一）

| 回调 | 语义 |
| --- | --- |
| `onPartial(text)` | **本句迄今为止的完整文本**（识别中，后续可能修正）。增量型厂商（FunASR online / OpenAI / Mistral）由库内部累积后回调，语义与全量型一致 |
| `onFinal(text)` | 一句话定稿（句号级），文本不再变化；每句触发一次 |
| `onError(err, msg)` | 协议 / 网络 / 服务端拒绝；随后连接通常已关闭 |
| `onConnected()` / `onClosed()` | 就绪（可发音频） / 连接关闭（含 finish 正常收尾） |

### API 摘要

| 方法 | 说明 |
| --- | --- |
| `connect()` | 建连 + 发启动指令（DashScope/OpenAI 会阻塞至就绪确认） |
| `sendAudio(pcm, len)` | 发一帧 16k/16bit/mono PCM；建议 100~200ms（3200~6400B）一帧 |
| `commitTurn()` | 手动提交一轮（仅 OpenAI / StepFun / Mistral 的手动轮次模式） |
| `finish(waitMs)` | 发结束信号 → 阻塞等最终结果 → 关连接；返回 `AIKIT_OK` / `AIKIT_ERR_TIMEOUT` |
| `loop()` | **必须在主循环高频调用**：驱动接收与空闲保活 |
| `close()` | 立即关闭（不等结果） |
| `setModel / setLanguage / setBaiduDevPid` | 厂商参数 |
| `setInsecure / setCACert` | TLS（默认 insecure=true） |

### 音频采集建议

- **ESP32**：I2S 数字麦克风（INMP441 / MSM261，约 ¥10），完整接线与驱动代码见 [examples/19](../../examples/19_ASR_Realtime/19_ASR_Realtime.ino)；
- **ESP8266**：无便捷 I2S 方案，需 ADC + 软件采样或外挂编码器；**实时识别建议直接用 ESP32**（BearSSL TLS 握手本身就要 ~22KB 堆）；
- 所有实时厂商统一要求 **16 kHz / 16 bit / 单声道 PCM**（OpenAI 的 24 kHz 由库内部重采样解决）。

### 各家协议细节（实现核对依据）

- **火山**：4 字节二进制头（`0x11 0x11/0x21/0x23 ...`）+ 大端 seq/size + payload；首包 full client request（JSON），音频帧 audio-only，末包负 seq；响应可能 gzip（库复用 StreamHTTPClient 内置 miniz 解压）；错误帧 message_type=0xF。
- **DashScope**：`run-task`（`header.action`）→ `task-started` → 二进制音频帧 → `result-generated`（`payload.output.sentence`，`sentence_end` 区分中间/定稿）→ `finish-task` → `task-finished`；`heartbeat:true` 开启保活空包。
- **百度**：`START`（appid/appkey/dev_pid/cuid/format/sample）→ 裸 PCM 二进制帧 → `MID_TEXT`（中间）/ `FIN_TEXT`（定稿或错误，看 `err_no`）→ `FINISH`；服务端每 ~5s 发 `HEARTBEAT`。
- **FunASR**：配置 JSON（`mode:"2pass"`、`chunk_size:[5,10,5]`...）→ 裸 PCM → `{"is_speaking":false}` 结束；响应 `mode` 区分 `2pass-online`（增量）/ `2pass-offline`（定稿）。
- **OpenAI**：`session.created` → `session.update`（嵌套式 GA 结构）→ `input_audio_buffer.append`（base64 PCM24k）→ `commit` → `...transcription.delta / .completed`；无应用层心跳，靠协议层 ping。
