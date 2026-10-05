/*
 * AIKitTypes.h - 公共类型：错误码、日志级别、消息与结果结构
 *
 * AIKit-Arduino (https://github.com/ioqite/AIKit-Arduino)
 */
#pragma once

#include <Arduino.h>

// ---------------------------------------------------------------------------
// 错误码
//
// 约定：
//   AIKIT_OK(0)      —— 成功（HTTP 2xx 且解析正常）
//   负数 AIKIT_ERR_* —— 失败，可用 aikitErrorToString() 取可读描述
//
// 注意取值区间与 StreamHTTPClient 的 SHC_ERROR_*(-1..-20) 刻意错开，
// 便于在日志中区分：底层传输错误原样保存在 lastTransportError()。
// ---------------------------------------------------------------------------
enum AIKitError : int {
    AIKIT_OK                  = 0,     // 成功
    AIKIT_ERR_CONFIG          = -101,  // 未 begin() / 缺少 API Key / 端点未配置
    AIKIT_ERR_NULL_ARG        = -102,  // 传入空指针
    AIKIT_ERR_TOO_MANY        = -103,  // 自定义请求头 / JSON 字段超出容量
    AIKIT_ERR_BAD_JSON        = -104,  // 自定义 JSON 字段值不是合法 JSON
    AIKIT_ERR_CONNECT         = -105,  // 连接 / 发送失败（详见 lastTransportError()）
    AIKIT_ERR_HTTP_STATUS     = -106,  // HTTP 非 2xx（详见 lastStatusCode()/lastErrorBody()）
    AIKIT_ERR_PARSE           = -107,  // 响应 JSON 解析失败或结构不符
    AIKIT_ERR_OOM             = -108,  // 内存不足（JSON 构建溢出 / 任务创建失败）
    AIKIT_ERR_TIMEOUT         = -109,  // 超时
    AIKIT_ERR_BUSY            = -110,  // 已有请求或流正在进行
    AIKIT_ERR_LINE_TOO_LONG   = -111,  // SSE 单行超过 AIKIT_SSE_LINE_MAX
    AIKIT_ERR_BUFFER_OVERFLOW = -112,  // 逐行读缓冲溢出（详见 README 平台差异）
    AIKIT_ERR_STREAM_CLOSED   = -113,  // 流已关闭
    AIKIT_ERR_NOT_SUPPORTED   = -114,  // 当前平台或配置不支持该操作
};

// 返回可读的错误描述（静态字符串，不分配内存）
const char* aikitErrorToString(int err);

// ---------------------------------------------------------------------------
// 日志级别
// ---------------------------------------------------------------------------
enum AIKitLogLevel : uint8_t {
    AIKIT_LOG_NONE  = 0,
    AIKIT_LOG_ERROR = 1,
    AIKIT_LOG_WARN  = 2,
    AIKIT_LOG_INFO  = 3,
    AIKIT_LOG_DEBUG = 4,
};

// ---------------------------------------------------------------------------
// 一条对话消息（轻量：只保存指针，不拷贝字符串）
// role 取值与 OpenAI 兼容协议一致："system" / "user" / "assistant" / "tool"
// ---------------------------------------------------------------------------
struct AIKitMessage {
    const char* role;
    const char* content;
};

// ---------------------------------------------------------------------------
// 用量统计（模型最后一条 chunk 或非流式响应的 usage 字段；无则为 -1）
// ---------------------------------------------------------------------------
struct AIKitUsage {
    int32_t promptTokens;
    int32_t completionTokens;
    int32_t totalTokens;
    AIKitUsage() : promptTokens(-1), completionTokens(-1), totalTokens(-1) {}
    bool valid() const { return totalTokens >= 0; }
};

// ---------------------------------------------------------------------------
// 非流式补全的完整结果
// ---------------------------------------------------------------------------
struct AIKitChatResult {
    String  content;            // 模型回复正文
    String  reasoningContent;   // 推理模型思维链（无则为空）
    String  finishReason;       // "stop" / "length" / "content_filter" / ...
    AIKitUsage usage;           // token 用量（若服务端返回）
    AIKitChatResult() : finishReason("") {}
};
