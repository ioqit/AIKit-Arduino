# 04 - Two Ways to Read a Streaming Response

English | **[简体中文](../04_streaming.md)**

The SSE streaming formats of all vendor presets are mutually compatible (`data: {json}` lines + the `data: [DONE]` sentinel), and this library offers two ways to consume them: **mode A** registers a callback to receive increments, **mode B** reads the response as a `Stream`, line by line. Both expose exactly the same raw SSE lines and HTTP status / error information — pick whichever fits your needs.

| | Mode A: onDelta callback | Mode B: Stream line-by-line reading |
| --- | --- | --- |
| Entry point | `chatStream(userText)` | `chatStreamTo(userText, resp)` |
| Amount of code | Minimal (one lambda) | Slightly more (an explicit loop) |
| What you get | Parsed incremental text / chain-of-thought | Raw SSE lines (everything — `usage`, comment lines and all) |
| Platform behavior | **Identical on both platforms** | Platform-dependent (see table below) |
| Best for | Printing to screen / per-chunk processing; **first choice for long replies on ESP8266** | Custom parsing (getting usage / finish_reason), doing other work while reading |

## Mode A: onDelta callback

```cpp
ai.onDelta([](const char* delta, size_t len) {
  Serial.write((const uint8_t*)delta, len);   // typewriter effect
});
int err = ai.chatStream("Tell me a story");   // callbacks fire throughout the call; return = stream over
```

- The callback receives an **undecoded UTF-8 byte stream plus its length** (one CJK character spans multiple bytes); a plain `write()` renders it correctly on screen;
- For reasoning models (DeepSeek-R series / GLM thinking / Kimi thinking) you can additionally register `onReasoningDelta` to receive `reasoning_content` chain-of-thought increments (it never fires when the model outputs none):

```cpp
ai.onReasoningDelta([](const char* d, size_t n) {
  Serial.printf("[thinking] %.*s", (int)n, d);
});
```

- Callbacks keep firing while the request runs, with identical behavior on both platforms — **the recommended way to handle long replies on ESP8266** (process while receiving; the full body is never held in memory);
- After the stream ends, `lastFinishReason()` / `lastUsage()` (v1.2.0, see below) can be queried directly.

Full examples: [examples/02_ChatStreamCallback](../../examples/02_ChatStreamCallback/02_ChatStreamCallback.ino), chain-of-thought in [examples/11_ReasoningDelta](../../examples/11_ReasoningDelta/11_ReasoningDelta.ino).

## Mode B: line-by-line reading via AIKitStreamResponse

```cpp
AIKitStreamResponse resp;                        // user-side object, reusable
int err = ai.chatStreamTo("Write a four-line poem about the ESP32", resp);
if (err != AIKIT_OK && err != AIKIT_ERR_HTTP_STATUS) return;

String line, piece;
while (resp.readLine(line)) {                    // blocks until a new line / stream end
  if (AIKitStreamResponse::isDoneSentinel(line)) break;   // "data: [DONE]"
  if (AIKitStreamResponse::extractDelta(line, piece)) {
    Serial.print(piece);                        // convenient delta-text extraction
    continue;
  }
  // Other lines (comments / blank lines / the final chunk carrying usage) can be
  // parsed yourself with ArduinoJson:
  //   JsonDocument doc; deserializeJson(doc, line.c_str() + 6);
}
resp.close();                                    // releases the underlying connection (auto-called when drained; idempotent)
```

`AIKitStreamResponse` derives from the Arduino `Stream` class, so all the standard Stream idioms work (`available()` / `read()` / `peek()` / `readStringUntil()`…, with a virtual `'\n'` inserted between lines; the stream is read-only — `write()` returns 0).

### AIKitStreamResponse API ([src/AIKitStream.h](../../src/AIKitStream.h))

| Member | Description |
| --- | --- |
| `bool readLine(String& out)` | **Recommended entry point**: blocking read of one raw SSE line (e.g. `data: {...}`, without CRLF); returns `false` = stream ended / error / already closed |
| `bool readLineAsync(String& out)` | Non-blocking variant: returns `false` immediately when no complete line is available |
| `static bool isDoneSentinel(const String& line)` | Whether the line is the end sentinel (`data: [DONE]`) |
| `static bool extractDelta(line, outDelta, outReasoning = nullptr)` | Extracts the incremental body from a `data: {...}` line (optionally also the chain-of-thought); returns `false` for non-data lines or when there is nothing to extract |
| `int statusCode() const` | HTTP status code (0 = response headers not yet received) |
| `bool complete() const` | Whether the request has finished (unread lines may still be buffered) |
| `bool doneSentinelSeen() const` | Whether `[DONE]` was ever seen |
| `int lastError() const` | `AIKIT_ERR_*` (`AIKIT_OK` = clean) |
| `size_t linesBuffered() const` / `bytesBuffered() const` | Number of lines / bytes currently buffered |
| `void close()` | Ends the stream and releases the underlying connection (idempotent) |

Full example: [examples/05_ChatStreamLineReader](../../examples/05_ChatStreamLineReader/05_ChatStreamLineReader.ino).

## Platform differences (important)

The differences stem from the execution model of the StreamHTTPClient transport layer:

| | ESP32 | ESP8266 etc. |
| --- | --- | --- |
| Request execution | Driven by a FreeRTOS background task; `chatStreamTo()` **returns immediately** | Runs to completion **synchronously inside** `chatStreamTo()` |
| Reading experience | `readLine()` truly reads while receiving (ring buffer + backpressure; the producer throttles and waits when the consumer is slow) | Lines are **fully queued first** (with caps) and then read out by `readLine()` |
| Long replies | Unlimited (within available RAM) | Bounded by the queue caps; **switch to mode A**, or raise the build macros |
| Related macros | `AIKIT_LINE_RING_SLOTS` (default 16 slots), `AIKIT_LINE_PUSH_TIMEOUT_MS` (default 30 s) | `AIKIT_LINE_QUEUE_MAX_LINES` (default 96 lines), `AIKIT_LINE_QUEUE_MAX_BYTES` (default 32 KB) |

Two safety guarantees common to both platforms:

- Overflow (an overlong single line / queue overflow) **never crashes** — `AIKIT_ERR_LINE_TOO_LONG` / `AIKIT_ERR_BUFFER_OVERFLOW` is set and subsequent lines are dropped (graceful degradation), queryable via `resp.lastError()`;
- After an overlong line is dropped, the parser automatically resynchronizes at the next newline and the stream continues.

## v1.2.0: Streaming usage

Streaming requests carry `stream_options.include_usage` by default (`setIncludeUsage(true)` is the default; see [03_chat.md](03_chat.md)), and mainstream OpenAI-compatible vendors return the `usage` field in the **final chunk**:

```cpp
ai.onDelta([](const char* d, size_t n) { Serial.write(d, n); });
int err = ai.chatStream("Count to five");
if (err == AIKIT_OK) {
  AIKitUsage u = ai.lastUsage();          // prompt / completion / total tokens
  if (u.valid()) Serial.printf("tokens: %d\n", u.totalTokens);
}
```

- **Automatic degradation**: when a vendor rejects `stream_options` with a 400, the library drops the field and resends once — no manual intervention (the switch stays off thereafter);
- In mode B you can equally parse the trailing data line yourself to get `usage` / `finish_reason`;
- For cumulative totals across requests use `totalUsage()` (same usage as [examples/15_TokenUsage](../../examples/15_TokenUsage/15_TokenUsage.ino)).

## Choosing between the modes

| Scenario | Recommendation |
| --- | --- |
| ESP32 / ESP8266, plain screen output, typewriter effect | Mode A |
| **Long replies on ESP8266** (tight memory) | **Mode A** (emit while receiving, nothing resident in memory), plus `setMaxTokens()` to bound the length |
| Need the raw JSON of every chunk (usage / tool_calls deltas) | Mode B |
| On ESP32, streaming while doing heavy work (request in the background) | Mode B |

> The line-by-line reading of mode B **does not participate in automatic retry** (queued lines cannot be undone); see [03_chat.md](03_chat.md) for the retry mechanism. The full error-code table is in [07_advanced.md](07_advanced.md).
