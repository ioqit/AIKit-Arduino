/*
 * AIKitClient.cpp - 主客户端实现
 *
 * AIKit-Arduino (https://github.com/ioqite/AIKit-Arduino)
 */
#include "AIKitClient.h"

// ===========================================================================
// 公共工具
// ===========================================================================

const char* aikitErrorToString(int err) {
    switch (err) {
        case AIKIT_OK:                  return "ok";
        case AIKIT_ERR_CONFIG:          return "not configured (call begin() first)";
        case AIKIT_ERR_NULL_ARG:        return "null argument";
        case AIKIT_ERR_TOO_MANY:        return "too many custom headers/fields";
        case AIKIT_ERR_BAD_JSON:        return "invalid JSON value";
        case AIKIT_ERR_CONNECT:         return "connection/send failed (see lastTransportError)";
        case AIKIT_ERR_HTTP_STATUS:     return "HTTP error status (see lastStatusCode/lastErrorBody)";
        case AIKIT_ERR_PARSE:           return "response JSON parse failed";
        case AIKIT_ERR_OOM:             return "out of memory";
        case AIKIT_ERR_TIMEOUT:         return "timeout";
        case AIKIT_ERR_BUSY:            return "busy: a request/stream is active";
        case AIKIT_ERR_LINE_TOO_LONG:   return "SSE line too long (AIKIT_SSE_LINE_MAX)";
        case AIKIT_ERR_BUFFER_OVERFLOW: return "line buffer overflow";
        case AIKIT_ERR_STREAM_CLOSED:   return "stream closed";
        case AIKIT_ERR_NOT_SUPPORTED:   return "not supported";
        case AIKIT_ERR_ASR_AUDIO:       return "invalid audio data/format";
        case AIKIT_ERR_ASR_PROTOCOL:    return "ASR response protocol error";
        case AIKIT_ERR_WS:              return "websocket connect/communication failed";
        case AIKIT_ERR_NOT_CONNECTED:   return "websocket not connected";
        case AIKIT_ERR_INFLATE:         return "gzip inflate failed";
        case AIKIT_ERR_HISTORY_LIMIT:   return "history capacity limit reached";
        case AIKIT_ERR_PAYLOAD_TOO_BIG: return "request body too large for this platform";
        default:                        return "unknown error";
    }
}

// ===========================================================================
// 生命周期
// ===========================================================================

AIKitClient::AIKitClient(Print* log)
    : _http(),
      _log(log),
      _log_level(log != nullptr ? AIKIT_LOG_INFO : AIKIT_LOG_NONE) {
}

AIKitClient::~AIKitClient() {
    // 防崩溃关键路径：销毁客户端前先斩断响应对象对本对象的反向引用，
    // 并释放后台请求（正常使用不会走到这里，仅为兜底）。
    if (_stream_active) {
        if (_active_resp != nullptr) {
            _active_resp->_closed = true;
            _active_resp->_owner = nullptr;
        }
        finishStreamFromResponse(1000);
    } else {
        _http.onData(nullptr);
        _http.onComplete(nullptr);
        _http.onError(nullptr);
        _http.end();
    }
#if defined(ESP8266)
    if (_x509_ta != nullptr) {
        delete _x509_ta;
        _x509_ta = nullptr;
    }
#endif
}

bool AIKitClient::begin(AIKitVendor vendor, const char* apiKey) {
    const AIKitVendorInfo* v = aikitVendorInfo((uint8_t)vendor);
    if (v == nullptr) {
        logE("unknown vendor id %u", (unsigned)vendor);
        return false;
    }
    // v1.2.0：本地部署厂商（llama.cpp / Ollama / vLLM / LM Studio）无鉴权，
    // 允许空 API Key；其余厂商仍然必填
    if (!v->noAuth && (apiKey == nullptr || apiKey[0] == '\0')) {
        logE("API key is empty");
        return false;
    }
    if (v->noAuth && (apiKey == nullptr || apiKey[0] == '\0')) {
        logI("local vendor without auth (no API key required)");
    }
    if (_stream_active) {
        logW("switching vendor while a stream is active, closing it");
        finishStreamFromResponse(2000);
    }

    bool vendorChanged = (_vendor_id != (uint8_t)vendor);
    _vendor_id = (uint8_t)vendor;
    _no_auth = v->noAuth;
    _host = v->host;
    _port = v->port;
    _path = v->path;
    _https = v->https;
    _api_key = (apiKey != nullptr) ? apiKey : "";
    if (vendorChanged || !_model_user_set || _model.isEmpty()) {
        _model = v->defaultModel;
        _model_user_set = false;
    }

    logI("AIKit v%s | vendor: %s | model: %s | endpoint: %s:%u%s",
         AIKIT_VERSION, v->vendorName, _model.c_str(),
         _host.c_str(), (unsigned)_port, _path.c_str());
    if (_https && !_ca_valid && _insecure) {
        logW("TLS certificate validation is DISABLED (insecure). "
             "For production, call setCACert() and setInsecure(false).");
    }
    return true;
}

bool AIKitClient::begin(AIKitVendor vendor, const String& apiKey) {
    return begin(vendor, apiKey.c_str());
}

bool AIKitClient::beginCustom(const char* host, uint16_t port, const char* path,
                              bool useHttps, const char* apiKey,
                              const char* defaultModel) {
    if (host == nullptr || path == nullptr || apiKey == nullptr) {
        logE("null endpoint argument");
        return false;
    }
    if (host[0] == '\0' || path[0] == '\0' || apiKey[0] == '\0') {
        logE("empty endpoint argument");
        return false;
    }
    if (port == 0) {
        logE("invalid port 0");
        return false;
    }
    if (_stream_active) {
        logW("reconfiguring while a stream is active, closing it");
        finishStreamFromResponse(2000);
    }

    _vendor_id = (uint8_t)AIKIT_CUSTOM;
    _host = host;
    _port = port;
    _path = path;
    _https = useHttps;
    _api_key = apiKey;
    if (defaultModel != nullptr && defaultModel[0] != '\0') {
        _model = defaultModel;
        _model_user_set = false;
    }

    logI("AIKit v%s | custom endpoint: %s://%s:%u%s | model: %s",
         AIKIT_VERSION, useHttps ? "https" : "http",
         _host.c_str(), (unsigned)_port, _path.c_str(), _model.c_str());
    if (_https && !_ca_valid && _insecure) {
        logW("TLS certificate validation is DISABLED (insecure). "
             "For production, call setCACert() and setInsecure(false).");
    }
    return true;
}

bool AIKitClient::beginCustom(const String& host, uint16_t port, const String& path,
                              bool useHttps, const String& apiKey,
                              const String& defaultModel) {
    return beginCustom(host.c_str(), port, path.c_str(), useHttps, apiKey.c_str(),
                       defaultModel.length() ? defaultModel.c_str() : nullptr);
}

void AIKitClient::end() {
    if (_stream_active) {
        finishStreamFromResponse(2000);
    } else {
        _http.onData(nullptr);
        _http.onComplete(nullptr);
        _http.onError(nullptr);
        _http.end();
        _parser.clear();
    }
}

bool AIKitClient::isReady() const {
    return (_no_auth || _api_key.length() > 0) && _host.length() > 0 &&
           _path.length() > 0 && _model.length() > 0;
}

const char* AIKitClient::vendorName() const {
    return aikitVendorName(_vendor_id);
}

// ===========================================================================
// 请求参数 setter
// ===========================================================================

void AIKitClient::setModel(const char* model) {
    if (model == nullptr || model[0] == '\0') {
        logW("setModel(nullptr) ignored");
        return;
    }
    _model = model;
    _model_user_set = true;
}

void AIKitClient::setModel(const String& model) {
    setModel(model.c_str());
}

void AIKitClient::setSystemPrompt(const char* sys) {
    _system_prompt = (sys != nullptr) ? sys : "";
}

void AIKitClient::setSystemPrompt(const String& sys) {
    _system_prompt = sys;
}

void AIKitClient::setTemperature(float t) {
    if (t < 0.0f || t > 2.0f) {
        logW("temperature %f out of [0,2], ignored", (double)t);
        return;
    }
    _has_temperature = true;
    _temperature = t;
}

void AIKitClient::setTopP(float p) {
    if (p <= 0.0f || p > 1.0f) {
        logW("top_p %f out of (0,1], ignored", (double)p);
        return;
    }
    _has_top_p = true;
    _top_p = p;
}

void AIKitClient::setMaxTokens(int32_t v) {
    if (v < 1) {
        logW("max_tokens %d invalid, ignored", (int)v);
        return;
    }
    _has_max_tokens = true;
    _max_tokens = v;
}

void AIKitClient::setMaxCompletionTokens(int32_t v) {
    if (v < 1) {
        logW("max_completion_tokens %d invalid, ignored", (int)v);
        return;
    }
    _has_max_completion_tokens = true;
    _max_completion_tokens = v;
}

void AIKitClient::setTimeouts(uint32_t connectMs, uint32_t tcpMs) {
    _connect_timeout_ms = (connectMs > 0) ? connectMs : AIKIT_DEFAULT_CONNECT_TIMEOUT_MS;
    _tcp_timeout_ms = (tcpMs > 0) ? tcpMs : AIKIT_DEFAULT_TCP_TIMEOUT_MS;
}

// ===========================================================================
// 自定义 HTTP 请求头
// ===========================================================================

int AIKitClient::addHeader(const char* name, const char* value, bool replace) {
    if (name == nullptr || value == nullptr) return AIKIT_ERR_NULL_ARG;
    if (name[0] == '\0') return AIKIT_ERR_NULL_ARG;

    if (replace) {
        // 覆盖：更新已有同名头（若无则新增）
        for (size_t i = 0; i < _header_count; i++) {
            if (_headers[i].name.equalsIgnoreCase(name)) {
                _headers[i].value = value;
                _headers[i].replace = true;
                return AIKIT_OK;
            }
        }
    }
    // 追加：允许同名多头（发送为重复行）
    if (_header_count >= AIKIT_MAX_CUSTOM_HEADERS) {
        logE("custom header limit reached (%d)", (int)AIKIT_MAX_CUSTOM_HEADERS);
        return AIKIT_ERR_TOO_MANY;
    }
    _headers[_header_count].name = name;
    _headers[_header_count].value = value;
    _headers[_header_count].replace = replace;
    _header_count++;
    return AIKIT_OK;
}

int AIKitClient::addHeader(const String& name, const String& value, bool replace) {
    return addHeader(name.c_str(), value.c_str(), replace);
}

void AIKitClient::removeHeader(const char* name) {
    if (name == nullptr) return;
    for (size_t i = 0; i < _header_count; ) {
        if (_headers[i].name.equalsIgnoreCase(name)) {
            // 用末尾元素填补（顺序无关紧要）
            _headers[i] = _headers[_header_count - 1];
            _header_count--;
        } else {
            i++;
        }
    }
}

// ===========================================================================
// 自定义 JSON 字段
// ===========================================================================

int AIKitClient::addJsonField(const char* key, const char* jsonValue, bool replace) {
    if (key == nullptr || jsonValue == nullptr) return AIKIT_ERR_NULL_ARG;
    if (key[0] == '\0' || jsonValue[0] == '\0') return AIKIT_ERR_BAD_JSON;

    // 预校验：非法 JSON 在设置时即报错，绝不在请求时崩溃
    {
        JsonDocument probe;
        if (deserializeJson(probe, jsonValue)) {
            logE("addJsonField('%s'): value is not valid JSON", key);
            return AIKIT_ERR_BAD_JSON;
        }
    }

    for (size_t i = 0; i < _field_count; i++) {
        if (_fields[i].key == key) {
            _fields[i].raw = jsonValue;
            _fields[i].replace = replace;
            return AIKIT_OK;
        }
    }
    if (_field_count >= AIKIT_MAX_JSON_FIELDS) {
        logE("custom JSON field limit reached (%d)", (int)AIKIT_MAX_JSON_FIELDS);
        return AIKIT_ERR_TOO_MANY;
    }
    _fields[_field_count].key = key;
    _fields[_field_count].raw = jsonValue;
    _fields[_field_count].replace = replace;
    _field_count++;
    return AIKIT_OK;
}

int AIKitClient::addJsonField(const char* key, const String& jsonValue, bool replace) {
    return addJsonField(key, jsonValue.c_str(), replace);
}

int AIKitClient::addJsonField(const char* key, float value, bool replace) {
    String raw(value, 4);              // "0.9000"
    // 去尾零更美观："0.9000"->"0.9"，"1.0000"->"1"
    if (raw.indexOf('.') >= 0) {
        while (raw.length() > 0 && raw.endsWith("0")) raw.remove(raw.length() - 1);
        if (raw.endsWith(".")) raw.remove(raw.length() - 1);
    }
    return addJsonField(key, raw.c_str(), replace);
}

int AIKitClient::addJsonField(const char* key, int32_t value, bool replace) {
    String raw(value);
    return addJsonField(key, raw.c_str(), replace);
}

int AIKitClient::addJsonField(const char* key, bool value, bool replace) {
    return addJsonField(key, value ? "true" : "false", replace);
}

int AIKitClient::addJsonFieldString(const char* key, const char* strValue, bool replace) {
    if (key == nullptr || strValue == nullptr) return AIKIT_ERR_NULL_ARG;
    // 借助 ArduinoJson 生成带引号且已转义的 JSON 字符串
    JsonDocument doc;
    doc.set(strValue);
    String raw;
    serializeJson(doc, raw);
    return addJsonField(key, raw.c_str(), replace);
}

void AIKitClient::removeJsonField(const char* key) {
    if (key == nullptr) return;
    for (size_t i = 0; i < _field_count; ) {
        if (_fields[i].key == key) {
            _fields[i] = _fields[_field_count - 1];
            _field_count--;
        } else {
            i++;
        }
    }
}

// ===========================================================================
// 状态查询
// ===========================================================================

String AIKitClient::lastErrorBody() const {
#if defined(ESP32)
    portENTER_CRITICAL(&_err_mux);
#endif
    String copy = _err_body;
#if defined(ESP32)
    portEXIT_CRITICAL(&_err_mux);
#endif
    return copy;
}

void AIKitClient::appendErrBody(const uint8_t* data, size_t len) {
    if (len == 0) return;
#if defined(ESP32)
    portENTER_CRITICAL(&_err_mux);
#endif
    if (_err_body.length() < (size_t)AIKIT_ERRBODY_MAX) {
        size_t room = (size_t)AIKIT_ERRBODY_MAX - _err_body.length();
        size_t take = (len < room) ? len : room;
        _err_body.concat((const char*)data, take);
    }
#if defined(ESP32)
    portEXIT_CRITICAL(&_err_mux);
#endif
}

// ===========================================================================
// 请求构建
// ===========================================================================

int AIKitClient::buildRequestBody(const AIKitMessage* messages, size_t count,
                                  bool stream, String& outBody) {
    if (messages == nullptr || count == 0) return AIKIT_ERR_NULL_ARG;
    for (size_t i = 0; i < count; i++) {
        if (messages[i].role == nullptr || messages[i].content == nullptr) {
            return AIKIT_ERR_NULL_ARG;
        }
    }

    JsonDocument doc;
    doc["model"] = _model;

    JsonArray msgs = doc["messages"].to<JsonArray>();
    bool hasOwnSystem =
        (count > 0 && strcmp(messages[0].role, "system") == 0);
    if (_system_prompt.length() > 0 && !hasOwnSystem) {
        JsonObject m = msgs.add<JsonObject>();
        m["role"] = "system";
        m["content"] = _system_prompt;
    }
    for (size_t i = 0; i < count; i++) {
        JsonObject m = msgs.add<JsonObject>();
        m["role"] = messages[i].role;
        m["content"] = messages[i].content;
    }

    if (_has_temperature)     doc["temperature"] = _temperature;
    if (_has_top_p)           doc["top_p"] = _top_p;
    if (_has_max_tokens)      doc["max_tokens"] = _max_tokens;
    if (_has_max_completion_tokens)
        doc["max_completion_tokens"] = _max_completion_tokens;
    if (stream) {
        doc["stream"] = true;
        // v1.2.0：请求流式 usage（末尾 chunk 携带 usage 字段）。
        // 主流 OpenAI 兼容厂商均支持；若个别厂商拒收（400 且错误体提到
        // stream_options / include_usage），重试管线会自动降级重发。
        if (_include_usage) {
            doc["stream_options"]["include_usage"] = true;
        }
    }

    // 自定义 JSON 字段：覆盖 / 追加
    for (size_t i = 0; i < _field_count; i++) {
        const CustomField& f = _fields[i];
        if (f.replace || doc[f.key.c_str()].isNull()) {
            doc[f.key.c_str()] = serialized(f.raw.c_str());
        }
    }

    if (doc.overflowed()) {
        logE("request JSON overflowed (messages too large?)");
        return AIKIT_ERR_OOM;
    }

    outBody = "";
    serializeJson(doc, outBody);
    if (outBody.length() == 0) {
        logE("request serialization produced empty body");
        return AIKIT_ERR_OOM;
    }
    return AIKIT_OK;
}

int AIKitClient::applyHttpConfig(bool stream, AIKitStreamResponse* resp) {
    (void)resp;

    // 关键防崩溃点：先清空上一请求的回调，避免陈旧回调触发悬垂对象
    _http.onData(nullptr);
    _http.onComplete(nullptr);
    _http.onError(nullptr);

    String url = _https ? "https://" : "http://";
    url += _host;
    if ((_https && _port != 443) || (!_https && _port != 80)) {
        url += ':';
        url += String((unsigned long)_port);
    }
    url += _path;

    if (!_http.begin(url)) {
        logE("endpoint URL invalid: %s", url.c_str());
        return AIKIT_ERR_CONFIG;
    }

    if (_https) {
        applySecureSettings(_http.getSecureClient());
    }

    _http.setConnectTimeout(_connect_timeout_ms);
    _http.setTcpTimeout(_tcp_timeout_ms);
    _http.setFollowRedirects(SHC_REDIRECT_OFF);   // JSON API 不跟随重定向

    if (_api_key.length() > 0) {
        String auth = "Bearer ";
        auth += _api_key;
        _http.setAuthorization(auth);
    }   // v1.2.0：无鉴权本地端点不发 Authorization 头

    _http.addHeader("Content-Type", "application/json");
    if (stream) {
        // SSE 流式：手动解析原始字节，禁用解压并声明 identity，
        // 避免服务端 gzip 压缩破坏行结构
        _http.addHeader("Accept", "text/event-stream");
        _http.useStreamMode(true);
        _http.setDecompress(false);
        _http.acceptEncoding("identity");
    } else {
        _http.useStreamMode(false);
        _http.setDecompress(true);
        _http.acceptEncoding("gzip, deflate");
    }

    // 自定义请求头（StreamHTTPClient 的 end() 会清空头部表，需每次重新应用）
    for (size_t i = 0; i < _header_count; i++) {
        _http.addHeader(_headers[i].name, _headers[i].value,
                        /*first=*/false, /*replace=*/_headers[i].replace);
    }

    // 统一数据泵：2xx 流式 → SSE 解析；非 2xx → 采集错误响应体
    _http.onData([this](uint8_t* data, size_t len) {
        int code = _http.getStatusCode();
        if (code >= 200 && code < 300) {
            if (_mode_stream) _parser.feed(data, len);
        } else {
            appendErrBody(data, len);
        }
    });

    if (resp != nullptr) {
        // 行模式：请求完成/出错时更新响应对象状态（同步/异步路径均触发）
        _http.onComplete([this](int code) {
            _parser.finish();          // 冲刷最后一个未换行的残行（异步路径的唯一时机）
            _http_status = code;
            if (_active_resp != nullptr) _active_resp->markComplete(code, 0);
        });
        _http.onError([this](int err) {
            _parser.finish();
            _transport_err = err;
            if (_active_resp != nullptr) {
                _active_resp->markComplete(_http.getStatusCode(), err);
            }
        });
    }
    return AIKIT_OK;
}

void AIKitClient::applySecureSettings(WiFiClientSecure* sec) {
    if (sec == nullptr) return;
    if (_ca_valid && _ca_cert != nullptr) {
#if defined(ESP8266)
        // BearSSL：PEM → X509List 信任锚（惰性解析一次并缓存；
        // ESP8266 的 flash 内存映射使 PROGMEM 指针可直接读取）
        if (_x509_ta == nullptr) {
            _x509_ta = new (std::nothrow) BearSSL::X509List(_ca_cert);
            if (_x509_ta == nullptr) {
                logE("OOM: cannot allocate trust anchor, falling back to strict mode");
                return;   // 不设置信任锚 → 严格校验下连接将失败（fail-closed）
            }
        }
        sec->setTrustAnchors(_x509_ta);
        logD("TLS: using custom root CA (BearSSL trust anchor)");
#else
        sec->setCACert(_ca_cert);
        logD("TLS: using custom root CA");
#endif
    } else if (_insecure) {
        sec->setInsecure();
        logD("TLS: insecure mode (validation disabled)");
    } else {
        // 未配置 CA 且未放行：保持严格校验，连接大概率失败（fail-closed）
        logW("TLS: strict mode without root CA - server cert will likely be rejected");
    }
}

// ===========================================================================
// 请求执行核心
// ===========================================================================

void AIKitClient::resetRequestState() {
    _http_status = 0;
    _transport_err = 0;
    _sse_err = AIKIT_OK;
    _parse_skips = 0;
    _mode_stream = false;
    _err_body = "";
    _response_body = "";
    _finish_reason = "";
    _usage = AIKitUsage();
    _stream_got_delta = false;
}

void AIKitClient::cleanupFailedRequest() {
    _http.onData(nullptr);
    _http.onComplete(nullptr);
    _http.onError(nullptr);
    _http.end();
    _parser.clear();
    _stream_active = false;
    _active_resp = nullptr;
}

int AIKitClient::performChat(const AIKitMessage* messages, size_t count,
                             bool stream, AIKitStreamResponse* resp) {
    if (_stream_active) {
        logW("busy: close the previous stream (resp.close()) first");
        return AIKIT_ERR_BUSY;
    }
    if (!isReady()) {
        logE("not configured: call begin(vendor, apiKey) first");
        return AIKIT_ERR_CONFIG;
    }

    resetRequestState();
    _mode_stream = stream;

    String body;
    int rc = buildRequestBody(messages, count, stream, body);
    if (rc != AIKIT_OK) {
        logE("request build failed: %s", aikitErrorToString(rc));
        return rc;
    }
    logD("POST %s://%s:%u%s | body: %u bytes",
         _https ? "https" : "http", _host.c_str(), (unsigned)_port,
         _path.c_str(), (unsigned)body.length());
    logD("body: %.*s", (int)(body.length() > 512 ? 512 : body.length()),
         body.c_str());

    rc = applyHttpConfig(stream, resp);
    if (rc != AIKIT_OK) {
        cleanupFailedRequest();
        return rc;
    }

    _parser.reset(this, resp);
    if (resp != nullptr) {
        resp->beginRequest(this);
        // 挂上活动流标记（同步路径的 onComplete 回调在 POST 内即会触发）
        _stream_active = true;
        _active_resp = resp;
    }

#if defined(ESP32)
    if (stream && resp != nullptr) {
        // ---- ESP32 行模式：FreeRTOS 后台任务推进，本调用立即返回 ----
        bool started = _http.asyncPOST(body);
        if (!started) {
            int aerr = _http.asyncLastError();
            logE("async request failed to start: %d", aerr);
            resp->markComplete(0, aerr);
            cleanupFailedRequest();
            if (aerr == SHC_ERROR_TASK_CREATE) return AIKIT_ERR_OOM;
            if (aerr == SHC_ERROR_ALREADY_RUNNING) return AIKIT_ERR_BUSY;
            return AIKIT_ERR_CONNECT;
        }
        logI("stream started (background task)");
        return AIKIT_OK;
    }
#endif

    // ---- 同步路径：非流式 / 回调流式 / 非 ESP32 的行模式 ----
    int code = _http.POST(body);
    _parser.finish();

    if (code < 0) {
        _transport_err = code;
        logE("transport error %d: %s", code,
             StreamHTTPClient::errorToString(code).c_str());
        if (resp != nullptr) resp->markComplete(0, code);
        cleanupFailedRequest();
        return AIKIT_ERR_CONNECT;
    }

    _http_status = code;
    if (resp != nullptr) resp->markComplete(code, 0);

    if (code < 200 || code >= 300) {
        logE("HTTP %d | error body: %s", code, _err_body.c_str());
        cleanupFailedRequest();
        return AIKIT_ERR_HTTP_STATUS;
    }

    if (!stream) {
        // ---- 非流式：取回完整响应体，解析交给 chat() ----
        _response_body = _http.getString();
        logD("response body: %u bytes", (unsigned)_response_body.length());
        cleanupFailedRequest();
        return AIKIT_OK;
    }

    if (resp == nullptr) {
        // ---- 回调流式：POST 返回即流结束 ----
        logI("stream finished: %u chunks | finish_reason: %s | parse skips: %u",
             (unsigned)_parser.lineCount(), _finish_reason.c_str(),
             (unsigned)_parse_skips);
        int ret = (_sse_err != AIKIT_OK) ? _sse_err : AIKIT_OK;
        cleanupFailedRequest();
        return ret;
    }

    // ---- 非 ESP32 的行模式：请求已同步完成，行已入队，交给用户读取 ----
    logI("stream completed: %u lines buffered; read with resp.readLine()",
         (unsigned)resp->linesBuffered());
    return AIKIT_OK;
}

// ===========================================================================
// 非流式对话
// ===========================================================================

int AIKitClient::chat(const char* userText, String& outContent) {
    outContent = "";
    AIKitChatResult result;
    int rc = chat(userText, result);
    outContent = result.content;
    return rc;
}

int AIKitClient::chat(const String& userText, String& outContent) {
    return chat(userText.c_str(), outContent);
}

int AIKitClient::chat(const char* userText, AIKitChatResult& result) {
    if (userText == nullptr) return AIKIT_ERR_NULL_ARG;
    AIKitMessage m = {"user", userText};
    return chat(&m, 1, result);
}

int AIKitClient::chat(const String& userText, AIKitChatResult& result) {
    return chat(userText.c_str(), result);
}

int AIKitClient::chat(const AIKitMessage* messages, size_t count, String& outContent) {
    outContent = "";
    AIKitChatResult result;
    int rc = chat(messages, count, result);
    outContent = result.content;
    return rc;
}

int AIKitClient::chat(const AIKitMessage* messages, size_t count, AIKitChatResult& result) {
    int rc = performChatWithRetry(messages, count, false, nullptr);
    if (rc != AIKIT_OK) return rc;
    int prc = parseChatResponse(_response_body, result);
    if (prc != AIKIT_OK) {
        logD("raw body head: %.*s",
             (int)(_response_body.length() > 256 ? 256 : _response_body.length()),
             _response_body.c_str());
        if (_err_body.length() == 0) {
            // 解析失败时保留响应体片段，便于排障
            _err_body = _response_body.substring(0, AIKIT_ERRBODY_MAX);
        }
    }
    // 及时释放完整响应体（对 ESP8266 的紧张堆更友好）
    _response_body = "";
    return prc;
}

int AIKitClient::parseChatResponse(const String& body, AIKitChatResult& result) {
    result.content = "";
    result.reasoningContent = "";
    result.finishReason = "";
    result.usage = AIKitUsage();

    if (body.length() == 0) {
        logE("empty response body");
        return AIKIT_ERR_PARSE;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err) {
        logE("response JSON parse failed: %s", err.c_str());
        return AIKIT_ERR_PARSE;
    }

    JsonVariantConst choice = doc["choices"][0];
    if (choice.isNull()) {
        logE("response has no choices[0]");
        return AIKIT_ERR_PARSE;
    }

    const char* content = choice["message"]["content"].as<const char*>();
    if (content != nullptr) result.content = content;

    const char* reasoning = choice["message"]["reasoning_content"].as<const char*>();
    if (reasoning != nullptr) result.reasoningContent = reasoning;

    const char* fr = choice["finish_reason"].as<const char*>();
    if (fr != nullptr) result.finishReason = fr;

    JsonVariantConst usage = doc["usage"];
    if (!usage.isNull()) {
        result.usage.promptTokens = usage["prompt_tokens"] | -1;
        result.usage.completionTokens = usage["completion_tokens"] | -1;
        result.usage.totalTokens = usage["total_tokens"] | -1;
    }
    _finish_reason = result.finishReason;
    _usage = result.usage;
    noteUsage(_usage);
    return AIKIT_OK;
}

// ===========================================================================
// v1.2.0：重试 / 用量统计 / 历史集成
// ===========================================================================

void AIKitClient::setRetry(uint8_t maxRetries, uint32_t baseDelayMs,
                           uint32_t maxDelayMs) {
    _retry_max = maxRetries;
    _retry_base_ms = baseDelayMs;
    _retry_cap_ms = maxDelayMs;
}

AIKitUsage AIKitClient::totalUsage() const {
    AIKitUsage u;
    if (_total_total > 0) {
        u.promptTokens = _total_prompt;
        u.completionTokens = _total_completion;
        u.totalTokens = _total_total;
    }
    return u;
}

void AIKitClient::resetTotalUsage() {
    _total_prompt = 0;
    _total_completion = 0;
    _total_total = 0;
}

void AIKitClient::noteUsage(const AIKitUsage& u) {
    if (u.promptTokens > 0)     _total_prompt += u.promptTokens;
    if (u.completionTokens > 0) _total_completion += u.completionTokens;
    if (u.totalTokens > 0)      _total_total += u.totalTokens;
}

bool AIKitClient::shouldRetry(int rc) const {
    if (!_retry_enabled || _retry_max == 0) return false;
    if (rc == AIKIT_ERR_CONNECT || rc == AIKIT_ERR_TIMEOUT) return true;
    if (rc == AIKIT_ERR_HTTP_STATUS) {
        int s = _http_status;
        if (s == 429 || s == 500 || s == 502 || s == 503 || s == 504) return true;
        // include_usage 降级重发：个别厂商对 stream_options 报 400
        if (s == 400 && (_err_body.indexOf("stream_options") >= 0 ||
                         _err_body.indexOf("include_usage") >= 0)) {
            return true;
        }
    }
    return false;
}

int AIKitClient::performChatWithRetry(const AIKitMessage* messages, size_t count,
                                      bool stream, AIKitStreamResponse* resp) {
    // 逐行读模式不自动重试：已入队的行无法撤销（文档说明）
    if (resp != nullptr) {
        return performChat(messages, count, stream, resp);
    }
    int rc = performChat(messages, count, stream, nullptr);
    uint8_t attempt = 0;
    while (rc != AIKIT_OK && attempt < _retry_max && shouldRetry(rc)) {
        // 流式已输出增量 → 不再重试（避免重复输出）
        if (stream && _stream_got_delta) {
            logW("stream already emitted deltas, skip retry");
            break;
        }
        // 400 + stream_options 拒收 → 关闭 include_usage 再试
        if (rc == AIKIT_ERR_HTTP_STATUS && _http_status == 400 && _include_usage) {
            logW("vendor rejected stream_options.include_usage, disable and retry");
            _include_usage = false;
        }
        uint32_t wait = _retry_base_ms << attempt;   // 指数退避
        if (wait > _retry_cap_ms || wait == 0) wait = _retry_cap_ms;
        logW("retry %u/%u after %ums (last error: %s)",
             (unsigned)(attempt + 1), (unsigned)_retry_max,
             (unsigned)wait, aikitErrorToString(rc));
        if (_on_retry) _on_retry(attempt + 1, rc);
        delay(wait);
        attempt++;
        rc = performChat(messages, count, stream, nullptr);
    }
    return rc;
}

// ---- 历史集成重载 ----

int AIKitClient::chat(AIKitHistory& history, const char* userText,
                       String& outContent) {
    outContent = "";
    AIKitChatResult result;
    int rc = chat(history, userText, result);
    outContent = result.content;
    return rc;
}

int AIKitClient::chat(AIKitHistory& history, const String& userText,
                       String& outContent) {
    return chat(history, userText.c_str(), outContent);
}

int AIKitClient::chat(AIKitHistory& history, const char* userText,
                       AIKitChatResult& result) {
    if (userText == nullptr) return AIKIT_ERR_NULL_ARG;
    int rc = history.appendUser(userText);
    if (rc != AIKIT_OK) {
        logE("history append failed: %s", aikitErrorToString(rc));
        return rc;
    }
    rc = chat(history.messages(), history.size(), result);
    if (rc == AIKIT_OK) {
        int hrc = history.appendAssistant(result.content);
        if (hrc != AIKIT_OK) {
            // 对话本身成功，仅历史入栈失败（可能触容量上限）：告警不报错
            logW("history append failed: %s", aikitErrorToString(hrc));
        }
    } else {
        history.popBack();   // 失败回滚本次 user 消息，保持历史一致
    }
    return rc;
}

int AIKitClient::chat(AIKitHistory& history, const String& userText,
                       AIKitChatResult& result) {
    return chat(history, userText.c_str(), result);
}

int AIKitClient::chatStream(AIKitHistory& history, const char* userText) {
    if (userText == nullptr) return AIKIT_ERR_NULL_ARG;
    int rc = history.appendUser(userText);
    if (rc != AIKIT_OK) {
        logE("history append failed: %s", aikitErrorToString(rc));
        return rc;
    }
    rc = chatStream(history.messages(), history.size());
    if (rc != AIKIT_OK) history.popBack();
    // 成功后：完整正文请在 onDelta 中拼接后 history.appendAssistant(...) 入史
    return rc;
}

int AIKitClient::chatStream(AIKitHistory& history, const String& userText) {
    return chatStream(history, userText.c_str());
}

// ===========================================================================
// 流式对话 A：回调
// ===========================================================================

int AIKitClient::chatStream(const char* userText) {
    if (userText == nullptr) return AIKIT_ERR_NULL_ARG;
    AIKitMessage m = {"user", userText};
    return chatStream(&m, 1);
}

int AIKitClient::chatStream(const String& userText) {
    return chatStream(userText.c_str());
}

int AIKitClient::chatStream(const AIKitMessage* messages, size_t count) {
    int rc = performChatWithRetry(messages, count, true, nullptr);
    if (rc == AIKIT_OK && _sse_err != AIKIT_OK) rc = _sse_err;
    if (rc == AIKIT_OK && _parse_skips > 0) {
        logW("%u chunks could not be parsed and were skipped", (unsigned)_parse_skips);
    }
    return rc;
}

// ===========================================================================
// 流式对话 B：逐行读
// ===========================================================================

int AIKitClient::chatStreamTo(const char* userText, AIKitStreamResponse& resp) {
    if (userText == nullptr) return AIKIT_ERR_NULL_ARG;
    AIKitMessage m = {"user", userText};
    return chatStreamTo(&m, 1, resp);
}

int AIKitClient::chatStreamTo(const String& userText, AIKitStreamResponse& resp) {
    return chatStreamTo(userText.c_str(), resp);
}

int AIKitClient::chatStreamTo(const AIKitMessage* messages, size_t count,
                              AIKitStreamResponse& resp) {
    return performChat(messages, count, true, &resp);
}

// ===========================================================================
// 流式内部回调
// ===========================================================================

void AIKitClient::handleChatChunk(JsonVariantConst root) {
    JsonVariantConst choice = root["choices"][0];
    if (!choice.isNull()) {
        JsonVariantConst delta = choice["delta"];
        if (!delta.isNull()) {
            const char* reasoning = delta["reasoning_content"].as<const char*>();
            if (reasoning != nullptr && _on_reasoning_delta) {
                _stream_got_delta = true;
                _on_reasoning_delta(reasoning, strlen(reasoning));
            }
            const char* content = delta["content"].as<const char*>();
            if (content != nullptr && _on_delta) {
                _stream_got_delta = true;
                _on_delta(content, strlen(content));
            }
        }
        const char* fr = choice["finish_reason"].as<const char*>();
        if (fr != nullptr && _finish_reason.length() == 0) _finish_reason = fr;
    }
    JsonVariantConst usage = root["usage"];
    if (!usage.isNull()) {
        _usage.promptTokens = usage["prompt_tokens"] | -1;
        _usage.completionTokens = usage["completion_tokens"] | -1;
        _usage.totalTokens = usage["total_tokens"] | -1;
        noteUsage(_usage);
    }
}

void AIKitClient::finishStreamFromResponse(uint32_t waitMs) {
    if (!_stream_active) return;

#if defined(ESP32)
    // 等待后台任务收尾（onComplete 在任务内触发；_running 置 false 稍早于
    // 任务自删除，等待可消除对任务句柄操作的竞态窗口）
    uint32_t t0 = millis();
    while (_http.isAsyncRunning()) {
        if (millis() - t0 > waitMs) {
            logW("background task still running after %ums, cancelling", (unsigned)waitMs);
            _http.asyncStop();
            delay(5);
            break;
        }
        delay(1);
    }
    delay(5);
#endif

    _http.onData(nullptr);
    _http.onComplete(nullptr);
    _http.onError(nullptr);
    _http.end();
    _parser.clear();
    _stream_active = false;
    _active_resp = nullptr;
}

// ===========================================================================
// 日志
// ===========================================================================

void AIKitClient::logv(AIKitLogLevel lvl, const char* tag, const char* fmt, ...) {
    if (_log == nullptr || _log_level == AIKIT_LOG_NONE || _log_level < lvl) return;
    va_list ap;
    va_start(ap, fmt);
    logRaw(lvl, tag, fmt, ap);
    va_end(ap);
}

void AIKitClient::logRaw(AIKitLogLevel lvl, const char* tag, const char* fmt, va_list ap) {
    (void)lvl;
    if (_log == nullptr || fmt == nullptr) return;

    char buf[AIKIT_LOG_BUF];
    buf[0] = '\0';
    vsnprintf(buf, sizeof(buf), fmt, ap);

    _log->print("[AIKit][");
    _log->print(tag);
    _log->print("] ");
    _log->println(buf);
}
