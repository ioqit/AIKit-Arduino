# AIKit-Arduino

**[English](./README_EN.md)** | 简体中文

一个面向 Arduino 的 **AI 能力一站式客户端库**：一个 `begin()` 接入 19 家厂商的 OpenAI 兼容 Chat Completions API（非流式 + SSE 流式），外加 **云端语音转文字**——HTTP 一句话转写与 WebSocket 实时识别。专为 ESP32 / ESP8266 打造。

```cpp
AIKitClient ai(&Serial);                 // 日志输出口在构造时传入
ai.begin(AIKIT_DEEPSEEK, "sk-...");      // 一键接入厂商
String reply;
ai.chat("用一句话介绍你自己", reply);     // 非流式
ai.onDelta([](const char* d, size_t n) { Serial.write(d, n); });
ai.chatStream("讲个故事");                // 流式（打字机效果）

AIKitASRClient asr(&Serial);             // 语音转文字（一句话）
asr.begin(AIKIT_ASR_DASHSCOPE, "sk-...");
asr.transcribePCM(pcm, len, 16000, 1, text);

AIKitRealtimeASR rt(&Serial);            // 实时语音识别（WebSocket）
rt.begin(AIKIT_RT_DASHSCOPE, "sk-...");
rt.connect(); rt.sendAudio(pcm, len); rt.loop(); rt.finish();
```

---

## ⚠ 依赖（重要，请先读这里）

本库**不是零依赖库**。所有功能都建立在下列库之上，**缺一编译不过**：

| 库 | 必要性 | 用途 | 安装方式 |
| --- | --- | --- | --- |
| [ArduinoJson](https://arduinojson.org/) **v7+** | **必装** | 请求构建 / 响应解析 | 库管理器直接安装 |
| [StreamHTTPClient](https://github.com/ioqite/StreamHTTPClient) **v1.0+** | **必装** | HTTP/HTTPS 传输（同步 / 异步 / SSE / gzip） | 暂未上架库管理器，从 GitHub 安装（见下） |
| [ArduinoWebsockets](https://github.com/gilmaimon/ArduinoWebsockets) **v0.5+** | **可选** | 仅**实时语音识别**（WebSocket）需要 | 库管理器直接安装 |

**三分钟装齐**（arduino-cli）：

```bash
arduino-cli lib install ArduinoJson ArduinoWebsockets
git clone https://github.com/ioqite/StreamHTTPClient.git ~/Arduino/libraries/StreamHTTPClient
git clone https://github.com/ioqit/AIKit-Arduino.git    ~/Arduino/libraries/AIKit-Arduino
```

Arduino IDE 用户：ArduinoJson / ArduinoWebsockets 用 *库管理器* 搜索安装；StreamHTTPClient 下载 [zip](https://github.com/ioqite/StreamHTTPClient/archive/refs/heads/master.zip) 后 *项目 → 导入库 → 添加 .ZIP 库*。

> **不装 ArduinoWebsockets 会怎样？** 对话、流式、HTTP 转写等全部功能**不受影响**；仅 `AIKitRealtimeASR`（实时识别）的方法返回 `AIKIT_ERR_NOT_SUPPORTED` 并在日志提示。库通过 `__has_include` 自动检测，无需任何配置。→ 详见 [docs/01_installation.md](docs/01_installation.md)

---

## 快速上手

```cpp
#include <AIKit.h>

AIKitClient ai(&Serial);              // 日志 → Serial；传 nullptr 完全静默

void setup() {
  Serial.begin(115200);
  WiFi.begin("ssid", "password");
  while (WiFi.status() != WL_CONNECTED) delay(300);

  ai.begin(AIKIT_DEEPSEEK, "sk-xxx"); // ① 一键接入（19 家预设见 docs/02）

  // ② 常用参数（可选）
  ai.setSystemPrompt("你是一个严谨的嵌入式助手");
  ai.setTemperature(0.7f);
  ai.setMaxTokens(1024);

  // ③ v1.2.0 增强（可选）
  ai.setRetry(3, 1000, 8000);         // 失败自动重试（指数退避）
  // ai.totalUsage()                   // 累计 token 用量

  // ④ 非流式对话
  String reply;
  if (ai.chat("你好", reply) == AIKIT_OK) Serial.println(reply);
}
void loop() {}
```

**多轮对话**（自动维护历史，可 JSON 导出 / 导入持久化）：

```cpp
AIKitHistory history;
history.setWindow(8);                  // 滑动窗口，防上下文无限增长
ai.chat(history, "我叫小明", reply);    // user 入史 → 请求 → 回复入史
ai.chat(history, "我叫什么？", reply);  // 第二轮记得上一轮内容
```

**语音转文字**：见 [examples/18_ASR_Transcribe](examples/18_ASR_Transcribe/18_ASR_Transcribe.ino)（一句话）与 [examples/19_ASR_Realtime](examples/19_ASR_Realtime/19_ASR_Realtime.ino)（实时流式）。

---

## 文档（按主题拆分）

| 文档 | 内容 |
| --- | --- |
| [docs/01_installation.md](docs/01_installation.md) | 安装与依赖详解：三种安装方式、PlatformIO、可选依赖说明、编译期配置宏总表 |
| [docs/02_vendors.md](docs/02_vendors.md) | 19 家 chat 厂商预设全表（端点 / 默认模型 / 鉴权）+ 本地部署专题（llama.cpp / Ollama / vLLM / LM Studio） |
| [docs/03_chat.md](docs/03_chat.md) | Chat API 参考：参数、自定义头 / JSON 注入、Token 用量统计、自动重试 |
| [docs/04_streaming.md](docs/04_streaming.md) | 流式两种模式（onDelta 回调 / Stream 逐行读）与平台差异 |
| [docs/05_asr.md](docs/05_asr.md) | 语音识别全解：8 家 HTTP 转写 + 7 家实时 WebSocket + 各家协议与音频要求 |
| [docs/06_history.md](docs/06_history.md) | 对话历史管理：动态内存、滑动窗口、JSON 导出 / 导入 |
| [docs/07_advanced.md](docs/07_advanced.md) | 进阶：TLS 证书、错误码表、内存调优、CI、Roadmap、已知限制 |

English documentation: [docs/en/](docs/en/)

---

## 示例一览（examples/，共 21 个）

| # | 主题 | # | 主题 |
| --- | --- | --- | --- |
| 01 | 最简非流式对话 | 12 | 错误处理 |
| 02 | 流式回调 | 13 | 日志控制 |
| 03 | system prompt 与参数 | 14 | 综合演练 |
| 04 | 多轮对话 | 15 | **Token 用量统计**（新） |
| 05 | 流式逐行读 | 16 | **失败自动重试**（新） |
| 06 | 厂商切换 | 17 | **对话历史管理**（新） |
| 07 | 自定义端点 | 18 | **HTTP 语音转写**（新） |
| 08 | 自定义请求头 | 19 | **实时语音识别**（新，WebSocket） |
| 09 | 自定义 JSON 字段 | 20 | **根证书导入**（新） |
| 10 | TLS 自定义 CA | 21 | **新厂商预设**（新，Groq 等 8 家） |
| 11 | 推理模型思维链 | | |

---

## 支持的平台

| 平台 | 状态 | 要点 |
| --- | --- | --- |
| **ESP32**（经典 / S2 / S3 / C3 / C6） | ✅ 完整支持（重点平台） | 流式逐行读由 FreeRTOS 后台任务推进；I2S 麦克风采集即插即用 |
| **ESP8266** | ✅ 完整支持（注意内存） | TLS 走 BearSSL（握手需 ~25KB 空闲堆）；长回复建议 onDelta 回调；实时 ASR 建议 ESP32 |
| Pico W / SAMD 等 | ⚠ 架构兼容、未验证 | 跟随 StreamHTTPClient 的平台支持；欢迎反馈 |

---

## 致谢与许可

- [StreamHTTPClient](https://github.com/ioqite/StreamHTTPClient) —— HTTP/HTTPS 传输层
- [ArduinoJson](https://arduinojson.org/) —— JSON 构建与解析（v7）
- [ArduinoWebsockets](https://github.com/gilmaimon/ArduinoWebsockets) —— 实时语音识别的 WebSocket 层（可选）
- 各厂商提供的开放接口与官方文档

MIT License © Ioqit
