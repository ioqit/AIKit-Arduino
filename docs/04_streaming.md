# 04 - 流式响应的两种读取模式

**[English](../en/04_streaming.md)** | 简体中文

所有厂商预设的 SSE 流式格式彼此兼容（`data: {json}` 行 + `data: [DONE]` 哨兵），本库提供两种读取方式：**方式 A** 注册回调拿增量，**方式 B** 把响应当作 `Stream` 逐行读。两种方式暴露的原始 SSE 行与 HTTP 状态 / 错误信息完全一致，按需选择即可。

| | 方式 A：onDelta 回调 | 方式 B：Stream 逐行读 |
| --- | --- | --- |
| 入口 | `chatStream(userText)` | `chatStreamTo(userText, resp)` |
| 代码量 | 最少（一个 lambda） | 稍多（显式循环） |
| 拿到什么 | 已解析的增量文本 / 思维链 | 原始 SSE 行（含 `usage`、注释行等一切） |
| 平台行为 | **两个平台完全一致** | 有平台差异（见下表） |
| 适合场景 | 上屏 / 逐块处理；**ESP8266 长回复首选** | 需要自定义解析（拿到 usage / finish_reason）、边读边做别的 |

## 方式 A：onDelta 回调

```cpp
ai.onDelta([](const char* delta, size_t len) {
  Serial.write((const uint8_t*)delta, len);   // 打字机效果
});
int err = ai.chatStream("给我讲个故事");        // 调用期间持续触发回调；返回即流结束
```

- 回调参数是**未解码的 UTF-8 字节流与长度**（中文一个字占多字节），直接 `write()` 即可正确上屏；
- 推理模型（DeepSeek-R 系 / GLM thinking / Kimi thinking）另可注册 `onReasoningDelta`，接收 `reasoning_content` 思维链增量（模型不输出则不触发）：

```cpp
ai.onReasoningDelta([](const char* d, size_t n) {
  Serial.printf("[thinking] %.*s", (int)n, d);
});
```

- 请求执行期间回调持续触发，两个平台行为一致——**ESP8266 处理长回复的推荐方式**（边收边处理，内存里不留整段正文）；
- 流结束后 `lastFinishReason()` / `lastUsage()`（v1.2.0，见下文）可直接查询。

完整示例：[examples/02_ChatStreamCallback](../examples/02_ChatStreamCallback/02_ChatStreamCallback.ino)、思维链 [examples/11_ReasoningDelta](../examples/11_ReasoningDelta/11_ReasoningDelta.ino)。

## 方式 B：AIKitStreamResponse 逐行读

```cpp
AIKitStreamResponse resp;                        // 用户侧对象，可复用
int err = ai.chatStreamTo("写一首关于 ESP32 的四行小诗", resp);
if (err != AIKIT_OK && err != AIKIT_ERR_HTTP_STATUS) return;

String line, piece;
while (resp.readLine(line)) {                    // 阻塞直至新行 / 流结束
  if (AIKitStreamResponse::isDoneSentinel(line)) break;   // "data: [DONE]"
  if (AIKitStreamResponse::extractDelta(line, piece)) {
    Serial.print(piece);                        // 便捷提取增量文本
    continue;
  }
  // 其它行（注释 / 空行 / 含 usage 的末块）可自行用 ArduinoJson 解析：
  //   JsonDocument doc; deserializeJson(doc, line.c_str() + 6);
}
resp.close();                                    // 释放底层连接（读尽时自动调用，幂等）
```

`AIKitStreamResponse` 继承自 Arduino `Stream`，因此标准 Stream 用法全部可用（`available()` / `read()` / `peek()` / `readStringUntil()`…，行与行之间插入虚拟 `'\n'`；只读流，`write()` 返回 0）。

### AIKitStreamResponse API（[src/AIKitStream.h](../src/AIKitStream.h)）

| 成员 | 说明 |
| --- | --- |
| `bool readLine(String& out)` | **推荐入口**：阻塞读取一行 SSE 原始文本（如 `data: {...}`，不含 CRLF）；返回 `false` = 流结束 / 出错 / 已关闭 |
| `bool readLineAsync(String& out)` | 非阻塞版本：当前无完整行立即返回 `false` |
| `static bool isDoneSentinel(const String& line)` | 该行是否为结束哨兵（`data: [DONE]`） |
| `static bool extractDelta(line, outDelta, outReasoning = nullptr)` | 从 `data: {...}` 行提取增量正文（可选同时取思维链）；非 data 行或无可提取内容返回 `false` |
| `int statusCode() const` | HTTP 状态码（0 = 尚未收到响应头） |
| `bool complete() const` | 请求是否已结束（缓冲区可能仍有未读行） |
| `bool doneSentinelSeen() const` | 是否收到过 `[DONE]` |
| `int lastError() const` | `AIKIT_ERR_*`（`AIKIT_OK` = 正常） |
| `size_t linesBuffered() const` / `bytesBuffered() const` | 当前缓冲的行数 / 字节数 |
| `void close()` | 结束流并释放底层连接（幂等） |

完整示例：[examples/05_ChatStreamLineReader](../examples/05_ChatStreamLineReader/05_ChatStreamLineReader.ino)。

## 平台差异（重要）

差异来自传输层 StreamHTTPClient 的执行模型：

| | ESP32 | ESP8266 等 |
| --- | --- | --- |
| 请求执行 | FreeRTOS 后台任务推进，`chatStreamTo()` **立即返回** | 在 `chatStreamTo()` **内同步执行完毕** |
| 读取体验 | `readLine()` 真·边收边读（环形缓冲 + 背压，消费者慢时生产者节流等待） | 行先**全部入队**（带上限）再由 `readLine()` 读出 |
| 长回复 | 无限制（RAM 允许范围内） | 受队列上限约束；**建议改用方式 A**，或调大构建宏 |
| 相关宏 | `AIKIT_LINE_RING_SLOTS`（默认 16 槽）、`AIKIT_LINE_PUSH_TIMEOUT_MS`（默认 30 s） | `AIKIT_LINE_QUEUE_MAX_LINES`（默认 96 行）、`AIKIT_LINE_QUEUE_MAX_BYTES`（默认 32 KB） |

两条共同的安全保证：

- 超限（单行超长 / 队列溢出）**从不崩溃**——置 `AIKIT_ERR_LINE_TOO_LONG` / `AIKIT_ERR_BUFFER_OVERFLOW` 并丢弃后续行（优雅降级），可经 `resp.lastError()` 查询；
- 单行超长被丢弃后，解析器在下一个换行处自动重新同步，流继续。

## v1.2.0：流式 usage

流式请求默认附带 `stream_options.include_usage`（`setIncludeUsage(true)` 为默认，详见 [docs/03_chat.md](03_chat.md)），主流 OpenAI 兼容厂商会在**最后一个 chunk** 返回 `usage` 字段：

```cpp
ai.onDelta([](const char* d, size_t n) { Serial.write(d, n); });
int err = ai.chatStream("数到五");
if (err == AIKIT_OK) {
  AIKitUsage u = ai.lastUsage();          // prompt / completion / total tokens
  if (u.valid()) Serial.printf("tokens: %d\n", u.totalTokens);
}
```

- **自动降级**：个别厂商对 `stream_options` 报 400 时，库会自动去掉该字段并重发一次，无需手工干预（该开关随之保持关闭）；
- 方式 B 中也可以自己解析末尾的 data 行拿到 `usage` / `finish_reason`；
- 跨请求累计请用 `totalUsage()`（与 [examples/15_TokenUsage](../examples/15_TokenUsage/15_TokenUsage.ino) 相同用法）。

## 选型建议

| 场景 | 推荐 |
| --- | --- |
| ESP32 / ESP8266 单纯上屏、打字机效果 | 方式 A |
| **ESP8266 长回复**（内存紧张） | **方式 A**（边收边出，不驻留内存），并 `setMaxTokens()` 限制长度 |
| 需要每个 chunk 的原始 JSON（拿 usage / tool_calls 增量） | 方式 B |
| ESP32 上边流式边做重活（请求后台化） | 方式 B |

> 方式 B 的逐行读模式**不参与自动重试**（已入队的行无法撤销），重试机制详见 [docs/03_chat.md](03_chat.md)。错误码全表见 [docs/07_advanced.md](07_advanced.md)。
