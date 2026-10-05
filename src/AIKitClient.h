/*
 * AIKitClient.h - 主客户端（begin + setter 风格）
 *
 * 一句话上手：
 *   AIKitClient ai(&Serial);
 *   ai.begin(AIKIT_DEEPSEEK, "sk-...");
 *   String reply; ai.chat("你好", reply);
 *
 * 详见 README（中英双语）与 examples/。
 *
 * AIKit-Arduino (https://github.com/ioqite/AIKit-Arduino)
 */
#pragma once

#include <Arduino.h>
#include <stdarg.h>
#include <functional>

#include <ArduinoJson.h>
#include <StreamHTTPClient.h>
#if defined(ESP8266)
  #include <BearSSLHelpers.h>   // BearSSL::X509List（根证书信任锚）
#endif

#include "AIKitConfig.h"
#include "AIKitTypes.h"
#include "AIKitVendors.h"
#include "AIKitStream.h"

class AIKitClient {
public:
    // log：日志输出目标（任意 Print/Stream，如 &Serial、&Serial0），
    //      传 nullptr 完全静默。日志对象需在 AIKitClient 生命周期内有效。
    explicit AIKitClient(Print* log = nullptr);
    ~AIKitClient();

    // -------------------------------------------------------------------
    // 生命周期
    // -------------------------------------------------------------------
    // 选择内置厂商预设并配置 API Key（一键接入）。
    // 可重复调用以切换厂商；用户 setModel() 过的模型在切厂商时会重置为
    // 新厂商默认模型。返回 false：参数非法。
    bool begin(AIKitVendor vendor, const char* apiKey);
    bool begin(AIKitVendor vendor, const String& apiKey);

    // 接入任意 OpenAI 兼容端点（自建网关 / vLLM / Ollama / OneAPI 等）。
    bool beginCustom(const char* host, uint16_t port, const char* path,
                     bool useHttps, const char* apiKey,
                     const char* defaultModel = nullptr);
    bool beginCustom(const String& host, uint16_t port, const String& path,
                     bool useHttps, const String& apiKey,
                     const String& defaultModel = String());

    // 结束当前请求/流并释放连接（保留全部配置，可再次 chat）。
    void end();

    // 是否已具备发起请求的条件
    bool isReady() const;

    // -------------------------------------------------------------------
    // 厂商 / 模型
    // -------------------------------------------------------------------
    AIKitVendor vendor() const { return (AIKitVendor)_vendor_id; }
    const char* vendorName() const;
    const char* model() const { return _model.c_str(); }
    void setModel(const char* model);
    void setModel(const String& model);

    // -------------------------------------------------------------------
    // 请求参数（setter，任意时刻可改，下一次请求生效）
    // -------------------------------------------------------------------
    void setSystemPrompt(const char* sys);       // nullptr/"" 等效清除
    void setSystemPrompt(const String& sys);
    void clearSystemPrompt() { _system_prompt = ""; }
    const char* systemPrompt() const { return _system_prompt.c_str(); }

    void setTemperature(float t);                // 越界值视为清除
    void setTopP(float p);
    void setMaxTokens(int32_t v);                // 写入 JSON 的 max_tokens
    void setMaxCompletionTokens(int32_t v);      // 写入 max_completion_tokens
                                                 // （Moonshot/OpenAI 新规范，
                                                 //  老参数已弃用）
    void setTimeouts(uint32_t connectMs, uint32_t tcpMs);   // 默认 10s / 30s

    // -------------------------------------------------------------------
    // TLS
    // -------------------------------------------------------------------
    // 跳过证书校验开关（默认 true —— 便于无 NTP/无根证书环境快速上手；
    // 生产环境请务必 setCACert() 并 setInsecure(false)！）。
    void setInsecure(bool yes) { _insecure = yes; }
    bool insecure() const { return _insecure; }

    // 自定义根 CA（PEM 文本）。指针须在请求期间保持有效。
    // ESP8266：库会在首次请求时把 PEM 解析为 BearSSL::X509List 并缓存；
    // 若证书存于 PROGMEM，请改用 setCACert_P(PSTR(...))。
    void setCACert(const char* pem) {
        _ca_cert = pem;
        _ca_valid = (pem != nullptr);
#if defined(ESP8266)
        _ca_progmem = false;   // 从 PROGMEM 版本切回普通版本时复位标志
        if (_x509_ta != nullptr) { delete _x509_ta; _x509_ta = nullptr; }
#endif
    }
#if defined(ESP8266)
    void setCACert_P(PGM_P pem) {
        _ca_cert = pem;
        _ca_progmem = true;
        _ca_valid = (pem != nullptr);
        if (_x509_ta != nullptr) { delete _x509_ta; _x509_ta = nullptr; }
    }
#endif

    // -------------------------------------------------------------------
    // 自定义 HTTP 请求头（随每次请求发送）
    // -------------------------------------------------------------------
    // replace=true（默认）：同名头覆盖为单行；
    // replace=false：同名头追加为重复行（如需要多值头的场景）。
    // 注意：Content-Type / Accept 亦可被覆盖（在库默认头之后写入）。
    // 返回 AIKIT_OK / AIKIT_ERR_NULL_ARG / AIKIT_ERR_TOO_MANY。
    int addHeader(const char* name, const char* value, bool replace = true);
    int addHeader(const String& name, const String& value, bool replace = true);
    void removeHeader(const char* name);
    void clearHeaders() { _header_count = 0; }
    size_t headerCount() const { return _header_count; }

    // -------------------------------------------------------------------
    // 自定义 JSON 字段（合并进最终请求 JSON，支持覆盖/追加）
    // -------------------------------------------------------------------
    // jsonValue 必须是合法的 JSON 值文本（数字/带引号字符串/对象/数组/true...）。
    // replace=true：覆盖库已生成的同名字段（如改写 model/temperature）；
    // replace=false：仅当字段不存在时追加（不覆盖既有参数）。
    // 例：ai.addJsonField("top_p", "0.9");
    //     ai.addJsonField("enable_thinking", "false");
    //     ai.addJsonField("tools", "[{\"type\":\"function\",...}]");
    int addJsonField(const char* key, const char* jsonValue, bool replace = true);
    int addJsonField(const char* key, const String& jsonValue, bool replace = true);
    int addJsonField(const char* key, float value, bool replace = true);
    int addJsonField(const char* key, int32_t value, bool replace = true);
    int addJsonField(const char* key, bool value, bool replace = true);
    // 便捷版本：value 为普通文本，自动加引号并转义（如自定义业务字段）
    int addJsonFieldString(const char* key, const char* strValue, bool replace = true);
    void removeJsonField(const char* key);
    void clearJsonFields() { _field_count = 0; }
    size_t jsonFieldCount() const { return _field_count; }

    // -------------------------------------------------------------------
    // 日志
    // -------------------------------------------------------------------
    void setLogLevel(AIKitLogLevel lvl) { _log_level = lvl; }
    AIKitLogLevel logLevel() const { return _log_level; }

    // -------------------------------------------------------------------
    // 非流式对话（阻塞直至响应完成）
    // -------------------------------------------------------------------
    // 最简：单条用户消息 → 回复正文
    int chat(const char* userText, String& outContent);
    int chat(const String& userText, String& outContent);
    // 单条用户消息 → 完整结果（含 finish_reason / usage / reasoning）
    int chat(const char* userText, AIKitChatResult& result);
    int chat(const String& userText, AIKitChatResult& result);
    // 多轮 / 自定义角色消息（system 由 setSystemPrompt 自动前置，也可显式传入）
    int chat(const AIKitMessage* messages, size_t count, String& outContent);
    int chat(const AIKitMessage* messages, size_t count, AIKitChatResult& result);

    // -------------------------------------------------------------------
    // 流式对话 A：onDelta 回调（真·增量，两个平台行为一致）
    // -------------------------------------------------------------------
    // 增量正文回调（每收到一块触发一次，参数为未解码 UTF-8 字节与长度）
    void onDelta(std::function<void(const char* delta, size_t len)> cb) { _on_delta = cb; }
    // 推理模型思维链增量回调（reasoning_content，如 DeepSeek-R 系列 /
    // GLM thinking / kimi thinking；无则不触发）
    void onReasoningDelta(std::function<void(const char* delta, size_t len)> cb) {
        _on_reasoning_delta = cb;
    }

    int chatStream(const char* userText);
    int chatStream(const String& userText);
    int chatStream(const AIKitMessage* messages, size_t count);

    // -------------------------------------------------------------------
    // 流式对话 B：Stream 逐行读（AIKitStreamResponse 继承自 Stream）
    // -------------------------------------------------------------------
    // 读取方式见 AIKitStreamResponse::readLine / extractDelta。
    // ESP32：请求后台任务推进，readLine() 边收边读；
    // ESP8266 等：请求在本调用内同步完成，行入队（带上限）后读出。
    int chatStreamTo(const char* userText, AIKitStreamResponse& resp);
    int chatStreamTo(const String& userText, AIKitStreamResponse& resp);
    int chatStreamTo(const AIKitMessage* messages, size_t count, AIKitStreamResponse& resp);

    // -------------------------------------------------------------------
    // 最近一次请求的状态（请求结束后查询）
    // -------------------------------------------------------------------
    int        lastStatusCode() const { return _http_status; }       // HTTP 状态码，0=未收到
    int        lastTransportError() const { return _transport_err; } // SHC_ERROR_*（0=无）
    String     lastErrorBody() const;                                // 非 2xx 时截取的响应体
    const char* lastFinishReason() const { return _finish_reason.c_str(); }
    AIKitUsage lastUsage() const { return _usage; }
    bool       streamActive() const { return _stream_active; }

private:
    friend class AIKitSSEParser;
    friend class AIKitStreamResponse;

    // ---- 内部：被流式解析器 / 响应对象回调 ----
    void finishStreamFromResponse(uint32_t waitMs);   // resp.close() 入口
    void noteSSEError(int err) { if (_sse_err == AIKIT_OK) _sse_err = err; }
    void noteSSEParseSkip() { ++_parse_skips; }
    void handleChatChunk(JsonVariantConst root);     // 回调模式：解析一个 data 块

    // ---- 内部：请求管线 ----
    int  performChat(const AIKitMessage* messages, size_t count,
                     bool stream, AIKitStreamResponse* resp);
    int  buildRequestBody(const AIKitMessage* messages, size_t count,
                          bool stream, String& outBody);
    int  applyHttpConfig(bool stream, AIKitStreamResponse* resp);
    void applySecureSettings(WiFiClientSecure* sec);
    int  parseChatResponse(const String& body, AIKitChatResult& result);
    void resetRequestState();
    void appendErrBody(const uint8_t* data, size_t len);
    void cleanupFailedRequest();

    // ---- 日志 ----
    void logv(AIKitLogLevel lvl, const char* tag, const char* fmt, ...);
    void logE(const char* fmt, ...) __attribute__((format(printf, 2, 3)));
    void logW(const char* fmt, ...) __attribute__((format(printf, 2, 3)));
    void logI(const char* fmt, ...) __attribute__((format(printf, 2, 3)));
    void logD(const char* fmt, ...) __attribute__((format(printf, 2, 3)));
    void logRaw(AIKitLogLevel lvl, const char* tag, const char* fmt, va_list ap);

private:
    StreamHTTPClient _http;
    Print*           _log;
    AIKitLogLevel    _log_level;

    // ---- 端点配置 ----
    uint8_t  _vendor_id = (uint8_t)AIKIT_CUSTOM;
    String   _host;
    uint16_t _port = 443;
    String   _path;
    bool     _https = true;
    String   _api_key;
    bool     _model_user_set = false;
    String   _model;

    // ---- 请求参数 ----
    String   _system_prompt;
    bool     _has_temperature = false;
    float    _temperature = 1.0f;
    bool     _has_top_p = false;
    float    _top_p = 1.0f;
    bool     _has_max_tokens = false;
    int32_t  _max_tokens = 0;
    bool     _has_max_completion_tokens = false;
    int32_t  _max_completion_tokens = 0;
    uint32_t _connect_timeout_ms = AIKIT_DEFAULT_CONNECT_TIMEOUT_MS;
    uint32_t _tcp_timeout_ms = AIKIT_DEFAULT_TCP_TIMEOUT_MS;

    // ---- TLS ----
    bool         _insecure = true;      // 默认放行（文档显著提示生产环境改 CA）
    const char*  _ca_cert = nullptr;    // 用户保证生命周期
    bool         _ca_valid = false;
#if defined(ESP8266)
    bool         _ca_progmem = false;
    BearSSL::X509List* _x509_ta = nullptr;   // 惰性解析并缓存的信任锚
#endif

    // ---- 自定义请求头 ----
    struct CustomHeader { String name; String value; bool replace; };
    CustomHeader _headers[AIKIT_MAX_CUSTOM_HEADERS];
    size_t       _header_count = 0;

    // ---- 自定义 JSON 字段 ----
    struct CustomField { String key; String raw; bool replace; };
    CustomField _fields[AIKIT_MAX_JSON_FIELDS];
    size_t      _field_count = 0;

    // ---- 回调 ----
    std::function<void(const char*, size_t)> _on_delta;
    std::function<void(const char*, size_t)> _on_reasoning_delta;

    // ---- 每请求状态 ----
    AIKitSSEParser        _parser;
    bool                  _stream_active = false;
    AIKitStreamResponse*  _active_resp = nullptr;
    bool                  _mode_stream = false;
    int                   _http_status = 0;
    int                   _transport_err = 0;
    int                   _sse_err = AIKIT_OK;
    size_t                _parse_skips = 0;
    String                _err_body;
    String                _response_body;
    String                _finish_reason;
    AIKitUsage            _usage;

#if defined(ESP32)
    mutable portMUX_TYPE  _err_mux = portMUX_INITIALIZER_UNLOCKED;  // _err_body 跨核保护
#endif
};

// ---------------------------------------------------------------------------
// 日志辅助：转发可变参数到 logRaw（编译器可内联）
// ---------------------------------------------------------------------------
#define AIKIT_LOG_FORWARD(LEVEL, TAG)                              \
    va_list ap;                                                    \
    if (_log == nullptr || _log_level == AIKIT_LOG_NONE ||         \
        _log_level < (LEVEL)) {                                    \
        return;                                                    \
    }                                                              \
    va_start(ap, fmt);                                             \
    logRaw(LEVEL, TAG, fmt, ap);                                   \
    va_end(ap);

inline void AIKitClient::logE(const char* fmt, ...) { AIKIT_LOG_FORWARD(AIKIT_LOG_ERROR, "E") }
inline void AIKitClient::logW(const char* fmt, ...) { AIKIT_LOG_FORWARD(AIKIT_LOG_WARN,  "W") }
inline void AIKitClient::logI(const char* fmt, ...) { AIKIT_LOG_FORWARD(AIKIT_LOG_INFO,  "I") }
inline void AIKitClient::logD(const char* fmt, ...) { AIKIT_LOG_FORWARD(AIKIT_LOG_DEBUG, "D") }
