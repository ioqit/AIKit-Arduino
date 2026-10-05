# AIKit-Arduino

**[English](./README_EN.md)** | 简体中文

一个面向 Arduino 的 **大模型（LLM）对话客户端库**：一个 `begin()` 接入国内外主流厂商的 OpenAI 兼容 Chat Completions API，支持**非流式**与 **SSE 流式**两种模式，专为 ESP32 / ESP8266 打造。

```cpp
AIKitClient ai(&Serial);                 // 日志输出口在构造时传入
ai.begin(AIKIT_DEEPSEEK, "sk-...");      // 一键接入厂商
String reply;
ai.chat("用一句话介绍你自己", reply);     // 非流式
ai.onDelta([](const char* d, size_t n) { Serial.write(d, n); });
ai.chatStream("讲个故事");                // 流式
```

构建于 [StreamHTTPClient](https://github.com/ioqite/StreamHTTPClient)（HTTP/HTTPS 传输）与 [ArduinoJson v7](https://arduinojson.org/)（JSON 构建/解析）之上。

---

## 目录

- [功能特性](#功能特性)
- [厂商 API 规格表](#厂商-api-规格表)
- [支持的平台](#支持的平台)
- [安装](#安装)
- [快速上手](#快速上手)
- [API 参考](#api-参考)
- [流式响应的两种读取方式](#流式响应的两种读取方式)
- [错误码](#错误码)
- [编译期配置宏](#编译期配置宏)
- [内存使用建议](#内存使用建议)
- [Roadmap（未来增强预留）](#roadmap未来增强预留)
- [已知限制](#已知限制)
- [致谢](#致谢)

---

## 功能特性

| 特性 | 说明 |
| --- | --- |
| 一键接入厂商 | `begin(厂商预设, apiKey)`，内置 11 个预设（含国内/国际双端点） |
| 任意兼容端点 | `beginCustom(host, port, path, https, key)` 接入自建网关 / vLLM / Ollama / 中转站 |
| 非流式对话 | `chat()`，支持单条消息、多轮历史、完整结果（正文/思维链/finish_reason/token 用量） |
| 流式对话 A | `chatStream()` + `onDelta()` 回调，逐块增量（附 `onReasoningDelta()` 思维链） |
| 流式对话 B | `chatStreamTo()` + `AIKitStreamResponse`（继承 Arduino `Stream`），逐行读取 SSE 原始行 |
| 常用参数 | system prompt / temperature / top_p / max_tokens / max_completion_tokens |
| 自定义 HTTP 头 | `addHeader(name, value, replace)`：同名覆盖为单行，或追加为重复行 |
| 自定义 JSON 字段 | `addJsonField(key, jsonValue, replace)`：覆盖或追加到最终请求 JSON（透传 tools 等高级参数） |
| TLS | 自定义根 CA 证书（`setCACert`）+ `insecure` 开关 |
| 日志路由 | 输出目标由构造函数传入（`AIKitClient ai(&Serial0)`），任意 `Print`；四级日志 + 完全静默 |
| 稳健性 | 无异常、无 RTTI、无崩溃路径；全参数校验、OOM/溢出优雅降级、跨核临界区保护 |

---

## 厂商 API 规格表

以下端点信息均依据**各厂商官方文档**核实整理（核对时间 2026-10），附官方文档入口。厂商迭代频繁，如与你所见文档不一致，请以官方文档为准（本库端点与模型均可用 `setModel()` / `beginCustom()` 自行覆盖）。

### 总表

| 预设枚举 | 厂商 | Base URL | Chat Completions 路径 | 默认模型 | 鉴权 |
| --- | --- | --- | --- | --- | --- |
| `AIKIT_DEEPSEEK` | DeepSeek 深度求索 | `https://api.deepseek.com` | `/chat/completions` | `deepseek-flash` | Bearer |
| `AIKIT_MOONSHOT` | Moonshot AI（Kimi） | `https://api.moonshot.cn` | `/v1/chat/completions` | `kimi-k3` | Bearer |
| `AIKIT_MOONSHOT_INTL` | Moonshot（国际站） | `https://api.moonshot.ai` | `/v1/chat/completions` | `kimi-k3` | Bearer |
| `AIKIT_QWEN_DASHSCOPE` | 阿里云百炼（Qwen） | `https://dashscope.aliyuncs.com/compatible-mode` | `/v1/chat/completions` | `qwen-plus` | Bearer |
| `AIKIT_QWEN_DASHSCOPE_INTL` | 阿里云百炼（国际站） | `https://dashscope-intl.aliyuncs.com/compatible-mode` | `/v1/chat/completions` | `qwen-plus` | Bearer |
| `AIKIT_ZHIPU` | 智谱开放平台（GLM） | `https://open.bigmodel.cn/api` | `/paas/v4/chat/completions` | `glm-4.7-flash` | Bearer |
| `AIKIT_ZHIPU_ZAI` | Z.ai（智谱国际站） | `https://api.z.ai/api` | `/paas/v4/chat/completions` | `glm-4.7-flash` | Bearer |
| `AIKIT_SILICONFLOW` | 硅基流动 SiliconFlow | `https://api.siliconflow.cn` | `/v1/chat/completions` | `deepseek-ai/DeepSeek-V4-Flash` | Bearer |
| `AIKIT_MINIMAX` | MiniMax（国内） | `https://api.minimax.cn` | `/v1/text/chatcompletion_v2` | `MiniMax-M2.7` | Bearer |
| `AIKIT_MINIMAX_INTL` | MiniMax（国际） | `https://api.minimaxi.com` | `/v1/text/chatcompletion_v2` | `MiniMax-M2.7` | Bearer |
| `AIKIT_OPENAI` | OpenAI | `https://api.openai.com` | `/v1/chat/completions` | `gpt-5-mini` | Bearer |

所有预设均为 HTTPS(443) + `Authorization: Bearer <key>`，SSE 流式格式彼此兼容（`data: {json}` 行 + `data: [DONE]` 哨兵）。

### 各厂商要点（核对自官方文档）

**DeepSeek**（[官方文档](https://api-docs.deepseek.com/)）
- Base URL **不带 `/v1`**（官方推荐写法；旧式 `/v1` 前缀亦兼容）。
- 模型：`deepseek-flash`（推荐默认；旧名 `deepseek-v4-flash` 等仍被接受）、`deepseek-v4-pro`。
- `temperature` 取值 0~2；流式与非流式均支持。

**Moonshot AI / Kimi**（[官方文档](https://platform.moonshot.cn/docs/api/chat)）
- 国内 `api.moonshot.cn`，国际 `api.moonshot.ai`；两站 API Key 不互通。
- 模型：`kimi-k3`（旗舰）、`kimi-k2.6`、`kimi-k2.7-code`。
- ⚠️ `max_tokens` 已标记**弃用**，请用 `setMaxCompletionTokens()`（写入 `max_completion_tokens`）。

**阿里云百炼 / Qwen**（[官方文档](https://help.aliyun.com/zh/model-studio/developer-reference/compatibility-of-openai-with-dashscope)）
- OpenAI 兼容模式完整前缀：`/compatible-mode/v1`，勿遗漏。
- 官方正在迁移到新域名 `https://{WorkspaceId}.cn-beijing.maas.aliyuncs.com/compatible-mode/v1`（可用 `beginCustom()` 接入）；现域名持续可用。
- 模型：`qwen-plus`（均衡默认）、`qwen-max`、`qwen-turbo`、`qwen-long`、`qwen3.8-max` 等。
- 国际站（新加坡/弗吉尼亚等）用 `dashscope-intl` 预设；国内外 Key 不互通。

**智谱 GLM**（[官方文档](https://docs.bigmodel.cn/)）
- 路径前缀特殊：`/api/paas/v4/chat/completions`。
- 模型：GLM-5.3 / GLM-4.7 / GLM-4.6 系列；**`*-flash` 系列免费**（如 `glm-4.7-flash`，默认值即它）。
- 思考模式：`addJsonField("thinking", "{\"type\":\"enabled\"}")`。
- 国际站为 Z.ai（`https://api.z.ai/api/paas/v4/chat/completions`）。

**SiliconFlow 硅基流动**（[官方文档](https://docs.siliconflow.cn/cn/api-reference/chat-completions/chat-completions)）
- 聚合平台，200+ 模型；**模型名必须带 `厂商/` 前缀**（如 `deepseek-ai/DeepSeek-V4-Flash`、`Qwen/Qwen3-8B`、`moonshotai/Kimi-K2.7-Code`）。
- 注册赠送额度，适合开发调试。

**MiniMax**（[官方文档](https://platform.minimaxi.com/docs)）
- ⚠️ 路径为 `/v1/text/chatcompletion_v2`，**不是** `/v1/chat/completions`（预设已处理）。
- 国内域名已迁移至 `api.minimax.cn`（旧 `api.minimax.chat` 仍可用）；国际 `api.minimaxi.com`。
- 模型：`MiniMax-M2.7`（默认）、`MiniMax-M2.5`、`MiniMax-M3`、`MiniMax-Text-01` 等。
- ⚠️ `max_tokens` 已标记弃用，请用 `setMaxCompletionTokens()`。

**OpenAI**（[官方文档](https://platform.openai.com/docs/api-reference/chat)）
- 模型：`gpt-5-mini`（默认）、GPT-5.6 系列（当前旗舰）等。
- ⚠️ o 系列推理模型**只接受** `max_completion_tokens`，且 `temperature` 固定为 1——用自定义 JSON 字段覆盖即可。
- Chat Completions 为长期稳定 API。

> 未列出的厂商只要提供 OpenAI 兼容端点（Groq / OpenRouter / Together / Mistral / vLLM / Ollama / OneAPI …），都可以用 `beginCustom()` 一行接入。

---

## 支持的平台

芯片支持与处理方式跟随 [StreamHTTPClient](https://github.com/ioqite/StreamHTTPClient)（传输层），本库在其上做了验证：

| 平台 | 状态 | 说明 |
| --- | --- | --- |
| **ESP32**（经典 / S2 / S3 / C3 / C6…Arduino-ESP32 核心） | ✅ 完整支持（**重点平台**） | 流式逐行读由 FreeRTOS 后台任务推进，`readLine()` 真·边收边读；CI 编译验证 esp32 / esp32s3 / esp32c3 |
| **ESP8266**（Arduino-ESP8266 核心） | ✅ 完整支持（注意内存） | TLS 走 BearSSL，握手需约 25KB 空闲堆；逐行读为「同步执行+入队」模式（带上限），长回复建议用 onDelta 回调；CI 编译验证 nodemcuv2 |
| Raspberry Pi Pico W / SAMD 等 StreamHTTPClient 支持平台 | ⚠️ 架构兼容、未验证 | 走单上下文代码路径；欢迎反馈 |

---

## 安装

本库依赖两个库，请先安装：

1. **ArduinoJson v7**（库管理器直接安装）
2. **StreamHTTPClient**（暂未上架库管理器，从 GitHub 安装）：
   - 下载 [zip](https://github.com/ioqite/StreamHTTPClient/archive/refs/heads/master.zip)，Arduino IDE：*项目 → 导入库 → 添加 .ZIP 库*；
   - 或 `git clone https://github.com/ioqite/StreamHTTPClient.git ~/Arduino/libraries/StreamHTTPClient`

然后安装 AIKit-Arduino 本体（同样方式）。arduino-cli 用户：

```bash
arduino-cli lib install ArduinoJson
git clone https://github.com/ioqite/StreamHTTPClient.git ~/Arduino/libraries/StreamHTTPClient
git clone https://github.com/ioqite/AIKit-Arduino.git ~/Arduino/libraries/AIKit-Arduino
```

---

## 快速上手

```cpp
#include <AIKit.h>

AIKitClient ai(&Serial);              // 日志 → Serial；传 nullptr 完全静默

void setup() {
  Serial.begin(115200);
  WiFi.begin("ssid", "password");
  while (WiFi.status() != WL_CONNECTED) delay(300);

  ai.begin(AIKIT_DEEPSEEK, "sk-xxx"); // ① 一键接入

  // ② 常用参数（可选）
  ai.setSystemPrompt("你是一个严谨的嵌入式助手");
  ai.setTemperature(0.7f);
  ai.setMaxTokens(1024);

  // ③ 非流式
  String reply;
  if (ai.chat("你好", reply) == AIKIT_OK) {
    Serial.println(reply);
  }
}
void loop() {}
```

流式（打字机效果）：

```cpp
ai.onDelta([](const char* delta, size_t len) {
  Serial.write((const uint8_t*)delta, len);
});
ai.chatStream("从 1 数到 10");
```

更多示例见 `examples/`（**以功能为主线**，共 14 个）：基础对话 / 流式回调 / system 与参数 / 多轮 / 逐行读 / 厂商切换 / 自定义端点 / 自定义请求头 / 自定义 JSON 字段 / TLS 证书 / 思维链 / 错误处理 / 日志控制 / 综合演练。

---

## API 参考

### 生命周期（begin + setter 风格）

| 方法 | 说明 |
| --- | --- |
| `AIKitClient(Print* log = nullptr)` | 构造时指定日志输出目标（任意 `Print`/`Stream`） |
| `bool begin(AIKitVendor vendor, const char* apiKey)` | 一键接入内置厂商；可重复调用切换厂商（模型重置为新厂商默认） |
| `bool beginCustom(host, port, path, https, apiKey, defaultModel)` | 接入任意 OpenAI 兼容端点 |
| `void end()` | 结束当前请求/流并释放连接（保留配置） |
| `bool isReady()` | 是否已具备请求条件 |

### 请求参数 setter

| 方法 | 说明 |
| --- | --- |
| `setModel(m)` | 模型名（见规格表） |
| `setSystemPrompt(s)` / `clearSystemPrompt()` | system 消息（自动前置；用户消息数组已含 system 时不重复添加） |
| `setTemperature(t)` | 0~2 |
| `setTopP(p)` | 0~1 |
| `setMaxTokens(v)` | 写入 `max_tokens` |
| `setMaxCompletionTokens(v)` | 写入 `max_completion_tokens`（Moonshot/OpenAI 新规范） |
| `setTimeouts(connectMs, tcpMs)` | 连接/读取超时（默认 10s/30s，推理模型友好） |

### TLS

| 方法 | 说明 |
| --- | --- |
| `setInsecure(bool)` | 跳过证书校验开关（**默认 true**，便于无根证书环境上手；生产环境务必关掉） |
| `setCACert(pem)` | 自定义根 CA（PEM 文本；ESP8266 的 PROGMEM 字符串用 `setCACert_P(PSTR(...))`） |

### 自定义 HTTP 请求头

| 方法 | 说明 |
| --- | --- |
| `addHeader(name, value, replace=true)` | `replace=true`：同名头覆盖为单行（可覆盖库默认的 `Content-Type`/`Accept`）；`replace=false`：同名头追加为重复行（多值头） |
| `removeHeader(name)` / `clearHeaders()` | 移除 |

### 自定义 JSON 字段（进）

| 方法 | 说明 |
| --- | --- |
| `addJsonField(key, jsonValue, replace=true)` | 值须为合法 JSON 文本（数字/带引号字符串/对象/数组/bool）；`replace=true` 覆盖库生成的同名字段，否则仅追加缺失字段。设置时即做合法性校验 |
| `addJsonField(key, float/int32_t/bool, ...)` | 数字/布尔便捷重载 |
| `addJsonFieldString(key, str, ...)` | 普通文本便捷版（自动加引号并转义） |
| `removeJsonField(key)` / `clearJsonFields()` | 移除 |

> 这是“逃生舱”通道：官方未提供 setter 的参数（`tools`、`response_format`、`thinking`、各厂思考开关…）都能透传，见示例 `09_CustomJsonFields`。

### 对话

| 方法 | 说明 |
| --- | --- |
| `int chat(userText, String& outContent)` | 单条 → 回复正文 |
| `int chat(userText, AIKitChatResult& result)` | 单条 → 完整结果（`content` / `reasoningContent` / `finishReason` / `usage`） |
| `int chat(messages, count, ...)` | 多轮 / 自定义角色 |
| `int chatStream(userText)` / `chatStream(messages, count)` | 流式（onDelta 回调模式） |
| `int chatStreamTo(userText, AIKitStreamResponse&)` | 流式（逐行读模式） |
| `onDelta(cb)` / `onReasoningDelta(cb)` | 增量回调 `void(const char* delta, size_t len)` |

### 状态查询

| 方法 | 说明 |
| --- | --- |
| `lastStatusCode()` | 最近 HTTP 状态码（0 = 未收到） |
| `lastTransportError()` | 底层 StreamHTTPClient 错误码（配合 `StreamHTTPClient::errorToString()`） |
| `lastErrorBody()` | 非 2xx 时截取保留的响应体（默认 ≤384 字节） |
| `lastFinishReason()` / `lastUsage()` | 流式/非流式统一记录 |
| `streamActive()` | 是否有流进行中（期间新的 chat* 调用返回 `AIKIT_ERR_BUSY`） |

---

## 流式响应的两种读取方式

### 方式 A：onDelta 回调

```cpp
ai.onDelta([](const char* d, size_t n) { Serial.write((const uint8_t*)d, n); });
ai.chatStream("讲个故事");   // 请求期间增量回调，返回即流结束
```

两个平台行为完全一致（回调在请求执行期间被触发），是 **ESP8266 上处理长回复的首选**。

### 方式 B：Stream 逐行读（AIKitStreamResponse）

```cpp
AIKitStreamResponse resp;
int err = ai.chatStreamTo("讲个故事", resp);
String line, piece;
while (resp.readLine(line)) {                        // 阻塞直至新行/结束
  if (AIKitStreamResponse::isDoneSentinel(line)) break;
  if (AIKitStreamResponse::extractDelta(line, piece)) Serial.print(piece);
  // line 是 SSE 原始行（"data: {...}"），也可自行用 ArduinoJson 解析
}
resp.close();
```

`AIKitStreamResponse` 继承自 Arduino `Stream`，`available()/read()/peek()/readStringUntil()` 等标准用法均可用。

**平台差异**（源于 StreamHTTPClient 的执行模型）：

| | ESP32 | ESP8266 等 |
| --- | --- | --- |
| 请求推进 | FreeRTOS 后台任务，`chatStreamTo()` 立即返回 | 在 `chatStreamTo()` 内同步执行完毕 |
| 读取体验 | `readLine()` 真流式（边收边读，环形缓冲+背压） | 行先入队（`AIKIT_LINE_QUEUE_MAX_LINES` / `MAX_BYTES` 上限）再读出 |
| 长回复建议 | 无限制（内存允许） | 用方式 A；或调大上限编译宏 |

两种方式给出相同的 SSE 原始行与 HTTP 状态/错误信息；超限不会崩溃，而是置 `AIKIT_ERR_BUFFER_OVERFLOW` 并可经 `resp.lastError()` 查询。

---

## 错误码

`chat*` 系列返回 `AIKIT_OK(0)` 或负数错误码；HTTP 非 2xx 统一映射为 `AIKIT_ERR_HTTP_STATUS`，具体状态码与错误体见 `lastStatusCode()` / `lastErrorBody()`。

| 错误码 | 值 | 含义 / 排查 |
| --- | --- | --- |
| `AIKIT_OK` | 0 | 成功 |
| `AIKIT_ERR_CONFIG` | -101 | 未 `begin()` / Key 缺失 |
| `AIKIT_ERR_NULL_ARG` | -102 | 空指针参数 |
| `AIKIT_ERR_TOO_MANY` | -103 | 自定义头/字段超过容量（对应编译宏） |
| `AIKIT_ERR_BAD_JSON` | -104 | 自定义 JSON 值非法（设置时即报） |
| `AIKIT_ERR_CONNECT` | -105 | 连接/发送失败 → 看 `lastTransportError()` |
| `AIKIT_ERR_HTTP_STATUS` | -106 | HTTP 非 2xx → 看 `lastStatusCode()` + `lastErrorBody()` |
| `AIKIT_ERR_PARSE` | -107 | 响应 JSON 解析失败 |
| `AIKIT_ERR_OOM` | -108 | 内存不足（JSON 构建/任务创建） |
| `AIKIT_ERR_TIMEOUT` | -109 | 超时 |
| `AIKIT_ERR_BUSY` | -110 | 已有请求/流进行中（先 `resp.close()` 或 `end()`） |
| `AIKIT_ERR_LINE_TOO_LONG` | -111 | SSE 单行超 `AIKIT_SSE_LINE_MAX`（该行被丢弃，流继续） |
| `AIKIT_ERR_BUFFER_OVERFLOW` | -112 | 逐行读缓冲超限（见平台差异） |
| `AIKIT_ERR_STREAM_CLOSED` | -113 | 流已关闭 |
| `AIKIT_ERR_NOT_SUPPORTED` | -114 | 当前平台/配置不支持 |

`aikitErrorToString(err)` 返回可读描述。

---

## 编译期配置宏

在包含 `AIKit.h` 之前 `-D` 定义即可覆盖：

| 宏 | 默认值 | 说明 |
| --- | --- | --- |
| `AIKIT_MAX_CUSTOM_HEADERS` | 8 | 自定义请求头容量 |
| `AIKIT_MAX_JSON_FIELDS` | 12 | 自定义 JSON 字段容量 |
| `AIKIT_SSE_LINE_MAX` | 2048 | SSE 单行上限（超长行丢弃并重同步） |
| `AIKIT_LINE_RING_SLOTS` | 16 | [ESP32] 行环形缓冲槽数 |
| `AIKIT_LINE_PUSH_TIMEOUT_MS` | 30000 | [ESP32] 缓冲满时生产者最长等待 |
| `AIKIT_LINE_QUEUE_MAX_LINES` | 96 | [非 ESP32] 行队列行数上限 |
| `AIKIT_LINE_QUEUE_MAX_BYTES` | 32768 | [非 ESP32] 行队列字节上限 |
| `AIKIT_ERRBODY_MAX` | 384 | 错误响应体保留长度 |
| `AIKIT_LOG_BUF` | 160 | 单条日志长度上限 |
| `AIKIT_DEFAULT_CONNECT_TIMEOUT_MS` | 10000 | 默认连接超时 |
| `AIKIT_DEFAULT_TCP_TIMEOUT_MS` | 30000 | 默认读取超时 |

---

## 内存使用建议

- **ESP32**：非流式响应会完整载入内存（StreamHTTPClient 缓冲）；数千 token 的回复通常在数十 KB 量级，无压力。流式逐行读的峰值内存 ≈ 槽数 × 行长（默认 16×~1KB）。
- **ESP8266**：用户堆约 40KB；BearSSL 握手需 ~25KB 空闲。建议：
  - 长回复用 **onDelta 回调**（边收边处理，不驻留内存）；
  - `setMaxTokens()` 限制回复长度；
  - 逐行读模式留意 `AIKIT_LINE_QUEUE_*` 上限；
  - 避免与大型第三方库（如大缓冲区显示驱动）同时持有内存。
- 所有错误路径都优雅返回错误码，不会因 OOM 崩溃或重启。

---

## Roadmap（未来增强预留）

以下能力**已预留位置**，按优先级规划中（欢迎在 Issues 提需求排序）：

- [ ] **Tool Calls 一等公民 API**：请求构建 + 流式 `tool_calls` 增量聚合（现阶段可先用 `addJsonField("tools", ...)` 透传，见示例 09）
- [ ] **Vision 多模态**：`content` 数组（image_url）便捷构建器
- [ ] **Embeddings**：`/v1/embeddings` 接口
- [ ] **模型列表**：`/v1/models` 拉取与解析
- [ ] **用量统计**：跨请求累计 token / 成本估算
- [ ] **自动重试与限速**：可配置的退避重试（现阶段见示例 12 手动实现）
- [ ] **Pico W / SAMD 实测**与官方适配声明
- [ ] **Keep-Alive 连接池**（多厂商并发实例）

架构上的预留：消息结构 `AIKitMessage` 保持轻量指针语义，可直接扩展多模态 `content`；自定义 JSON 通道是所有未落地特性的通用透传后门；`AIKIT_ERR_NOT_SUPPORTED` 为未来接口存根统一预留返回值。

---

## 已知限制

- 默认 `insecure=true`（跳过 TLS 证书校验）——为无根证书/无 NTP 环境的可用性取舍，**生产部署请 `setCACert()` + `setInsecure(false)`**（见示例 10）。
- 非流式响应需完整载入内存后解析（超长回复请用流式）。
- 不代管对话历史（内存考量），历史数组由用户维护。
- ESP8266 逐行读为缓冲模式（详见上文平台差异）。
- API Key 以明文参与请求，请勿将含 Key 的 DEBUG 日志发布到公开渠道。

---

## 致谢

- [StreamHTTPClient](https://github.com/ioqite/StreamHTTPClient) —— HTTP/HTTPS 传输层（同步/异步/SSE 能力）
- [ArduinoJson](https://arduinojson.org/) —— JSON 构建与解析（v7）
- 各厂商提供的 OpenAI 兼容开放接口
