/*
 * AIKitStream.h - 流式响应的逐行读取（AIKitStreamResponse）
 *                 与内部 SSE 行解析器（AIKitSSEParser）
 *
 * AIKitStreamResponse 继承 Arduino Stream：
 *   - 可用标准 Stream 用法（read / peek / available / readStringUntil ...）
 *   - 更常用的是 readLine()：逐行取出 SSE 原始行（如 "data: {...}"，不含换行）
 *   - extractDelta() 帮助函数可直接从一行中取出增量文本
 *
 * 平台差异（重要）：
 *   ESP32    ：请求由 StreamHTTPClient 的 FreeRTOS 任务在后台推进，
 *              readLine() 阻塞等待新行 —— 真·边收边读。
 *   ESP8266 等：请求在 chatStreamTo() 内同步执行完毕，行先入队（带上限）
 *              再供 readLine() 读出 —— 相同的用户代码，注意长回复的内存。
 *              （ESP8266 上需要真增量处理时请用 onDelta 回调模式。）
 *
 * AIKit-Arduino (https://github.com/ioqite/AIKit-Arduino)
 */
#pragma once

#include <Arduino.h>
#include <Stream.h>

#include "AIKitConfig.h"
#include "AIKitTypes.h"

class AIKitClient;

// ---------------------------------------------------------------------------
// AIKitStreamResponse - 流式响应读取器（用户侧对象，可复用）
// ---------------------------------------------------------------------------
class AIKitStreamResponse : public Stream {
public:
    AIKitStreamResponse();
    virtual ~AIKitStreamResponse();

    // ---- Stream 接口（非阻塞语义；行与行之间插入虚拟 '\n'）----
    int    available() override;
    int    read() override;
    int    peek() override;
    size_t write(uint8_t) override { return 0; }   // 只读流
    void   flush() override {}

    // ---- 行读取（推荐入口）----
    // 阻塞读取一行 SSE 原始文本（不含 CRLF）。
    // 返回 true：out 为新行；返回 false：流结束 / 出错 / 已关闭。
    bool readLine(String& out);
    // 非阻塞版本：当前无完整行立即返回 false（不等待）。
    bool readLineAsync(String& out);

    // ---- 状态查询 ----
    int    statusCode() const { return _http_status; }  // 0 = 尚未收到响应头
    bool   complete() const;        // 请求是否已结束（缓冲区可能仍有未读行）
    bool   doneSentinelSeen() const { return _done_sentinel; } // 是否收到 data:[DONE]
    int    lastError() const;       // AIKIT_ERR_*（AIKIT_OK = 正常）
    size_t linesBuffered() const;   // 当前缓冲的行数
    size_t bytesBuffered() const { return _bytes_buffered; }

    // ---- 结束流并释放底层连接（幂等；自然读尽时会自动调用）----
    void close();

    // ---- 静态帮助函数 ----
    // 判断一行是否为结束哨兵（"data: [DONE]"）
    static bool isDoneSentinel(const String& line);
    // 从 "data: {...}" 行提取增量文本；outReasoning 可为 nullptr。
    // 返回 false：该行不含可提取的 delta（或不是 data 行）。
    static bool extractDelta(const String& line, String& outDelta,
                             String* outReasoning = nullptr);

private:
    friend class AIKitClient;
    friend class AIKitSSEParser;

    // ---- 生产者接口（由 AIKitClient / SSE 解析器调用）----
    void beginRequest(AIKitClient* owner);
    // 压入一行（生产者上下文）。ESP32：缓冲满时阻塞等待（背压）；
    // 其它平台：超限即置溢出错误并丢弃（优雅降级）。返回 false = 已溢出。
    bool pushLineBlocking(const String& line);
    void markComplete(int httpStatus, int transportErr);
    void markDoneSentinel() { _done_sentinel = true; }
    void setError(int err) {
        if (_err == AIKIT_OK) _err = err;   // 记录首个错误
    }

    bool popLine(String& out);
    void internalClose(uint32_t waitMs);

private:
#if defined(ESP32)
    portMUX_TYPE _mux = portMUX_INITIALIZER_UNLOCKED;
    static const size_t kRingSlots = (size_t)AIKIT_LINE_RING_SLOTS;
#else
    // ESP8266 等平台：请求在 begin 阶段同步执行完，生产者与消费者同上下文，
    // 无需互斥；槽数更大以缓冲整个（有上限的）响应。
    static const size_t kRingSlots = (size_t)AIKIT_LINE_QUEUE_MAX_LINES;
#endif
    String  _ring[kRingSlots];
    size_t  _head = 0;            // 生产者写指针（模 kRingSlots）
    size_t  _tail = 0;            // 消费者读指针（模 kRingSlots）
    size_t  _count = 0;           // 当前缓冲行数
    size_t  _bytes_buffered = 0;  // 缓冲字节估计（含虚拟换行）

    AIKitClient* _owner = nullptr;
    volatile bool _completed = false;   // 请求已结束
    volatile int  _http_status = 0;
    volatile int  _transport_err = 0;
    volatile bool _done_sentinel = false;
    int  _err = AIKIT_OK;
    bool _closed = true;                // 初始即“未开始/已关闭”状态

    // read()/peek() 的当前行游标
    String _cur;
    size_t _cur_pos = 0;
    bool   _cur_valid = false;
};

// ---------------------------------------------------------------------------
// AIKitSSEParser - 内部 SSE 行解析器（不作为公开 API，仅为头文件可见性所需）
//
// 将 onData 送来的原始字节切成行：
//   - 行模式：全部行原样推给 AIKitStreamResponse
//   - 回调模式：解析 "data:" 负载 JSON，触发 onDelta / onReasoningDelta，
//     并记录 finish_reason / usage
// ---------------------------------------------------------------------------
class AIKitSSEParser {
public:
    AIKitSSEParser() {}

    void reset(AIKitClient* client, AIKitStreamResponse* resp) {
        _client = client;
        _resp = resp;
        _buf = "";
        _overflow_skip = 0;
        _done_seen = false;
        _lines = 0;
    }
    void clear() { reset(nullptr, nullptr); }

    void feed(const uint8_t* data, size_t len);
    void finish();                        // 请求体结束：冲刷最后一个未换行的残行

    bool doneSeen() const { return _done_seen; }
    size_t lineCount() const { return _lines; }

private:
    void handleLine(String& line);

    AIKitClient* _client = nullptr;       // 回调模式：取回调与记录字段
    AIKitStreamResponse* _resp = nullptr; // 行模式：行推送目标
    String _buf;
    size_t _overflow_skip = 0;            // >0：当前行超长，丢弃至下一换行
    bool   _done_seen = false;
    size_t _lines = 0;
};
