/*
 * AIKitRealtimeASR.cpp - 实时语音识别实现（v1.2.0）
 *
 * 各厂商协议（2026-10 官方文档核实；详见文件头注释与 docs/05_asr.md）：
 *   VOLC      wss://openspeech.bytedance.com/api/v3/sauc/bigmodel
 *             二进制帧协议（4B header + seq + size + payload），响应 gzip
 *   DASHSCOPE wss://dashscope.aliyuncs.com/api-ws/v1/inference
 *             run-task / result-generated / finish-task（JSON 文本帧）
 *   BAIDU     wss://vop.baidu.com/realtime_asr?sn=...
 *             START / MID_TEXT / FIN_TEXT / HEARTBEAT（JSON 文本帧 + 裸 PCM）
 *   FUNASR    ws://host:10095  配置 JSON 首包 + 裸 PCM + {"is_speaking":false}
 *   OPENAI    wss://api.openai.com/v1/realtime?intent=transcription
 *             session.update → input_audio_buffer.append(base64 PCM24k) → commit
 *   STEPFUN   wss://api.stepfun.com/v1/realtime/asr/stream（OpenAI 同构）
 *   MISTRAL   wss://api.mistral.ai/v1/audio/transcriptions/realtime?model=...
 *             （实验性：上行事件按 OpenAI 同构实现，未经真机验证）
 *
 * AIKit-Arduino (https://github.com/ioqit/AIKit-Arduino)
 */
#include "AIKitRealtimeASR.h"

#if !AIKIT_HAS_RTASR
// ===========================================================================
// 桩实现：未安装 ArduinoWebsockets 库时，全部功能返回 NOT_SUPPORTED
// ===========================================================================

AIKitRealtimeASR::AIKitRealtimeASR(Print* log)
    : _log(log), _log_level(log != nullptr ? AIKIT_LOG_WARN : AIKIT_LOG_NONE) {}

AIKitRealtimeASR::~AIKitRealtimeASR() { close(); }

bool AIKitRealtimeASR::begin(AIKitRTVendor vendor, const char* key1, const char* key2) {
    (void)vendor; (void)key1; (void)key2;
    if (_log) _log->println("[RTASR][E] ArduinoWebsockets library not installed");
    return false;
}
bool AIKitRealtimeASR::begin(AIKitRTVendor vendor, const String& key1, const String& key2) {
    return begin(vendor, key1.c_str(), key2.c_str());
}
bool AIKitRealtimeASR::beginVolc(const char* appId, const char* accessToken,
                                 const char* resourceId) {
    (void)appId; (void)accessToken; (void)resourceId;
    return false;
}
bool AIKitRealtimeASR::beginBaidu(const char* appId, const char* appKey) {
    (void)appId; (void)appKey;
    return false;
}
bool AIKitRealtimeASR::beginFunASR(const char* host, uint16_t port, bool useTls) {
    (void)host; (void)port; (void)useTls;
    return false;
}
void AIKitRealtimeASR::setModel(const char* model) { (void)model; }
void AIKitRealtimeASR::setModel(const String& model) { (void)model; }
void AIKitRealtimeASR::setLanguage(const char* lang) { (void)lang; }
int  AIKitRealtimeASR::connect()    { return AIKIT_ERR_NOT_SUPPORTED; }
int  AIKitRealtimeASR::sendAudio(const uint8_t* pcm, size_t len) {
    (void)pcm; (void)len;
    return AIKIT_ERR_NOT_SUPPORTED;
}
int  AIKitRealtimeASR::commitTurn() { return AIKIT_ERR_NOT_SUPPORTED; }
int  AIKitRealtimeASR::finish(uint32_t waitMs) { (void)waitMs; return AIKIT_ERR_NOT_SUPPORTED; }
void AIKitRealtimeASR::loop() {}
void AIKitRealtimeASR::close() {}
const char* AIKitRealtimeASR::vendorName() const { return "None"; }

// 私有方法桩（不被调用，仅满足链接）
String AIKitRealtimeASR::buildStartUrl() { return String(); }
bool   AIKitRealtimeASR::applyAuthHeaders() { return false; }
bool   AIKitRealtimeASR::sendStartCommand() { return false; }
int    AIKitRealtimeASR::sendAudioBinary(const uint8_t* pcm, size_t len) {
    (void)pcm; (void)len; return AIKIT_ERR_NOT_SUPPORTED;
}
int    AIKitRealtimeASR::sendAudioVolc(const uint8_t* pcm, size_t len, bool lastPacket) {
    (void)pcm; (void)len; (void)lastPacket; return AIKIT_ERR_NOT_SUPPORTED;
}
int    AIKitRealtimeASR::sendAudioB64(const uint8_t* pcm, size_t len) {
    (void)pcm; (void)len; return AIKIT_ERR_NOT_SUPPORTED;
}
void   AIKitRealtimeASR::sendFinishCommand() {}
void   AIKitRealtimeASR::pump(uint32_t waitMs, bool untilFinished) {
    (void)waitMs; (void)untilFinished;
}
void   AIKitRealtimeASR::handleMessageText(const char* data, size_t len) {
    (void)data; (void)len;
}
void   AIKitRealtimeASR::handleMessageBinary(const uint8_t* data, size_t len) {
    (void)data; (void)len;
}
void   AIKitRealtimeASR::handleVolcBinary(const uint8_t* data, size_t len) {
    (void)data; (void)len;
}
void   AIKitRealtimeASR::emitPartial(const char* text) { (void)text; }
void   AIKitRealtimeASR::emitFinal(const char* text) { (void)text; }
void   AIKitRealtimeASR::emitError(int err, const char* msg) { (void)err; (void)msg; }
bool   AIKitRealtimeASR::gunzipToString(const uint8_t* in, size_t len, String& out) {
    (void)in; (void)len; (void)out; return false;
}
void   AIKitRealtimeASR::logE(const char* fmt, ...) { (void)fmt; }
void   AIKitRealtimeASR::logI(const char* fmt, ...) { (void)fmt; }

#else // AIKIT_HAS_RTASR =====================================================

#include <ArduinoJson.h>
#include <ArduinoWebsockets.h>
#include <StreamHTTPClient_GzipDecompressor.h>

#if defined(ESP32)
#  if __has_include(<esp_random.h>)
#    include <esp_random.h>
#  else
#    include <esp_system.h>
#  endif
#endif

using websockets::WebsocketsClient;
using websockets::WebsocketsEvent;
using websockets::WebsocketsMessage;

// ===========================================================================
// 内部工具
// ===========================================================================

namespace {

const char* rtVendorName(uint8_t id) {
    switch (id) {
        case AIKIT_RT_VOLC:      return "Volcengine (Seed-ASR)";
        case AIKIT_RT_DASHSCOPE: return "DashScope Realtime";
        case AIKIT_RT_BAIDU:     return "Baidu Realtime ASR";
        case AIKIT_RT_FUNASR:    return "FunASR (self-hosted)";
        case AIKIT_RT_OPENAI:    return "OpenAI Realtime Transcription";
        case AIKIT_RT_STEPFUN:   return "StepFun Realtime ASR";
        case AIKIT_RT_MISTRAL:   return "Mistral Realtime (experimental)";
        default:                 return "Custom";
    }
}

uint32_t rtRand() {
#if defined(ESP32)
    return esp_random();
#else
    return ((uint32_t)rand() << 16) ^ (uint32_t)micros() ^ ((uint32_t)millis() << 8);
#endif
}

// 生成 UUID 形状的唯一 id（DashScope task_id / 百度 sn / 设备 cuid）
void genId(char* out, size_t cap) {
    uint32_t r[5];
    for (auto& v : r) v = rtRand();
    snprintf(out, cap, "%08x-%04x-4%03x-%04x-%04x%08x",
             r[0], r[1] & 0xFFFF, r[2] & 0xFFF,
             (r[3] & 0x3FFF) | 0x8000, r[4] & 0xFFFF, rtRand());
}

uint32_t rdBE32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}
void wrBE32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);  p[3] = (uint8_t)v;
}

const char kB64Chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

size_t rtB64Encode(const uint8_t* in, size_t n, char* out) {
    size_t o = 0;
    for (size_t i = 0; i < n; i += 3) {
        uint32_t v = (uint32_t)in[i] << 16;
        if (i + 1 < n) v |= (uint32_t)in[i + 1] << 8;
        if (i + 2 < n) v |= (uint32_t)in[i + 2];
        out[o++] = kB64Chars[(v >> 18) & 0x3F];
        out[o++] = kB64Chars[(v >> 12) & 0x3F];
        out[o++] = (i + 1 < n) ? kB64Chars[(v >> 6) & 0x3F] : '=';
        out[o++] = (i + 2 < n) ? kB64Chars[v & 0x3F] : '=';
    }
    out[o] = '\0';
    return o;
}

void rtLog(Print* log, const char* tag, const char* msg) {
    if (log == nullptr) return;
    log->print("[RTASR][");
    log->print(tag);
    log->print("] ");
    log->print(msg);
    log->print("\r\n");
}

} // namespace

// ===========================================================================
// 接入
// ===========================================================================

AIKitRealtimeASR::AIKitRealtimeASR(Print* log)
    : _log(log), _log_level(log != nullptr ? AIKIT_LOG_INFO : AIKIT_LOG_NONE) {}

AIKitRealtimeASR::~AIKitRealtimeASR() { close(); }

bool AIKitRealtimeASR::begin(AIKitRTVendor vendor, const char* key1, const char* key2) {
    if ((uint8_t)vendor >= AIKIT_RT_COUNT) {
        logE("unknown realtime vendor id");
        return false;
    }
    if (vendor == AIKIT_RT_FUNASR) {
        logE("use beginFunASR(host, port) for FunASR");
        return false;
    }
    if (key1 == nullptr || key1[0] == '\0') {
        logE("key1 (API key / appId) is empty");
        return false;
    }
    bool needKey2 = (vendor == AIKIT_RT_VOLC || vendor == AIKIT_RT_BAIDU);
    if (needKey2 && (key2 == nullptr || key2[0] == '\0')) {
        logE("key2 (access token / appkey) required for this vendor");
        return false;
    }
    _vendor = (uint8_t)vendor;
    _key1 = key1;
    _key2 = (key2 != nullptr) ? key2 : "";
    logI("realtime ASR ready");
    return true;
}

bool AIKitRealtimeASR::begin(AIKitRTVendor vendor, const String& key1, const String& key2) {
    const char* k2 = key2.length() > 0 ? key2.c_str() : nullptr;
    return begin(vendor, key1.c_str(), k2);
}

bool AIKitRealtimeASR::beginVolc(const char* appId, const char* accessToken,
                                 const char* resourceId) {
    if (appId == nullptr || accessToken == nullptr ||
        appId[0] == '\0' || accessToken[0] == '\0') {
        return false;
    }
    _vendor = (uint8_t)AIKIT_RT_VOLC;
    _key1 = appId;
    _key2 = accessToken;
    if (resourceId != nullptr && resourceId[0] != '\0') {
        _volc_resource = resourceId;
    }
    return true;
}

bool AIKitRealtimeASR::beginBaidu(const char* appId, const char* appKey) {
    if (appId == nullptr || appKey == nullptr ||
        appId[0] == '\0' || appKey[0] == '\0') {
        return false;
    }
    _vendor = (uint8_t)AIKIT_RT_BAIDU;
    _key1 = appId;
    _key2 = appKey;
    return true;
}

bool AIKitRealtimeASR::beginFunASR(const char* host, uint16_t port, bool useTls) {
    if (host == nullptr || host[0] == '\0') return false;
    _vendor = (uint8_t)AIKIT_RT_FUNASR;
    _funasr_host = host;
    _funasr_port = port;
    _funasr_tls = useTls;
    _key1 = "";
    _key2 = "";
    return true;
}

void AIKitRealtimeASR::setModel(const char* model) {
    if (model != nullptr && model[0] != '\0') _model = model;
}
void AIKitRealtimeASR::setModel(const String& model) { setModel(model.c_str()); }

void AIKitRealtimeASR::setLanguage(const char* lang) {
    _language = (lang != nullptr) ? lang : "";
}

const char* AIKitRealtimeASR::vendorName() const { return rtVendorName(_vendor); }

// ===========================================================================
// URL / 认证
// ===========================================================================

String AIKitRealtimeASR::buildStartUrl() {
    switch (_vendor) {
        case AIKIT_RT_VOLC:
            return "wss://openspeech.bytedance.com/api/v3/sauc/bigmodel";
        case AIKIT_RT_DASHSCOPE:
            return "wss://dashscope.aliyuncs.com/api-ws/v1/inference";
        case AIKIT_RT_BAIDU: {
            char sn[40];
            genId(sn, sizeof(sn));
            String url = "wss://vop.baidu.com/realtime_asr?sn=";
            url += sn;
            return url;
        }
        case AIKIT_RT_FUNASR: {
            String url = _funasr_tls ? "wss://" : "ws://";
            url += _funasr_host;
            if (_funasr_port != 80 && _funasr_port != 443) {
                url += ':';
                url += String((unsigned long)_funasr_port);
            }
            return url;
        }
        case AIKIT_RT_OPENAI:
            return "wss://api.openai.com/v1/realtime?intent=transcription";
        case AIKIT_RT_STEPFUN:
            return "wss://api.stepfun.com/v1/realtime/asr/stream";
        case AIKIT_RT_MISTRAL: {
            String m = _model.length() > 0 ? _model
                                           : "voxtral-mini-transcribe-realtime-2602";
            String url = "wss://api.mistral.ai/v1/audio/transcriptions/realtime?model=";
            url += m;
            return url;
        }
        default:
            return String();
    }
}

bool AIKitRealtimeASR::applyAuthHeaders() {
    switch (_vendor) {
        case AIKIT_RT_VOLC:
            _ws->addHeader("X-Api-App-Key", _key1);
            _ws->addHeader("X-Api-Access-Key", _key2);
            _ws->addHeader("X-Api-Resource-Id", _volc_resource);
            {
                char rid[40];
                genId(rid, sizeof(rid));
                _ws->addHeader("X-Api-Connect-Id", rid);
            }
            return true;
        case AIKIT_RT_DASHSCOPE:
        case AIKIT_RT_OPENAI:
        case AIKIT_RT_STEPFUN:
        case AIKIT_RT_MISTRAL: {
            String auth = "Bearer ";
            auth += _key1;
            _ws->addHeader("Authorization", auth);
            return true;
        }
        default:
            return true;   // BAIDU / FUNASR 无 header 鉴权
    }
}

// ===========================================================================
// 连接
// ===========================================================================

int AIKitRealtimeASR::connect() {
    String url = buildStartUrl();
    if (url.length() == 0) {
        logE("not configured: call begin() first");
        return AIKIT_ERR_CONFIG;
    }
    close();   // 清理可能的旧连接

    _finished = false;
    _volc_seq = 0;
    _volc_utt_reported = 0;
    _sent_buf = "";

    _ws = new (std::nothrow) WebsocketsClient();
    if (_ws == nullptr) return AIKIT_ERR_OOM;

    _ws->onMessage([this](WebsocketsMessage msg) {
        if (msg.isText()) {
            handleMessageText(msg.rawData().c_str(), msg.rawData().size());
        } else if (msg.isBinary()) {
            handleMessageBinary((const uint8_t*)msg.rawData().data(),
                                msg.rawData().size());
        }
    });
    _ws->onEvent([this](WebsocketsEvent event, WSInterfaceString data) {
        (void)data;
        if (event == WebsocketsEvent::ConnectionClosed) {
            bool finishing = (_state == RT_FINISHING);
            _connected = false;
            if (finishing) {
                _finished = true;    // 服务端正常关流（百度 / DashScope 等）
            } else if (_state == RT_READY) {
                emitError(AIKIT_ERR_NOT_CONNECTED, "connection closed by server");
            }
            _state = RT_CLOSED;
            if (_on_closed) _on_closed();
        }
    });

    if (url.startsWith("wss://")) {
#if defined(ESP8266)
        // ESP8266 BearSSL：insecure 或 CA 信任锚
        if (_ca_valid && _ca_cert != nullptr) {
            static BearSSL::X509List* s_ta = nullptr;
            if (s_ta == nullptr) s_ta = new (std::nothrow) BearSSL::X509List(_ca_cert);
            if (s_ta != nullptr) _ws->setTrustAnchors(s_ta);
            else _ws->setInsecure();
        } else if (_insecure) {
            _ws->setInsecure();
        }
#else
        if (_ca_valid && _ca_cert != nullptr) {
            _ws->setCACert(_ca_cert);
        } else if (_insecure) {
            _ws->setInsecure();
        }
#endif
    }

    applyAuthHeaders();

    logI("connecting");
    if (!_ws->connect(url)) {
        logE("websocket connect failed");
        delete _ws;
        _ws = nullptr;
        emitError(AIKIT_ERR_WS, "websocket connect/handshake failed");
        return AIKIT_ERR_WS;
    }
    _connected = true;
    _state = RT_READY;
    _last_audio_ms = millis();
    _last_ping_ms = millis();

    // 厂商启动指令
    bool needConfirm = (_vendor == AIKIT_RT_DASHSCOPE || _vendor == AIKIT_RT_OPENAI);
    if (!sendStartCommand()) {
        close();
        return AIKIT_ERR_WS;
    }
    if (needConfirm) {
        // 阻塞等待就绪确认（task-started / session.created）
        _state = RT_IDLE;
        uint32_t t0 = millis();
        while (millis() - t0 < AIKIT_RTASR_CONNECT_MS) {
            _ws->poll();
            if (!_connected || _state == RT_READY) break;
            delay(5);
        }
        if (_state != RT_READY) {
            emitError(AIKIT_ERR_TIMEOUT, "no start confirmation from server");
            close();
            return AIKIT_ERR_TIMEOUT;
        }
    }

    logI("connected & ready");
    if (_on_connected) _on_connected();
    return AIKIT_OK;
}

// ===========================================================================
// 音频发送
// ===========================================================================

int AIKitRealtimeASR::sendAudio(const uint8_t* pcm, size_t len) {
    if (pcm == nullptr || len == 0) return AIKIT_ERR_ASR_AUDIO;
    if (!_connected || _ws == nullptr) return AIKIT_ERR_NOT_CONNECTED;
    _last_audio_ms = millis();

    switch (_vendor) {
        case AIKIT_RT_VOLC:
            return sendAudioVolc(pcm, len, false);
        case AIKIT_RT_DASHSCOPE:
        case AIKIT_RT_BAIDU:
        case AIKIT_RT_FUNASR:
            return sendAudioBinary(pcm, len);
        case AIKIT_RT_OPENAI:
        case AIKIT_RT_STEPFUN:
        case AIKIT_RT_MISTRAL:
            return sendAudioB64(pcm, len);
        default:
            return AIKIT_ERR_CONFIG;
    }
}

int AIKitRealtimeASR::sendAudioBinary(const uint8_t* pcm, size_t len) {
    return _ws->sendBinary((const char*)pcm, len) ? AIKIT_OK : AIKIT_ERR_WS;
}

int AIKitRealtimeASR::sendAudioVolc(const uint8_t* pcm, size_t len, bool lastPacket) {
    _volc_seq++;
    int32_t seq = lastPacket ? -(int32_t)_volc_seq : (int32_t)_volc_seq;
    uint8_t hdr[12];
    hdr[0] = 0x11;                          // protocol v1 | header size 1
    hdr[1] = lastPacket ? 0x23 : 0x21;      // msg_type=audio(0b0010) | flags
    hdr[2] = 0x00;                          // 无序列化 / 无压缩
    hdr[3] = 0x00;
    wrBE32(hdr + 4, (uint32_t)seq);
    wrBE32(hdr + 8, (uint32_t)len);
    uint8_t* buf = (uint8_t*)malloc(12 + len);
    if (buf == nullptr) return AIKIT_ERR_OOM;
    memcpy(buf, hdr, 12);
    if (len > 0) memcpy(buf + 12, pcm, len);
    bool ok = _ws->sendBinary((const char*)buf, 12 + len);
    free(buf);
    return ok ? AIKIT_OK : AIKIT_ERR_WS;
}

int AIKitRealtimeASR::sendAudioB64(const uint8_t* pcm, size_t len) {
    const uint8_t* p = pcm;
    size_t n = len;
    uint8_t* resampled = nullptr;

    if (_vendor == AIKIT_RT_OPENAI) {
        // 16 kHz → 24 kHz 线性插值（3/2 倍样本，纯整数运算）
        size_t inSamples = len / 2;
        if (inSamples == 0) return AIKIT_ERR_ASR_AUDIO;
        size_t outSamples = inSamples * 3 / 2;
        resampled = (uint8_t*)malloc(outSamples * 2);
        if (resampled == nullptr) return AIKIT_ERR_OOM;
        const int16_t* in = (const int16_t*)pcm;
        int16_t* out = (int16_t*)resampled;
        for (size_t i = 0; i < outSamples; i++) {
            uint32_t numer = (uint32_t)i * 2;
            size_t i0 = numer / 3;
            uint32_t rem = numer % 3;
            int32_t s0 = in[i0];
            int32_t s1 = (i0 + 1 < inSamples) ? in[i0 + 1] : s0;
            out[i] = (int16_t)(s0 + (s1 - s0) * (int32_t)rem / 3);
        }
        p = resampled;
        n = outSamples * 2;
    }

    size_t b64Len = (n + 2) / 3 * 4 + 1;
    char* b64 = (char*)malloc(b64Len);
    if (b64 == nullptr) {
        if (resampled != nullptr) free(resampled);
        return AIKIT_ERR_OOM;
    }
    rtB64Encode(p, n, b64);
    if (resampled != nullptr) free(resampled);

    String msg = "{\"type\":\"input_audio_buffer.append\",\"audio\":\"";
    msg.reserve(msg.length() + b64Len + 4);
    msg += b64;
    free(b64);
    msg += "\"}";
    bool ok = _ws->send(msg);
    return ok ? AIKIT_OK : AIKIT_ERR_WS;
}

int AIKitRealtimeASR::commitTurn() {
    if (!_connected || _ws == nullptr) return AIKIT_ERR_NOT_CONNECTED;
    if (_vendor != AIKIT_RT_OPENAI && _vendor != AIKIT_RT_STEPFUN &&
        _vendor != AIKIT_RT_MISTRAL) {
        return AIKIT_ERR_NOT_SUPPORTED;
    }
    return _ws->send("{\"type\":\"input_audio_buffer.commit\"}") ? AIKIT_OK
                                                                 : AIKIT_ERR_WS;
}

// ===========================================================================
// 厂商启动指令
// ===========================================================================

bool AIKitRealtimeASR::sendStartCommand() {
    switch (_vendor) {
        case AIKIT_RT_VOLC: {
            // full client request：4B header + seq + size + JSON
            JsonDocument doc;
            doc["user"]["uid"] = "aikit";
            doc["audio"]["format"] = "pcm";
            doc["audio"]["codec"] = "raw";
            doc["audio"]["rate"] = 16000;
            doc["audio"]["bits"] = 16;
            doc["audio"]["channel"] = 1;
            doc["request"]["model_name"] = "bigmodel";
            doc["request"]["enable_itn"] = true;
            doc["request"]["enable_punc"] = true;
            doc["request"]["result_type"] = "full";
            doc["request"]["show_utterances"] = true;
            String json;
            serializeJson(doc, json);
            if (json.length() == 0) return false;
            _volc_seq = 1;
            uint8_t hdr[12];
            hdr[0] = 0x11;   // protocol v1 | header size 1
            hdr[1] = 0x11;   // msg_type=full client request(0b0001) | flags=正seq
            hdr[2] = 0x10;   // serialization=JSON(0b0001) | compression=none
            hdr[3] = 0x00;
            wrBE32(hdr + 4, 1);
            wrBE32(hdr + 8, (uint32_t)json.length());
            size_t total = 12 + json.length();
            uint8_t* buf = (uint8_t*)malloc(total);
            if (buf == nullptr) return false;
            memcpy(buf, hdr, 12);
            memcpy(buf + 12, json.c_str(), json.length());
            bool ok = _ws->sendBinary((const char*)buf, total);
            free(buf);
            return ok;
        }
        case AIKIT_RT_DASHSCOPE: {
            char tid[40];
            genId(tid, sizeof(tid));
            _task_id = tid;
            JsonDocument doc;
            doc["header"]["action"] = "run-task";
            doc["header"]["task_id"] = tid;
            doc["header"]["streaming"] = "duplex";
            JsonObject payload = doc["payload"].to<JsonObject>();
            payload["task_group"] = "audio";
            payload["task"] = "asr";
            payload["function"] = "recognition";
            payload["model"] = _model.length() > 0 ? _model : "paraformer-realtime-v2";
            payload["input"].to<JsonObject>();
            JsonObject params = payload["parameters"].to<JsonObject>();
            params["format"] = "pcm";
            params["sample_rate"] = 16000;
            params["heartbeat"] = true;
            params["punctuation_prediction_enabled"] = true;
            params["inverse_text_normalization_enabled"] = true;
            if (_language.length() > 0) {
                JsonArray hints = params["language_hints"].to<JsonArray>();
                hints.add(_language);
            }
            doc["context"]["device_id"] = "aikit";
            String json;
            serializeJson(doc, json);
            return (json.length() > 0) && _ws->send(json);
        }
        case AIKIT_RT_BAIDU: {
            char cuid[17];
            snprintf(cuid, sizeof(cuid), "esp%08x", rtRand());
            JsonDocument doc;
            doc["type"] = "START";
            JsonObject d = doc["data"].to<JsonObject>();
            d["appid"] = (long)atoi(_key1.c_str());
            d["appkey"] = _key2;
            d["dev_pid"] = _baidu_dev_pid;
            d["cuid"] = cuid;
            d["format"] = "pcm";
            d["sample"] = 16000;
            String json;
            serializeJson(doc, json);
            return (json.length() > 0) && _ws->send(json);
        }
        case AIKIT_RT_FUNASR: {
            JsonDocument doc;
            doc["mode"] = "2pass";
            JsonArray cs = doc["chunk_size"].to<JsonArray>();
            cs.add(5); cs.add(10); cs.add(5);
            doc["chunk_interval"] = 10;
            doc["encoder_chunk_look_back"] = 4;
            doc["decoder_chunk_look_back"] = 0;
            doc["audio_fs"] = 16000;
            doc["wav_name"] = "aikit";
            doc["wav_format"] = "pcm";
            doc["is_speaking"] = true;
            doc["hotwords"] = "";
            doc["itn"] = true;
            String json;
            serializeJson(doc, json);
            return (json.length() > 0) && _ws->send(json);
        }
        case AIKIT_RT_STEPFUN: {
            JsonDocument doc;
            doc["type"] = "session.update";
            JsonObject session = doc["session"].to<JsonObject>();
            session["type"] = "transcription";
            JsonObject input = session["audio"]["input"].to<JsonObject>();
            JsonObject fmt = input["format"].to<JsonObject>();
            fmt["type"] = "pcm";
            fmt["codec"] = "pcm_s16le";
            fmt["rate"] = 16000;
            fmt["bits"] = 16;
            fmt["channel"] = 1;
            JsonObject tr = input["transcription"].to<JsonObject>();
            tr["model"] = _model.length() > 0 ? _model : "stepaudio-2.5-asr-stream";
            if (_language.length() > 0) tr["language"] = _language;
            input["turn_detection"] = nullptr;
            String json;
            serializeJson(doc, json);
            return (json.length() > 0) && _ws->send(json);
        }
        case AIKIT_RT_OPENAI:
            // 等 session.created 后再发 session.update（见 handleMessageText）
            return true;
        case AIKIT_RT_MISTRAL:
            // 实验性：模型已在 URL 参数，无启动指令
            return true;
        default:
            return false;
    }
}

// ===========================================================================
// 结束 / 主循环 / 关闭
// ===========================================================================

void AIKitRealtimeASR::sendFinishCommand() {
    switch (_vendor) {
        case AIKIT_RT_VOLC: {
            // 最后一包：flags=0b0011 + 负 seq + 一小段静音（160 样本）
            static const uint8_t kSilence[320] = {0};
            sendAudioVolc(kSilence, sizeof(kSilence), true);
            break;
        }
        case AIKIT_RT_DASHSCOPE: {
            JsonDocument doc;
            doc["header"]["action"] = "finish-task";
            doc["header"]["task_id"] = _task_id;
            doc["header"]["streaming"] = "duplex";
            doc["payload"]["input"].to<JsonObject>();
            String json;
            serializeJson(doc, json);
            if (json.length() > 0) _ws->send(json);
            break;
        }
        case AIKIT_RT_BAIDU:
            _ws->send("{\"type\":\"FINISH\"}");
            break;
        case AIKIT_RT_FUNASR:
            _ws->send("{\"is_speaking\": false}");
            break;
        case AIKIT_RT_OPENAI:
        case AIKIT_RT_STEPFUN:
        case AIKIT_RT_MISTRAL:
            _ws->send("{\"type\":\"input_audio_buffer.commit\"}");
            break;
        default:
            break;
    }
}

int AIKitRealtimeASR::finish(uint32_t waitMs) {
    if (!_connected || _ws == nullptr) return AIKIT_ERR_NOT_CONNECTED;
    _state = RT_FINISHING;
    _finished = false;
    sendFinishCommand();
    pump(waitMs, true);
    bool got = _finished;
    close();
    return got ? AIKIT_OK : AIKIT_ERR_TIMEOUT;
}

void AIKitRealtimeASR::pump(uint32_t waitMs, bool untilFinished) {
    uint32_t t0 = millis();
    while (millis() - t0 < waitMs) {
        if (_ws != nullptr) _ws->poll();
        if (!untilFinished || _finished || !_connected) break;
        delay(5);
    }
}

void AIKitRealtimeASR::loop() {
    if (_ws == nullptr || !_connected) return;
    _ws->poll();
    uint32_t now = millis();
    if (now - _last_ping_ms > AIKIT_RTASR_PING_MS) {
        _last_ping_ms = now;
        // 空闲（未发音频）超阈值时保活
        if (now - _last_audio_ms > AIKIT_RTASR_PING_MS) {
            if (_vendor == AIKIT_RT_BAIDU) {
                _ws->send("{\"type\":\"HEARTBEAT\"}");
            } else {
                _ws->ping();
            }
        }
    }
}

void AIKitRealtimeASR::close() {
    if (_ws != nullptr) {
        if (_connected) {
            _ws->close();
        }
        delete _ws;
        _ws = nullptr;
    }
    bool wasConnected = _connected;
    _connected = false;
    _state = RT_CLOSED;
    _volc_seq = 0;
    _volc_utt_reported = 0;
    _sent_buf = "";
    if (wasConnected && _on_closed) _on_closed();
}

// ===========================================================================
// 回调转发
// ===========================================================================

void AIKitRealtimeASR::emitPartial(const char* text) {
    if (_on_partial && text != nullptr) _on_partial(text);
}
void AIKitRealtimeASR::emitFinal(const char* text) {
    if (_on_final && text != nullptr) _on_final(text);
}
void AIKitRealtimeASR::emitError(int err, const char* msg) {
    _last_err = err;
    _last_err_text = (msg != nullptr) ? msg : "";
    logE("%s", _last_err_text.c_str());
    if (_on_error) _on_error(err, _last_err_text.c_str());
}

// ===========================================================================
// 消息分发
// ===========================================================================

void AIKitRealtimeASR::handleMessageBinary(const uint8_t* data, size_t len) {
    if (_vendor == AIKIT_RT_VOLC) {
        handleVolcBinary(data, len);
    }
}

void AIKitRealtimeASR::handleMessageText(const char* data, size_t len) {
    JsonDocument doc;
    if (deserializeJson(doc, data, len) != DeserializationError::Ok) {
        emitError(AIKIT_ERR_PARSE, "invalid JSON from server");
        return;
    }

    switch (_vendor) {
        case AIKIT_RT_DASHSCOPE: {
            const char* ev = doc["header"]["event"];
            if (ev == nullptr) break;
            if (!strcmp(ev, "task-started")) {
                _state = RT_READY;
            } else if (!strcmp(ev, "result-generated")) {
                JsonObjectConst sent = doc["payload"]["output"]["sentence"];
                if (sent.isNull()) break;
                if (sent["heartbeat"].as<bool>()) break;   // 保活空包
                const char* text = sent["text"];
                if (text == nullptr) break;
                if (sent["sentence_end"].as<bool>()) {
                    _sent_buf = "";
                    emitFinal(text);
                    if (_state == RT_FINISHING) _finished = true;
                } else {
                    emitPartial(text);
                }
            } else if (!strcmp(ev, "task-finished")) {
                _finished = true;
            } else if (!strcmp(ev, "task-failed")) {
                const char* code = doc["header"]["error_code"];
                const char* msg = doc["header"]["error_message"];
                emitError(AIKIT_ERR_ASR_PROTOCOL,
                          (msg != nullptr) ? msg : (code ? code : "task-failed"));
            }
            break;
        }
        case AIKIT_RT_BAIDU: {
            const char* type = doc["type"];
            if (type == nullptr) break;
            if (!strcmp(type, "MID_TEXT")) {
                const char* r = doc["result"];
                if (r != nullptr) emitPartial(r);
            } else if (!strcmp(type, "FIN_TEXT")) {
                int errNo = doc["err_no"] | 0;
                if (errNo != 0) {
                    const char* m = doc["err_msg"];
                    emitError(AIKIT_ERR_ASR_PROTOCOL,
                              (m != nullptr) ? m : "baidu asr error");
                } else {
                    const char* r = doc["result"];
                    if (r != nullptr) emitFinal(r);
                }
                if (_state == RT_FINISHING) _finished = true;
            }
            // HEARTBEAT：忽略
            break;
        }
        case AIKIT_RT_FUNASR: {
            const char* mode = doc["mode"];
            const char* text = doc["text"];
            if (mode == nullptr || text == nullptr) break;
            if (!strcmp(mode, "2pass-online") || !strcmp(mode, "online")) {
                _sent_buf += text;                  // 增量（无标点）→ 句子累积
                emitPartial(_sent_buf.c_str());
            } else {
                _sent_buf = "";                     // offline / 2pass-offline 定稿
                emitFinal(text);
                if (_state == RT_FINISHING) _finished = true;
            }
            break;
        }
        case AIKIT_RT_OPENAI:
        case AIKIT_RT_STEPFUN:
        case AIKIT_RT_MISTRAL: {
            const char* type = doc["type"];
            if (type == nullptr) break;
            if (!strcmp(type, "session.created")) {
                // OpenAI：会话创建后下发配置更新
                if (_vendor == AIKIT_RT_OPENAI) {
                    JsonDocument u;
                    u["type"] = "session.update";
                    JsonObject session = u["session"].to<JsonObject>();
                    session["type"] = "transcription";
                    JsonObject input = session["audio"]["input"].to<JsonObject>();
                    JsonObject fmt = input["format"].to<JsonObject>();
                    fmt["type"] = "audio/pcm";
                    fmt["rate"] = 24000;
                    JsonObject tr = input["transcription"].to<JsonObject>();
                    tr["model"] = _model.length() > 0 ? _model : "gpt-live-transcribe";
                    if (_language.length() > 0) tr["language"] = _language;
                    input["turn_detection"] = nullptr;
                    String json;
                    serializeJson(u, json);
                    if (json.length() > 0) _ws->send(json);
                    _state = RT_READY;
                }
            } else if (!strcmp(type, "conversation.item.input_audio_transcription.delta") ||
                       !strcmp(type, "transcription.text.delta")) {
                const char* d = doc["delta"];
                if (d == nullptr) d = doc["text"];
                if (d != nullptr) {
                    if (_vendor == AIKIT_RT_OPENAI || _vendor == AIKIT_RT_MISTRAL) {
                        _sent_buf += d;             // 增量 → 句子累积
                        emitPartial(_sent_buf.c_str());
                    } else {
                        emitPartial(d);            // StepFun：text 已是累计全量
                    }
                }
            } else if (!strcmp(type, "conversation.item.input_audio_transcription.completed") ||
                       !strcmp(type, "transcription.done")) {
                const char* t = doc["transcript"];
                if (t == nullptr) t = doc["text"];
                _sent_buf = "";
                if (t != nullptr) emitFinal(t);
                if (_state == RT_FINISHING) _finished = true;
            } else if (!strcmp(type, "error")) {
                const char* m = doc["error"]["message"];
                emitError(AIKIT_ERR_ASR_PROTOCOL,
                          (m != nullptr) ? m : "server error event");
            }
            break;
        }
        default:
            break;
    }
}

// ===========================================================================
// 火山引擎二进制响应
// ===========================================================================

void AIKitRealtimeASR::handleVolcBinary(const uint8_t* p, size_t n) {
    if (n < 4) return;
    uint8_t msgType = p[1] >> 4;
    uint8_t flags   = p[1] & 0x0F;
    uint8_t comp    = p[2] & 0x0F;

    if (msgType == 0x0F) {
        // 错误响应：[4B header][4B code][4B msg size][msg]
        if (n >= 12) {
            uint32_t code = rdBE32(p + 4);
            uint32_t mlen = rdBE32(p + 8);
            size_t avail = (n - 12 < mlen) ? (n - 12) : mlen;
            char codeStr[48];
            snprintf(codeStr, sizeof(codeStr), "volc error %u", (unsigned)code);
            if (avail > 0) {
                String m((const char*)p + 12, avail);
                emitError(AIKIT_ERR_ASR_PROTOCOL, m.c_str());
            } else {
                emitError(AIKIT_ERR_ASR_PROTOCOL, codeStr);
            }
        }
        return;
    }
    if (msgType != 0x09) return;   // 仅处理 full server response

    size_t off = 4;
    if (flags & 0x01) off += 4;    // sequence number
    if (n < off + 4) return;
    uint32_t psz = rdBE32(p + off);
    off += 4;
    const uint8_t* payload = p + off;
    size_t plen = (psz <= n - off) ? psz : (n - off);

    String json;
    if (comp & 0x01) {
        if (!gunzipToString(payload, plen, json)) {
            emitError(AIKIT_ERR_INFLATE, "gunzip failed on volc response");
            return;
        }
    } else {
        json.concat((const char*)payload, plen);
    }
    if (json.length() == 0) return;

    JsonDocument doc;
    if (deserializeJson(doc, json) != DeserializationError::Ok) {
        emitError(AIKIT_ERR_PARSE, "invalid JSON in volc response");
        return;
    }

    // result.text 为全量累计；utterances[] 区分中间 / 定稿
    JsonArrayConst utts = doc["result"]["utterances"];
    if (!utts.isNull()) {
        size_t seenDefinite = 0;
        for (JsonObjectConst u : utts) {
            bool def = u["definite"].as<bool>();
            const char* t = u["text"];
            if (def) {
                if (seenDefinite == _volc_utt_reported) {
                    if (t != nullptr) emitFinal(t);
                    _volc_utt_reported++;
                }
                seenDefinite++;
            } else if (t != nullptr) {
                emitPartial(t);
            }
        }
    } else {
        const char* text = doc["result"]["text"];
        if (text != nullptr) emitPartial(text);
    }

    if (flags & 0x02) _finished = true;   // is_last_package：最终响应
}

// ===========================================================================
// gunzip（复用 StreamHTTPClient 内置 miniz）
// ===========================================================================

bool AIKitRealtimeASR::gunzipToString(const uint8_t* in, size_t len, String& out) {
    out = "";
    if (in == nullptr || len == 0) return false;
    uint8_t* buf = (uint8_t*)malloc(AIKIT_RTASR_RECV_BUF);
    if (buf == nullptr) return false;
    StreamHTTPClient_GzipDecompressor dc("gzip");
    auto cb = [](const uint8_t* d, size_t n, void* user) {
        ((String*)user)->concat((const char*)d, n);
    };
    bool ok = false;
    size_t off = 0;
    while (off < len) {
        int c = dc.decode(in + off, len - off, /*final_chunk=*/true, buf,
                          AIKIT_RTASR_RECV_BUF, cb, &out);
        if (c == -1) { ok = true; break; }   // 流结束
        if (c == -2 || c == 0) break;        // 解码错误 / 无进展
        off += (size_t)c;
        if (off >= len) { ok = true; break; }
    }
    free(buf);
    return ok && out.length() > 0;
}

// ===========================================================================
// 日志
// ===========================================================================

void AIKitRealtimeASR::logE(const char* fmt, ...) {
    if (_log == nullptr || _log_level == AIKIT_LOG_NONE) return;
    char buf[AIKIT_LOG_BUF];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    rtLog(_log, "E", buf);
}

void AIKitRealtimeASR::logI(const char* fmt, ...) {
    if (_log == nullptr || _log_level < AIKIT_LOG_INFO) return;
    char buf[AIKIT_LOG_BUF];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    rtLog(_log, "I", buf);
}

#endif // AIKIT_HAS_RTASR
