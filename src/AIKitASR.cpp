/*
 * AIKitASR.cpp - HTTP 语音转文字实现（v1.2.0）
 *
 * 各厂商协议（2026-10 官方文档核实）：
 *   OpenAI   POST /v1/audio/transcriptions         multipart {file,model,language}
 *   Groq     POST /openai/v1/audio/transcriptions  同上（完全兼容）
 *   Mistral  POST /v1/audio/transcriptions         multipart + x-api-key
 *   智谱     POST /api/paas/v4/audio/transcriptions multipart {file,model,prompt,hotwords}
 *   百炼     POST /api/v1/services/aigc/multimodal-generation/generation
 *            JSON {"model","input":{"file":"data:audio/wav;base64,..."},
 *                  "parameters":{"format":...}} + X-DashScope-SSE: disable
 *            → 响应 output.text
 *   StepFun  POST /v1/audio/asr/sse                JSON（音频 base64）+ SSE
 *            事件 transcript.text.delta / transcript.text.done
 *   MiMo     POST /v1/chat/completions             messages 内 input_audio data URI
 *            → 响应 choices[0].message.content
 *   FunASR   自部署 examples/openai_api：OpenAI 兼容 multipart，无鉴权
 *
 * AIKit-Arduino (https://github.com/ioqit/AIKit-Arduino)
 */
#include "AIKitASR.h"

// ===========================================================================
// 厂商预设表
// ===========================================================================

static const AIKitASRVendorInfo kAIKitASRVendors[AIKIT_ASR_VENDOR_COUNT] = {
    // name             host                        port  path                                defaultModel               https style
    { "OpenAI",         "api.openai.com",            443,  "/v1/audio/transcriptions",         "gpt-transcribe",          true,  0 },
    { "Groq",           "api.groq.com",              443,  "/openai/v1/audio/transcriptions",  "whisper-large-v3-turbo",  true,  0 },
    { "Mistral",        "api.mistral.ai",            443,  "/v1/audio/transcriptions",         "voxtral-mini-latest",     true,  0 },
    { "Zhipu GLM-ASR",  "open.bigmodel.cn",          443,  "/api/paas/v4/audio/transcriptions","glm-asr-2512",            true,  0 },
    { "DashScope ASR",  "dashscope.aliyuncs.com",    443,  "/api/v1/services/aigc/multimodal-generation/generation",
                                                                          "fun-asr-flash",          true,  1 },
    { "StepFun ASR",    "api.stepfun.com",           443,  "/v1/audio/asr/sse",                "stepaudio-3-asr-max",     true,  2 },
    { "MiMo ASR",       "api.xiaomimimo.com",        443,  "/v1/chat/completions",             "mimo-v2.5-asr",           true,  3 },
    { "FunASR (self)",  "localhost",                 8000, "/v1/audio/transcriptions",         "sensevoice",              false, 0 },
};

const AIKitASRVendorInfo* aikitASRVendorInfo(uint8_t id) {
    if (id >= AIKIT_ASR_VENDOR_COUNT) return nullptr;
    return &kAIKitASRVendors[id];
}

const char* aikitASRVendorName(uint8_t id) {
    const AIKitASRVendorInfo* v = aikitASRVendorInfo(id);
    return v ? v->name : "Unknown";
}

// ===========================================================================
// 工具
// ===========================================================================

namespace {
const char kB64Chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

void logLine(Print* log, const char* tag, const char* msg) {
    if (log == nullptr) return;
    log->print("[ASR][");
    log->print(tag);
    log->print("] ");
    log->print(msg);
    log->print("\r\n");
}
} // namespace

size_t AIKitASRClient::b64Encode(const uint8_t* in, size_t n, char* out) {
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

void AIKitASRClient::buildWavHeader(uint8_t* h, uint32_t pcmLen,
                                    uint32_t sampleRate, uint16_t bits,
                                    uint16_t channels) {
    // 目标平台（ESP32/ESP8266，ARM/Xtensa LS 微控制器）均为小端，
    // 可直接写整数；RIFF/WAVE 格式本身即小端。
    uint32_t byteRate  = sampleRate * channels * (bits / 8);
    uint16_t blockAlign = channels * (bits / 8);
    uint32_t riffSize = 36 + pcmLen;
    uint32_t fmtSize = 16;
    uint16_t fmtTag = 1;    // PCM
    memcpy(h + 0,  "RIFF", 4);
    memcpy(h + 4,  &riffSize, 4);
    memcpy(h + 8,  "WAVE", 4);
    memcpy(h + 12, "fmt ", 4);
    memcpy(h + 16, &fmtSize, 4);
    memcpy(h + 20, &fmtTag, 2);
    memcpy(h + 22, &channels, 2);
    memcpy(h + 24, &sampleRate, 4);
    memcpy(h + 28, &byteRate, 4);
    memcpy(h + 32, &blockAlign, 2);
    memcpy(h + 34, &bits, 2);
    memcpy(h + 36, "data", 4);
    memcpy(h + 40, &pcmLen, 4);
}

const char* AIKitASRClient::mimeFromFilename(const char* filename) {
    if (filename == nullptr) return "application/octet-stream";
    const char* dot = strrchr(filename, '.');
    if (dot == nullptr) return "application/octet-stream";
    if (!strcasecmp(dot, ".wav"))  return "audio/wav";
    if (!strcasecmp(dot, ".mp3"))  return "audio/mpeg";
    if (!strcasecmp(dot, ".m4a"))  return "audio/mp4";
    if (!strcasecmp(dot, ".ogg"))  return "audio/ogg";
    if (!strcasecmp(dot, ".flac")) return "audio/flac";
    if (!strcasecmp(dot, ".opus")) return "audio/ogg";
    if (!strcasecmp(dot, ".pcm"))  return "application/octet-stream";
    return "application/octet-stream";
}

const char* AIKitASRClient::formatFromFilename(const char* filename) {
    if (filename == nullptr) return "wav";
    const char* dot = strrchr(filename, '.');
    if (dot == nullptr) return "wav";
    if (!strcasecmp(dot, ".pcm"))  return "pcm";
    if (!strcasecmp(dot, ".mp3"))  return "mp3";
    if (!strcasecmp(dot, ".wav"))  return "wav";
    return "wav";
}

// ===========================================================================
// 生命周期
// ===========================================================================

AIKitASRClient::AIKitASRClient(Print* log)
    : _log(log), _log_level(log != nullptr ? AIKIT_LOG_INFO : AIKIT_LOG_NONE) {}

AIKitASRClient::~AIKitASRClient() {
#if defined(ESP8266)
    if (_x509_ta != nullptr) { delete _x509_ta; _x509_ta = nullptr; }
#endif
}

bool AIKitASRClient::begin(AIKitASRVendor vendor, const char* apiKey) {
    if (apiKey == nullptr || apiKey[0] == '\0') {
        logE("API key is empty");
        return false;
    }
    const AIKitASRVendorInfo* v = aikitASRVendorInfo((uint8_t)vendor);
    if (v == nullptr) {
        logE("unknown ASR vendor id");
        return false;
    }
    if (vendor == AIKIT_ASR_FUNASR) {
        // FunASR 自部署无鉴权，建议 beginFunASR()；此处按默认本机处理
        return beginFunASR(v->host, v->port);
    }
    _vendor_id = (uint8_t)vendor;
    _preset = v;
    _vendor_name = v->name;
    _host = v->host;
    _port = v->port;
    _path = v->path;
    _https = v->https;
    _api_key = apiKey;
    _model = v->defaultModel;
    logI("ASR ready");
    return true;
}

bool AIKitASRClient::begin(AIKitASRVendor vendor, const String& apiKey) {
    return begin(vendor, apiKey.c_str());
}

bool AIKitASRClient::beginFunASR(const char* host, uint16_t port) {
    if (host == nullptr || host[0] == '\0') return false;
    _vendor_id = (uint8_t)AIKIT_ASR_FUNASR;
    _preset = &kAIKitASRVendors[AIKIT_ASR_FUNASR];
    _vendor_name = _preset->name;
    _host = host;
    _port = port;
    _path = "/v1/audio/transcriptions";
    _https = false;
    _api_key = "";
    _model = _preset->defaultModel;
    logI("FunASR self-hosted ready");
    return true;
}

bool AIKitASRClient::beginCustom(const char* host, uint16_t port, const char* path,
                                 bool useHttps, const char* apiKey,
                                 const char* defaultModel) {
    if (host == nullptr || path == nullptr || host[0] == '\0' || path[0] == '\0') {
        return false;
    }
    _vendor_id = (uint8_t)AIKIT_ASR_CUSTOM;
    _preset = nullptr;
    _vendor_name = "Custom";
    _host = host;
    _port = port;
    _path = path;
    _https = useHttps;
    _api_key = (apiKey != nullptr) ? apiKey : "";
    _model = (defaultModel != nullptr && defaultModel[0] != '\0') ? defaultModel : "whisper-1";
    return true;
}

void AIKitASRClient::setModel(const char* model) {
    if (model != nullptr && model[0] != '\0') _model = model;
}
void AIKitASRClient::setModel(const String& model) { setModel(model.c_str()); }

void AIKitASRClient::setLanguage(const char* lang) {
    _language = (lang != nullptr) ? lang : "";
}

void AIKitASRClient::setPrompt(const char* prompt) {
    _prompt = (prompt != nullptr) ? prompt : "";
}

void AIKitASRClient::setTimeouts(uint32_t connectMs, uint32_t tcpMs) {
    _connect_timeout_ms = connectMs;
    _tcp_timeout_ms = tcpMs;
}

// ===========================================================================
// 转写入口
// ===========================================================================

int AIKitASRClient::transcribe(const uint8_t* audio, size_t audioLen,
                               const char* filename, const char* mime,
                               String& outText) {
    outText = "";
    if (audio == nullptr || audioLen == 0) return AIKIT_ERR_ASR_AUDIO;
    if (_host.length() == 0 || _path.length() == 0) return AIKIT_ERR_CONFIG;

    uint8_t style = (_preset != nullptr) ? _preset->style : 0;
    switch (style) {
        case 1: return performDashScope(audio, audioLen, filename, outText);
        case 2: return performStepFun(audio, audioLen, filename, outText);
        case 3: return performMiMo(audio, audioLen, filename, outText);
        default: break;
    }
    return performMultipart(audio, audioLen, filename,
                            (mime != nullptr) ? mime : mimeFromFilename(filename),
                            outText, nullptr);
}

int AIKitASRClient::transcribePCM(const uint8_t* pcm, size_t pcmLen,
                                  uint32_t sampleRate, uint16_t channels,
                                  String& outText) {
    outText = "";
    if (pcm == nullptr || pcmLen == 0) return AIKIT_ERR_ASR_AUDIO;
    if (_host.length() == 0 || _path.length() == 0) return AIKIT_ERR_CONFIG;

    uint8_t style = (_preset != nullptr) ? _preset->style : 0;

    // StepFun：直接收裸 PCM（format.type=pcm）
    if (style == 2) {
        return performStepFun(pcm, pcmLen, "audio.pcm", outText);
    }

    uint8_t hdr[44];
    buildWavHeader(hdr, (uint32_t)pcmLen, sampleRate, 16, channels);

    if (style == 1) {
        return performDashScope(pcm, pcmLen, "audio.pcm", outText, hdr, (uint32_t)pcmLen,
                                sampleRate, channels);
    }
    if (style == 3) {
        return performMiMo(pcm, pcmLen, "audio.wav", outText, hdr);
    }
    // multipart：WAV 头内嵌进上传缓冲（峰值内存 ≈ 1 份音频）
    return performMultipart(pcm, pcmLen, "audio.wav", "audio/wav",
                            outText, hdr);
}

// ===========================================================================
// 公共 HTTP 收尾
// ===========================================================================

bool AIKitASRClient::prepareHttp(bool sse) {
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
        logE("invalid endpoint URL");
        return false;
    }
    if (_https) {
        WiFiClientSecure* sec = _http.getSecureClient();
        if (sec != nullptr) {
            if (_ca_valid && _ca_cert != nullptr) {
#if defined(ESP8266)
                if (_x509_ta == nullptr) {
                    _x509_ta = new (std::nothrow) BearSSL::X509List(_ca_cert);
                }
                if (_x509_ta != nullptr) sec->setTrustAnchors(_x509_ta);
#else
                sec->setCACert(_ca_cert);
#endif
            } else if (_insecure) {
                sec->setInsecure();
            }
        }
    }
    _http.setConnectTimeout(_connect_timeout_ms);
    _http.setTcpTimeout(_tcp_timeout_ms);
    _http.setFollowRedirects(SHC_REDIRECT_OFF);

    if (_api_key.length() > 0) {
        // Mistral 用 x-api-key；其余 Bearer（Mistral 也兼容 Bearer，但按官方文档走）
        if (_vendor_id == (uint8_t)AIKIT_ASR_MISTRAL) {
            _http.addHeader("x-api-key", _api_key);
        } else {
            String auth = "Bearer ";
            auth += _api_key;
            _http.setAuthorization(auth);
        }
    }
    if (sse) {
        _http.addHeader("Accept", "text/event-stream");
        _http.useStreamMode(false);
        _http.setDecompress(true);
        _http.acceptEncoding("identity");   // 保持 SSE 行结构
    } else {
        _http.useStreamMode(false);
        _http.setDecompress(true);
        _http.acceptEncoding("gzip, deflate");
    }
    // 非 2xx 时采集错误响应体
    _http.onData([this](uint8_t* data, size_t len) {
        int code = _http.getStatusCode();
        if (!(code >= 200 && code < 300)) appendErrBody(data, len);
    });
    return true;
}

void AIKitASRClient::appendErrBody(const uint8_t* data, size_t len) {
    if (_err_body.length() >= AIKIT_ERRBODY_MAX) return;
    size_t room = AIKIT_ERRBODY_MAX - _err_body.length();
    if (len > room) len = room;
    _err_body.concat((const char*)data, len);
}

int AIKitASRClient::finishRequest(int code, String& body, String& outText) {
    _http_status = code;
    if (code < 0) {
        _transport_err = code;
        logE("transport error");
        _http.end();
        return AIKIT_ERR_CONNECT;
    }
    if (code < 200 || code >= 300) {
        logE("HTTP error status");
        _http.end();
        return AIKIT_ERR_HTTP_STATUS;
    }
    int rc = parseTranscription(body, outText);
    _http.end();
    return rc;
}

// ===========================================================================
// multipart（OpenAI / Groq / Mistral / 智谱 / FunASR / 自定义）
// ===========================================================================

int AIKitASRClient::performMultipart(const uint8_t* audio, size_t audioLen,
                                     const char* filename, const char* mime,
                                     String& outText, const uint8_t* wavHdr) {
    static const char kBoundary[] = "AIKitBoundary7f3a9c2x1";

    size_t hdrLen = (wavHdr != nullptr) ? 44 : 0;

    // ---- 组装 multipart 文本头 ----
    String head;
    head.reserve(512);
    head += "--";
    head += kBoundary;
    head += "\r\nContent-Disposition: form-data; name=\"model\"\r\n\r\n";
    head += _model;
    head += "\r\n";
    if (_language.length() > 0) {
        head += "--";
        head += kBoundary;
        head += "\r\nContent-Disposition: form-data; name=\"language\"\r\n\r\n";
        head += _language;
        head += "\r\n";
    }
    if (_prompt.length() > 0) {
        head += "--";
        head += kBoundary;
        head += "\r\nContent-Disposition: form-data; name=\"prompt\"\r\n\r\n";
        head += _prompt;
        head += "\r\n";
    }
    head += "--";
    head += kBoundary;
    head += "\r\nContent-Disposition: form-data; name=\"file\"; filename=\"";
    head += filename;
    head += "\"\r\nContent-Type: ";
    head += mime;
    head += "\r\n\r\n";

    String tail = "\r\n--";
    tail += kBoundary;
    tail += "--\r\n";

    size_t total = head.length() + hdrLen + audioLen + tail.length();
    if (total > AIKIT_ASR_MAX_UPLOAD_BYTES) {
        logE("upload body too large");
        return AIKIT_ERR_PAYLOAD_TOO_BIG;
    }

    uint8_t* buf = (uint8_t*)malloc(total);
    if (buf == nullptr) {
        logE("OOM building multipart body");
        return AIKIT_ERR_OOM;
    }
    memcpy(buf, head.c_str(), head.length());
    size_t off = head.length();
    if (hdrLen > 0) { memcpy(buf + off, wavHdr, hdrLen); off += hdrLen; }
    memcpy(buf + off, audio, audioLen); off += audioLen;
    memcpy(buf + off, tail.c_str(), tail.length());

    if (!prepareHttp(false)) {
        free(buf);
        return AIKIT_ERR_CONFIG;
    }
    String ct = "multipart/form-data; boundary=";
    ct += kBoundary;
    _http.addHeader("Content-Type", ct);

    int code = _http.POST(buf, total);
    free(buf);

    String body = _http.getString();
    return finishRequest(code, body, outText);
}

// ===========================================================================
// DashScope 同步（fun-asr-flash / qwen3-asr-flash）
// ===========================================================================

int AIKitASRClient::performDashScope(const uint8_t* audio, size_t audioLen,
                                     const char* filename, String& outText,
                                     const uint8_t* wavHdr, uint32_t pcmLen,
                                     uint32_t sampleRate, uint16_t channels) {
    (void)channels;
    const char* fmt = formatFromFilename(filename);

    // ---- 组装完整音频（PCM 需包 WAV 头）----
    size_t audioTotal = audioLen;
    const uint8_t* audioPtr = audio;
    uint8_t* wavBuf = nullptr;
    if (wavHdr != nullptr) {
        wavBuf = (uint8_t*)malloc(44 + audioLen);
        if (wavBuf == nullptr) return AIKIT_ERR_OOM;
        memcpy(wavBuf, wavHdr, 44);
        memcpy(wavBuf + 44, audio, audioLen);
        audioPtr = wavBuf;
        audioTotal = 44 + audioLen;
        fmt = "wav";
    }

    // ---- base64 ----
    size_t b64Len = (audioTotal + 2) / 3 * 4 + 1;
    char* b64 = (char*)malloc(b64Len);
    if (b64 == nullptr) {
        if (wavBuf != nullptr) free(wavBuf);
        return AIKIT_ERR_OOM;
    }
    b64Encode(audioPtr, audioTotal, b64);
    if (wavBuf != nullptr) free(wavBuf);

    size_t jsonEst = b64Len + 256;
    if (jsonEst > AIKIT_ASR_MAX_UPLOAD_BYTES) {
        free(b64);
        logE("request body too large");
        return AIKIT_ERR_PAYLOAD_TOO_BIG;
    }

    // ---- JSON body ----
    JsonDocument doc;
    doc["model"] = _model;
    String dataUri = "data:audio/";
    dataUri += fmt;
    dataUri += ";base64,";
    dataUri += b64;
    free(b64);
    doc["input"]["file"] = dataUri;
    doc["parameters"]["format"] = fmt;
    if (strcmp(fmt, "pcm") == 0) {
        doc["parameters"]["sample_rate"] = String((unsigned long)sampleRate);
    }
    String body;
    serializeJson(doc, body);
    if (doc.overflowed() || body.length() == 0) return AIKIT_ERR_OOM;

    if (!prepareHttp(false)) return AIKIT_ERR_CONFIG;
    _http.addHeader("X-DashScope-SSE", "disable");
    _http.addHeader("Content-Type", "application/json");

    int code = _http.POST(body);
    String resp = _http.getString();
    return finishRequest(code, resp, outText);
}

// ===========================================================================
// StepFun（JSON + SSE）
// ===========================================================================

int AIKitASRClient::performStepFun(const uint8_t* audio, size_t audioLen,
                                   const char* filename, String& outText) {
    const char* fmt = formatFromFilename(filename);
    bool isPcm = (strcmp(fmt, "pcm") == 0);

    size_t b64Len = (audioLen + 2) / 3 * 4 + 1;
    char* b64 = (char*)malloc(b64Len);
    if (b64 == nullptr) return AIKIT_ERR_OOM;
    b64Encode(audio, audioLen, b64);

    if (b64Len + 256 > AIKIT_ASR_MAX_UPLOAD_BYTES) {
        free(b64);
        return AIKIT_ERR_PAYLOAD_TOO_BIG;
    }

    JsonDocument doc;
    doc["audio"]["data"] = (const char*)b64;
    free(b64);
    JsonObject tr = doc["audio"]["input"]["transcription"].to<JsonObject>();
    tr["model"] = _model;
    tr["enable_itn"] = true;
    if (_language.length() > 0) tr["language"] = _language;
    JsonObject fo = doc["audio"]["format"].to<JsonObject>();
    if (isPcm) {
        fo["type"] = "pcm";
        fo["codec"] = "pcm_s16le";
        fo["rate"] = 16000;
        fo["bits"] = 16;
        fo["channel"] = 1;
    } else {
        fo["type"] = fmt;
    }
    String body;
    serializeJson(doc, body);
    if (doc.overflowed() || body.length() == 0) return AIKIT_ERR_OOM;

    if (!prepareHttp(true)) return AIKIT_ERR_CONFIG;
    _http.addHeader("Content-Type", "application/json");

    int code = _http.POST(body);
    String resp = _http.getString();
    return finishRequest(code, resp, outText);
}

// ===========================================================================
// MiMo（chat completions + input_audio）
// ===========================================================================

int AIKitASRClient::performMiMo(const uint8_t* audio, size_t audioLen,
                                const char* filename, String& outText,
                                const uint8_t* wavHdr) {
    const char* fmt = formatFromFilename(filename);

    size_t audioTotal = audioLen;
    const uint8_t* audioPtr = audio;
    uint8_t* wavBuf = nullptr;
    if (wavHdr != nullptr) {
        wavBuf = (uint8_t*)malloc(44 + audioLen);
        if (wavBuf == nullptr) return AIKIT_ERR_OOM;
        memcpy(wavBuf, wavHdr, 44);
        memcpy(wavBuf + 44, audio, audioLen);
        audioPtr = wavBuf;
        audioTotal = 44 + audioLen;
        fmt = "wav";
    }

    size_t b64Len = (audioTotal + 2) / 3 * 4 + 1;
    char* b64 = (char*)malloc(b64Len);
    if (b64 == nullptr) {
        if (wavBuf != nullptr) free(wavBuf);
        return AIKIT_ERR_OOM;
    }
    b64Encode(audioPtr, audioTotal, b64);
    if (wavBuf != nullptr) free(wavBuf);

    if (b64Len + 256 > AIKIT_ASR_MAX_UPLOAD_BYTES) {
        free(b64);
        return AIKIT_ERR_PAYLOAD_TOO_BIG;
    }

    // MiMo 仅支持 mp3 / wav
    const char* useFmt = (strcmp(fmt, "wav") == 0 || strcmp(fmt, "mp3") == 0)
                             ? fmt : "wav";
    JsonDocument doc;
    doc["model"] = _model;
    JsonArray msgs = doc["messages"].to<JsonArray>();
    JsonObject m = msgs.add<JsonObject>();
    m["role"] = "user";
    JsonArray content = m["content"].to<JsonArray>();
    JsonObject part = content.add<JsonObject>();
    part["type"] = "input_audio";
    String dataUri = "data:audio/";
    dataUri += useFmt;
    dataUri += ";base64,";
    dataUri += b64;
    free(b64);
    part["input_audio"]["data"] = dataUri;
    if (_language.length() > 0) {
        doc["asr_options"]["language"] = _language;
    }
    String body;
    serializeJson(doc, body);
    if (doc.overflowed() || body.length() == 0) return AIKIT_ERR_OOM;

    if (!prepareHttp(false)) return AIKIT_ERR_CONFIG;
    _http.addHeader("Content-Type", "application/json");

    int code = _http.POST(body);
    String resp = _http.getString();
    return finishRequest(code, resp, outText);
}

// ===========================================================================
// 响应解析
// ===========================================================================

int AIKitASRClient::parseTranscription(const String& body, String& outText) {
    if (body.length() == 0) return AIKIT_ERR_ASR_PROTOCOL;

    uint8_t style = (_preset != nullptr) ? _preset->style : 0;

    // StepFun：SSE 文本流，扫描 data: 行
    if (style == 2) {
        String acc;
        int pos = 0;
        while (true) {
            int lineEnd = body.indexOf('\n', pos);
            String line = (lineEnd < 0)
                              ? body.substring(pos)
                              : body.substring(pos, lineEnd);
            line.trim();
            if (line.startsWith("data:")) {
                String payload = line.substring(5);
                payload.trim();
                if (payload.length() > 0 && payload != "[DONE]") {
                    JsonDocument ev;
                    if (deserializeJson(ev, payload) == DeserializationError::Ok) {
                        const char* type = ev["type"];
                        if (type != nullptr) {
                            if (strcmp(type, "transcript.text.delta") == 0) {
                                const char* d = ev["delta"];
                                if (d != nullptr) acc += d;
                            } else if (strcmp(type, "transcript.text.done") == 0) {
                                const char* t = ev["text"];
                                if (t != nullptr) { outText = t; return AIKIT_OK; }
                            }
                        }
                    }
                }
            }
            if (lineEnd < 0) break;
            pos = lineEnd + 1;
        }
        if (acc.length() > 0) { outText = acc; return AIKIT_OK; }
        return AIKIT_ERR_ASR_PROTOCOL;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err != DeserializationError::Ok) return AIKIT_ERR_ASR_PROTOCOL;

    if (style == 1) {
        // DashScope：output.text
        const char* t = doc["output"]["text"];
        if (t == nullptr) {
            // 可能是错误响应：{"code":"...","message":"..."}
            return AIKIT_ERR_ASR_PROTOCOL;
        }
        outText = t;
        return AIKIT_OK;
    }
    if (style == 3) {
        // MiMo：choices[0].message.content
        const char* t = doc["choices"][0]["message"]["content"];
        if (t == nullptr) return AIKIT_ERR_ASR_PROTOCOL;
        outText = t;
        return AIKIT_OK;
    }

    // OpenAI 兼容：{"text":"..."}
    const char* t = doc["text"];
    if (t == nullptr) return AIKIT_ERR_ASR_PROTOCOL;
    outText = t;
    return AIKIT_OK;
}

// ===========================================================================
// 日志
// ===========================================================================

void AIKitASRClient::logE(const char* fmt, ...) {
    if (_log == nullptr || _log_level == AIKIT_LOG_NONE) return;
    char buf[AIKIT_LOG_BUF];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    logLine(_log, "E", buf);
}

void AIKitASRClient::logI(const char* fmt, ...) {
    if (_log == nullptr || _log_level < AIKIT_LOG_INFO) return;
    char buf[AIKIT_LOG_BUF];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    logLine(_log, "I", buf);
}
