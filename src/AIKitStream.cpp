/*
 * AIKitStream.cpp - 流式响应读取与 SSE 行解析实现
 *
 * AIKit-Arduino (https://github.com/ioqite/AIKit-Arduino)
 */
#include "AIKitStream.h"

#include <ArduinoJson.h>

#if defined(ESP32)
  #include <freertos/FreeRTOS.h>
  #include <freertos/task.h>
#endif

#include "AIKitClient.h"

// ---------------------------------------------------------------------------
// 平台锁：ESP32 上生产者（FreeRTOS 任务）与消费者（用户 loop）并发访问
// 行环形缓冲，用 portMUX 临界区保护（持锁时间仅为一次 String 赋值，微秒级）；
// 其它平台请求在 chatStreamTo() 内同步执行完毕，生产与消费同上下文，无需加锁。
// ---------------------------------------------------------------------------
#if defined(ESP32)
    #define AIKIT_SR_ENTER() portENTER_CRITICAL(&_mux)
    #define AIKIT_SR_EXIT()  portEXIT_CRITICAL(&_mux)
#else
    #define AIKIT_SR_ENTER() do {} while (0)
    #define AIKIT_SR_EXIT()  do {} while (0)
#endif

// ===========================================================================
// AIKitStreamResponse
// ===========================================================================

AIKitStreamResponse::AIKitStreamResponse() : Stream() {}

AIKitStreamResponse::~AIKitStreamResponse() {
    // 用户未显式 close() 也安全：内部收尾（等待/取消后台请求并释放连接）
    if (!_closed) internalClose(2000);
}

void AIKitStreamResponse::beginRequest(AIKitClient* owner) {
    AIKIT_SR_ENTER();
    _owner = owner;
    _head = 0; _tail = 0; _count = 0; _bytes_buffered = 0;
    _completed = false;
    _http_status = 0;
    _transport_err = 0;
    _done_sentinel = false;
    _err = AIKIT_OK;
    _closed = false;
    _cur = ""; _cur_pos = 0; _cur_valid = false;
    // 清空槽位残留（复用对象时）
    for (size_t i = 0; i < kRingSlots; i++) _ring[i] = String();
    AIKIT_SR_EXIT();
}

bool AIKitStreamResponse::pushLineBlocking(const String& line) {
    if (_closed) return true;   // 消费者已提前关闭：静默丢弃，避免误报溢出

#if defined(ESP32)
    // 生产者运行在 StreamHTTPClient 的后台任务中；缓冲满时节流等待，
    // 由消费者读取形成背压。超时则置溢出错误并放弃该行（不崩溃）。
    uint32_t t0 = millis();
    while (true) {
        AIKIT_SR_ENTER();
        if (_count < kRingSlots) {
            _ring[_head] = line;
            _head = (_head + 1) % kRingSlots;
            ++_count;
            _bytes_buffered += line.length() + 1;
            AIKIT_SR_EXIT();
            return true;
        }
        AIKIT_SR_EXIT();
        if (_completed && _count >= kRingSlots) {
            // 消费者已放弃且请求仍在收尾：不再等待
            setError(AIKIT_ERR_BUFFER_OVERFLOW);
            return false;
        }
        if (millis() - t0 >= (uint32_t)AIKIT_LINE_PUSH_TIMEOUT_MS) {
            setError(AIKIT_ERR_BUFFER_OVERFLOW);
            return false;
        }
        vTaskDelay(1);
    }
#else
    // 单上下文平台：请求已在 chatStreamTo() 内执行完毕后才会被读取，
    // 无法形成背压，因此用「行数 + 字节」双重上限防止长回复耗尽内存。
    AIKIT_SR_ENTER();
    bool over = (_count >= kRingSlots) ||
                (_bytes_buffered + line.length() + 1 > (size_t)AIKIT_LINE_QUEUE_MAX_BYTES);
    if (!over) {
        _ring[_head] = line;
        _head = (_head + 1) % kRingSlots;
        ++_count;
        _bytes_buffered += line.length() + 1;
    }
    AIKIT_SR_EXIT();
    if (over) setError(AIKIT_ERR_BUFFER_OVERFLOW);
    return !over;
#endif
}

void AIKitStreamResponse::markComplete(int httpStatus, int transportErr) {
    AIKIT_SR_ENTER();
    _completed = true;
    if (httpStatus > 0) _http_status = httpStatus;
    if (transportErr < 0) _transport_err = transportErr;
    AIKIT_SR_EXIT();
}

bool AIKitStreamResponse::popLine(String& out) {
    AIKIT_SR_ENTER();
    if (_count == 0) {
        AIKIT_SR_EXIT();
        return false;
    }
    out = _ring[_tail];
    _ring[_tail] = String();   // 立即释放该槽堆内存
    _tail = (_tail + 1) % kRingSlots;
    --_count;
    _bytes_buffered -= (out.length() + 1);
    AIKIT_SR_EXIT();
    return true;
}

bool AIKitStreamResponse::complete() const {
    return _completed;
}

int AIKitStreamResponse::lastError() const {
    if (_err != AIKIT_OK) return _err;
    if (_transport_err != 0) return AIKIT_ERR_CONNECT;
    return AIKIT_OK;
}

size_t AIKitStreamResponse::linesBuffered() const {
    return _count;
}

// ---------------------------------------------------------------------------
// Stream 接口
// ---------------------------------------------------------------------------

int AIKitStreamResponse::available() {
    return (int)_bytes_buffered;
}

int AIKitStreamResponse::read() {
    while (true) {
        if (_cur_valid) {
            if (_cur_pos < _cur.length()) {
                return (uint8_t)_cur[(size_t)_cur_pos++];
            }
            // 当前行读完 → 输出虚拟换行，结束该行
            _cur_valid = false;
            _cur = "";
            _cur_pos = 0;
            return '\n';
        }
        String line;
        if (!popLine(line)) return -1;   // 无数据（非阻塞语义）
        _cur = line;
        _cur_pos = 0;
        _cur_valid = true;
    }
}

int AIKitStreamResponse::peek() {
    while (true) {
        if (_cur_valid) {
            if (_cur_pos < _cur.length()) return (uint8_t)_cur[_cur_pos];
            return '\n';                  // 行尾的虚拟换行
        }
        String line;
        if (!popLine(line)) return -1;
        _cur = line;
        _cur_pos = 0;
        _cur_valid = true;
    }
}

// ---------------------------------------------------------------------------
// 行读取 API
// ---------------------------------------------------------------------------

bool AIKitStreamResponse::readLineAsync(String& out) {
    return popLine(out);
}

bool AIKitStreamResponse::readLine(String& out) {
    while (true) {
        if (popLine(out)) return true;
        if (_closed) return false;

        if (_completed) {
            // 请求已结束且缓冲读尽 → 自动收尾（幂等）
            internalClose(10000);
            return false;
        }

#if defined(ESP32)
        // 后台任务仍在推进：等待新行
        delay(1);
#else
        // 非任务平台：chatStreamTo() 同步执行完毕后才可能到这里，
        // _completed 必已置位（上面的分支已处理）；兜底返回结束。
        return false;
#endif
    }
}

// ---------------------------------------------------------------------------
// 关闭
// ---------------------------------------------------------------------------

void AIKitStreamResponse::close() {
    if (_closed) return;
    internalClose(10000);
}

void AIKitStreamResponse::internalClose(uint32_t waitMs) {
    if (_closed) return;
    _closed = true;

    // 通知所有者结束底层请求（等待后台任务收尾、释放连接）
    if (_owner != nullptr) {
        AIKitClient* owner = _owner;
        _owner = nullptr;
        owner->finishStreamFromResponse(waitMs);
    }

    // 丢弃剩余缓冲，释放内存
    AIKIT_SR_ENTER();
    String tmp;
    while (_count > 0) {
        tmp = _ring[_tail];
        _ring[_tail] = String();
        _tail = (_tail + 1) % kRingSlots;
        --_count;
    }
    _bytes_buffered = 0;
    _cur = ""; _cur_pos = 0; _cur_valid = false;
    AIKIT_SR_EXIT();
}

// ---------------------------------------------------------------------------
// 静态帮助函数
// ---------------------------------------------------------------------------

bool AIKitStreamResponse::isDoneSentinel(const String& line) {
    if (!line.startsWith("data:")) return false;
    size_t i = 5;
    if (i < line.length() && line.charAt(i) == ' ') ++i;
    return line.substring(i) == "[DONE]";
}

bool AIKitStreamResponse::extractDelta(const String& line, String& outDelta,
                                        String* outReasoning) {
    if (!line.startsWith("data:")) return false;
    size_t i = 5;
    if (i < line.length() && line.charAt(i) == ' ') ++i;
    if (i >= line.length()) return false;
    const char* payload = line.c_str() + i;
    size_t payloadLen = line.length() - i;

    JsonDocument doc;
    if (deserializeJson(doc, payload, payloadLen)) return false;

    JsonVariantConst delta = doc["choices"][0]["delta"];
    if (delta.isNull()) return false;

    bool got = false;
    const char* content = delta["content"].as<const char*>();
    if (content != nullptr) {
        outDelta = content;   // ArduinoJson 反转义后的真实文本
        got = true;
    } else {
        outDelta = "";
    }
    if (outReasoning != nullptr) {
        const char* reasoning = delta["reasoning_content"].as<const char*>();
        if (reasoning != nullptr) {
            *outReasoning = reasoning;
        } else {
            *outReasoning = "";
        }
    }
    return got;
}

// ===========================================================================
// AIKitSSEParser
// ===========================================================================

void AIKitSSEParser::feed(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        char c = (char)data[i];
        if (c == '\n') {
            if (_overflow_skip > 0) {
                // 超长行的丢弃状态：在此换行处重新同步
                _overflow_skip = 0;
                _buf = "";
                continue;
            }
            handleLine(_buf);
            _buf = "";
        } else if (c == '\r') {
            continue;   // SSE 行以 \n 分隔，\r 忽略
        } else {
            if (_overflow_skip > 0) continue;
            if (_buf.length() >= (size_t)AIKIT_SSE_LINE_MAX) {
                // 行过长：置错误（可查询），丢弃该行剩余部分并重新同步
                _overflow_skip = 1;
                _buf = "";
                if (_resp != nullptr) _resp->setError(AIKIT_ERR_LINE_TOO_LONG);
                else if (_client != nullptr) _client->noteSSEError(AIKIT_ERR_LINE_TOO_LONG);
                continue;
            }
            _buf += c;
        }
    }
}

void AIKitSSEParser::finish() {
    if (_overflow_skip > 0) {
        _overflow_skip = 0;
        _buf = "";
        return;
    }
    if (_buf.length() > 0) {
        handleLine(_buf);
        _buf = "";
    }
}

void AIKitSSEParser::handleLine(String& line) {
    ++_lines;

    if (_resp != nullptr) {
        // ---- 行模式：原始行全部入队（含空行与注释行，忠实还原线路数据）----
        if (!_resp->_done_sentinel && AIKitStreamResponse::isDoneSentinel(line)) {
            _resp->markDoneSentinel();
        }
        _resp->pushLineBlocking(line);
        return;
    }

    // ---- 回调模式：解析 data: 行 ----
    if (line.length() == 0) return;          // 事件边界
    if (line.charAt(0) == ':') return;       // SSE 注释行
    if (!line.startsWith("data:")) return;   // 其余字段行（event:/id:/retry:）忽略

    size_t i = 5;
    if (i < line.length() && line.charAt(i) == ' ') ++i;
    if (i >= line.length()) return;
    const char* payload = line.c_str() + i;
    size_t payloadLen = line.length() - i;

    if (strcmp(payload, "[DONE]") == 0) {
        _done_seen = true;
        return;
    }

    if (_client == nullptr) return;

    JsonDocument doc;
    if (deserializeJson(doc, payload, payloadLen)) {
        // 单块解析失败：计数并跳过，不中断整个流
        _client->noteSSEParseSkip();
        return;
    }

    _client->handleChatChunk(doc);
}
