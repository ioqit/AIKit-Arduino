# 03 - Chat API 参考

**[English](../en/03_chat.md)** | 简体中文

本页是 `AIKitClient`（[src/AIKitClient.h](../src/AIKitClient.h)）的完整 API 参考，以表格为主。厂商预设与端点信息见 [docs/02_vendors.md](02_vendors.md)；流式两种读取模式的深入说明见 [docs/04_streaming.md](04_streaming.md)；带 `v1.2.0` 标记的小节为本版新增。

## 生命周期（begin + setter 风格）

| 方法 | 说明 |
| --- | --- |
| `explicit AIKitClient(Print* log = nullptr)` | 构造时指定日志输出口（任意 `Print`/`Stream`，如 `&Serial`、`&Serial0`）；传 `nullptr` 完全静默。日志对象须在客户端生命周期内保持有效 |
| `bool begin(AIKitVendor vendor, const char* apiKey)` | 一键接入内置厂商预设（19 家，见 [docs/02_vendors.md](02_vendors.md)）。可重复调用切换厂商；用户 `setModel()` 过的模型在切换时重置为新厂商默认值。参数非法返回 `false` |
| `bool begin(AIKitVendor vendor, const String& apiKey)` | 同上（`String` 重载） |
| `bool beginCustom(host, port, path, useHttps, apiKey, defaultModel = nullptr)` | 接入任意 OpenAI 兼容端点（自建网关 / vLLM / Ollama / 中转站…）；`defaultModel` 可空 |
| `void end()` | 结束当前请求/流并释放连接（**全部配置保留**，可再次 chat） |
| `bool isReady() const` | 是否已具备发起请求的条件（已 begin 且配置完整） |

## 厂商 / 模型

| 方法 | 说明 |
| --- | --- |
| `AIKitVendor vendor() const` | 当前厂商枚举（`AIKIT_CUSTOM` 表示自定义端点） |
| `const char* vendorName() const` | 厂商可读名称 |
| `const char* model() const` | 当前模型名 |
| `void setModel(const char* model)` / `setModel(const String&)` | 换模型（覆盖预设默认值；各厂可用模型见 [docs/02_vendors.md](02_vendors.md)） |

## 请求参数 setter

任意时刻可改，**下一次请求生效**：

| 方法 | 说明 |
| --- | --- |
| `setSystemPrompt(sys)` / `setSystemPrompt(String)` | system 消息，随每次请求自动前置；传 `nullptr`/`""` 等效清除。若你的消息数组本身以 system 开头则自动跳过（不会重复） |
| `clearSystemPrompt()` / `systemPrompt()` | 清除 / 读取当前 system prompt |
| `setTemperature(float t)` | 采样温度，0~2；越界值视为清除该参数 |
| `setTopP(float p)` | 核采样，0~1 |
| `setMaxTokens(int32_t v)` | 写入请求中的 `max_tokens` |
| `setMaxCompletionTokens(int32_t v)` | 写入 `max_completion_tokens`（Moonshot / OpenAI 现行规范，老参数已弃用） |
| `setTimeouts(connectMs, tcpMs)` | 连接 / 读超时，默认 10 s / 30 s（推理模型首字节延迟可达数十秒，已给足） |

## TLS

| 方法 | 说明 |
| --- | --- |
| `setInsecure(bool)` / `insecure()` | 跳过证书校验开关，**默认 `true`**（便于无 NTP / 无根证书环境开箱即用）。生产环境请务必配合 `setCACert()` 关闭！ |
| `setCACert(const char* pem)` | 自定义根 CA（PEM 文本），指针须在请求期间保持有效。ESP8266 上库会在首次请求时惰性解析为 `BearSSL::X509List` 并缓存 |
| `setCACert_P(PGM_P pem)` | [仅 ESP8266] 证书存于 PROGMEM 时使用，如 `setCACert_P(PSTR("-----BEGIN CERTIFICATE-----..."))` |

## 自定义 HTTP 请求头

随每次请求发送：

| 方法 | 说明 |
| --- | --- |
| `int addHeader(name, value, replace = true)` | `replace=true`：同名头覆盖为单行（也可覆盖库默认的 `Content-Type` / `Accept`）；`replace=false`：追加为重复行（多值头场景）。返回 `AIKIT_OK` / `AIKIT_ERR_NULL_ARG` / `AIKIT_ERR_TOO_MANY` |
| `void removeHeader(name)` | 删除指定自定义头 |
| `void clearHeaders()` | 清空全部自定义头 |
| `size_t headerCount() const` | 当前数量（容量上限 `AIKIT_MAX_CUSTOM_HEADERS`，默认 8） |

## 自定义 JSON 字段

合并进最终请求 JSON——**没有专用 setter 的参数**（`tools`、`response_format`、厂商思考开关…）都从这里透传：

| 方法 | 说明 |
| --- | --- |
| `int addJsonField(key, jsonValue, replace = true)` | `jsonValue` 必须是**合法 JSON 值文本**（数字 / 带引号字符串 / 对象 / 数组 / `true`…）。`replace=true` 覆盖库生成的同名字段（可改写 `model`/`temperature`）；`replace=false` 仅在字段不存在时追加。设置时即校验合法性，非法返回 `AIKIT_ERR_BAD_JSON` |
| `addJsonField(key, float / int32_t / bool, ...)` | 数值 / 布尔便捷重载 |
| `int addJsonFieldString(key, strValue, replace = true)` | 普通文本便捷版：自动加引号并转义（适合自定义业务字段） |
| `void removeJsonField(key)` / `clearJsonFields()` | 删除 / 清空 |
| `size_t jsonFieldCount() const` | 当前数量（容量上限 `AIKIT_MAX_JSON_FIELDS`，默认 12） |

```cpp
ai.addJsonField("top_p", "0.9");
ai.addJsonField("enable_thinking", "false");
ai.addJsonField("tools", "[{\"type\":\"function\",...}]");   // Tool Calls 现阶段的透传用法
```

## 日志

| 方法 | 说明 |
| --- | --- |
| `setLogLevel(AIKitLogLevel)` / `logLevel()` | 级别：`AIKIT_LOG_NONE` / `ERROR` / `WARN` / `INFO` / `DEBUG`。传了日志口时默认 `INFO`，传 `nullptr` 时默认 `NONE` |

## 非流式对话（阻塞直至响应完成）

| 方法 | 说明 |
| --- | --- |
| `int chat(userText, String& outContent)` | 单条用户消息 → 回复正文 |
| `int chat(userText, AIKitChatResult& result)` | 单条用户消息 → 完整结果（正文 / 思维链 / finish_reason / usage，见下方结构体） |
| `int chat(messages, count, String& outContent)` | 多轮 / 自定义角色（`const AIKitMessage*` 数组；system 由 `setSystemPrompt` 自动前置，也可显式传入）。注意 `AIKitMessage` 只存指针，传入字符串须在 `chat()` 返回前保持有效 |
| `int chat(messages, count, AIKitChatResult& result)` | 同上的完整结果版本 |

`userText` / `messages` 各有 `const char*` 与 `String` 重载。**v1.2.0 起另有一组 `AIKitHistory` 重载**（自动入史），见下文。

## 流式入口（详见 [docs/04_streaming.md](04_streaming.md)）

| 方法 | 说明 |
| --- | --- |
| `void onDelta(cb)` | 增量正文回调 `void(const char* delta, size_t len)`（未解码 UTF-8 字节） |
| `void onReasoningDelta(cb)` | 推理模型思维链增量回调（`reasoning_content`；无则不触发） |
| `int chatStream(userText)` / `chatStream(messages, count)` | 流式方式 A：回调模式 |
| `int chatStreamTo(userText, AIKitStreamResponse&)` / `chatStreamTo(messages, count, resp)` | 流式方式 B：`Stream` 逐行读模式 |

## v1.2.0：Token 用量统计

流式请求默认附带 `stream_options.include_usage`（主流 OpenAI 兼容厂商均支持），使最后一个 chunk 携带 `usage` 字段；非流式响应本身自带。

| 方法 | 说明 |
| --- | --- |
| `void setIncludeUsage(bool)` / `bool includeUsage()` | 流式是否附带 `stream_options.include_usage`，**默认 `true`**。若个别厂商拒收（400），库会自动去掉该字段降级重发一次 |
| `AIKitUsage totalUsage() const` | 本客户端生命周期内的**累计**用量（跨请求求和，含流式与非流式） |
| `void resetTotalUsage()` | 累计值清零（如按会话 / 按天统计） |
| `AIKitUsage lastUsage() const` | 最近一次请求的用量（流式 / 非流式都记录） |

`AIKitUsage` 结构：`promptTokens` / `completionTokens` / `totalTokens`（`int32_t`），`valid()` 判断服务端是否返回（无则字段为 -1）。

## v1.2.0：失败自动重试

| 方法 | 说明 |
| --- | --- |
| `void setRetry(maxRetries, baseDelayMs = 1000, maxDelayMs = 30000)` | 最多重试 `maxRetries` 次；第 1 次重试前等待 `baseDelayMs`，其后每次翻倍（指数退避），封顶 `maxDelayMs` |
| `void setRetryEnabled(bool)` / `bool retryEnabled()` | 总开关（默认开）；`setRetryEnabled(false)` 完全关闭 |
| `uint8_t retryCount() const` | 当前配置的最大重试次数（默认 `AIKIT_DEFAULT_MAX_RETRIES` = 2） |
| `void onRetry(cb)` | 重试即将发生时的回调 `void(uint8_t attempt, int lastErr)`；**`attempt` 从 1 起计**，`lastErr` 为上一次错误码。可用于记录日志 / 点灯 / 上报 |

**触发条件**（满足其一，且总开关开启）：

| 条件 | 行为 |
| --- | --- |
| 连接 / 传输失败（`AIKIT_ERR_CONNECT`）、超时（`AIKIT_ERR_TIMEOUT`） | 正常重试 |
| HTTP `429 / 500 / 502 / 503 / 504` | 正常重试（429 限速时退避尤其有用） |
| HTTP 400 且错误体提到 `stream_options` / `include_usage` | 自动关闭 `include_usage` 后降级重发 |
| 其它错误（参数错 / 解析错 / 401 / 403…） | **不重试**——重试也无济于事 |

**两条安全边界**：

- 流式请求**已输出增量**（`onDelta` / `onReasoningDelta` 已触发过）→ 不再重试，避免重复输出；
- 逐行读模式（`chatStreamTo`）**从不自动重试**——已入队的行无法撤销。

## v1.2.0：配合 AIKitHistory 的对话重载

历史管理详见 [docs/06_history.md](06_history.md)：

| 方法 | 说明 |
| --- | --- |
| `int chat(AIKitHistory& history, userText, String& outContent)` | 自动流程：user 入史 → 携带全部历史请求 → 成功后回复入史；**请求失败则回滚 user 消息**（回复入史失败仅告警不报错） |
| `int chat(AIKitHistory& history, userText, AIKitChatResult& result)` | 同上的完整结果版本 |
| `int chatStream(AIKitHistory& history, userText)` | user 入史 → 流式请求；**回复正文需自行在 `onDelta` 中拼接后 `appendAssistant()` 入史**（库内无法自动获取完整正文） |

## 状态查询（请求结束后调用）

| 方法 | 说明 |
| --- | --- |
| `int lastStatusCode() const` | 最近一次 HTTP 状态码（0 = 未收到响应头） |
| `int lastTransportError() const` | 底层 StreamHTTPClient 错误码（`SHC_ERROR_*`，0 = 无；可配合 `StreamHTTPClient::errorToString()`） |
| `String lastErrorBody() const` | 非 2xx 时截取的响应体（默认 ≤384 字节，`AIKIT_ERRBODY_MAX` 可调） |
| `const char* lastFinishReason() const` | 最近一次 `finish_reason`（`"stop"` / `"length"` / …），流式与非流式都记录 |
| `AIKitUsage lastUsage() const` | 最近一次用量（见上文 v1.2.0 小节） |
| `bool streamActive() const` | 是否有流正在进行（期间新的 chat* 调用返回 `AIKIT_ERR_BUSY`） |

## 结果结构体（AIKitTypes.h）

| 结构 | 字段 |
| --- | --- |
| `AIKitChatResult` | `content`（回复正文）、`reasoningContent`（思维链，无则空）、`finishReason`、`usage`（`AIKitUsage`） |
| `AIKitMessage` | `{ const char* role; const char* content; }` —— 轻量指针视图，不拷贝字符串 |
| `AIKitUsage` | `promptTokens` / `completionTokens` / `totalTokens` + `valid()` |

## 示例

**Token 用量统计**（节选自 [examples/15_TokenUsage](../examples/15_TokenUsage/15_TokenUsage.ino)）：

```cpp
AIKitChatResult r1;
int err = ai.chat("用一个词形容 Arduino", r1);
if (err == AIKIT_OK) {
  Serial.println("[Reply] " + r1.content);
  if (r1.usage.valid()) {
    Serial.printf("prompt=%d completion=%d total=%d\n",
                  r1.usage.promptTokens, r1.usage.completionTokens,
                  r1.usage.totalTokens);
  }
}

// 流式：默认已开启 include_usage，结束后 lastUsage() 读取
ai.onDelta([](const char* delta, size_t len) { Serial.write(delta, len); });
err = ai.chatStream("数到五，用逗号分隔");
if (err == AIKIT_OK) {
  AIKitUsage u = ai.lastUsage();     // 末尾 chunk 携带的 usage
}

// 跨请求累计
printUsage("TOTAL", ai.totalUsage());   // 两次请求的 token 都已计入
ai.resetTotalUsage();                   // 需要时清零
```

**失败自动重试**（节选自 [examples/16_AutoRetry](../examples/16_AutoRetry/16_AutoRetry.ino)）：

```cpp
ai.setRetry(3, 1000, 8000);            // 最多 3 次，1s 起步指数退避，封顶 8s

ai.onRetry([](uint8_t attempt, int lastErr) {
  Serial.printf("[Retry] attempt %u, last error %d (%s)\n",
                attempt, lastErr, aikitErrorToString(lastErr));
});

String reply;
int err = ai.chat("hello", reply);     // 失败时自动按策略重试
Serial.printf("final error: %d (%s)\n", err, aikitErrorToString(err));
```

完整示例目录见仓库 `examples/`（共 21 个）；错误码全表见 [docs/07_advanced.md](07_advanced.md)。
