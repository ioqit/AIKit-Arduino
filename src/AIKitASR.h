/*
 * AIKitASR.h - HTTP 语音转文字（云端 ASR，v1.2.0）
 *
 * 一句话上手：
 *   AIKitASRClient asr(&Serial);
 *   asr.begin(AIKIT_ASR_DASHSCOPE, "sk-...");
 *   String text; asr.transcribePCM(pcm, len, 16000, 1, text);
 *
 * 覆盖的厂商（协议各不相同，本类内部自动适配）：
 *   OpenAI 兼容 multipart —— OpenAI / Groq / Mistral / 智谱 GLM-ASR /
 *                            FunASR 自部署（examples/openai_api）
 *   DashScope 同步 JSON   —— 阿里百炼 fun-asr-flash（SenseVoice 系）/
 *                            qwen3-asr-flash（走 multimodal-generation）
 *   StepFun JSON+SSE      —— 阶跃星辰 stepaudio-3-asr-max
 *   MiMo chat+base64      —— 小米 MiMo-V2.5-ASR（api.xiaomimimo.com）
 *
 * 实时（流式）语音识别请见 AIKitRealtimeASR.h（WebSocket）。
 *
 * 内存提示：请求体在堆上整体拼装（multipart 或 base64 JSON），
 * 默认上限 AIKIT_ASR_MAX_UPLOAD_BYTES（64 KB）。ESP8266 建议控制在
 * ~12 KB 音频（16 kHz PCM16 约 6 秒）以内，或调小该宏。
 *
 * AIKit-Arduino (https://github.com/ioqit/AIKit-Arduino)
 */
#pragma once

#include <Arduino.h>

#include <ArduinoJson.h>
#include <StreamHTTPClient.h>
#if defined(ESP8266)
  #include <BearSSLHelpers.h>
#endif

#include "AIKitConfig.h"
#include "AIKitTypes.h"

// ---------------------------------------------------------------------------
// ASR 厂商预设编号
// ---------------------------------------------------------------------------
enum AIKitASRVendor : uint8_t {
    AIKIT_ASR_OPENAI     = 0,   // OpenAI        /v1/audio/transcriptions
    AIKIT_ASR_GROQ       = 1,   // Groq          /openai/v1/audio/transcriptions
    AIKIT_ASR_MISTRAL    = 2,   // Mistral       /v1/audio/transcriptions（x-api-key）
    AIKIT_ASR_ZHIPU      = 3,   // 智谱 GLM-ASR  /api/paas/v4/audio/transcriptions
    AIKIT_ASR_DASHSCOPE  = 4,   // 阿里百炼      multimodal-generation（fun-asr-flash）
    AIKIT_ASR_STEPFUN    = 5,   // 阶跃星辰      /v1/audio/asr/sse（JSON+SSE）
    AIKIT_ASR_MIMO       = 6,   // 小米 MiMo     /v1/chat/completions（base64 音频）
    AIKIT_ASR_FUNASR     = 7,   // FunASR 自部署 /v1/audio/transcriptions（无鉴权）
    AIKIT_ASR_VENDOR_COUNT = 8,
    AIKIT_ASR_CUSTOM     = 0xFF // 任意 OpenAI 兼容 multipart 端点
};

// ---------------------------------------------------------------------------
// ASR 厂商预设信息
// ---------------------------------------------------------------------------
struct AIKitASRVendorInfo {
    const char* name;          // 展示名
    const char* host;
    uint16_t    port;
    const char* path;
    const char* defaultModel;  // 可 setModel() 更换
    bool        https;
    uint8_t     style;         // 0=OpenAI multipart / 1=DashScope JSON / 2=StepFun SSE / 3=MiMo chat
};

const AIKitASRVendorInfo* aikitASRVendorInfo(uint8_t id);
const char* aikitASRVendorName(uint8_t id);

// ---------------------------------------------------------------------------
// HTTP 转写客户端
// ---------------------------------------------------------------------------
class AIKitASRClient {
public:
    explicit AIKitASRClient(Print* log = nullptr);
    ~AIKitASRClient();

    // 选择厂商预设并配置 API Key（FunASR 自部署请用 beginFunASR()）
    bool begin(AIKitASRVendor vendor, const char* apiKey);
    bool begin(AIKitASRVendor vendor, const String& apiKey);

    // FunASR 自部署（runtime/examples/openai_api 的 server，默认 :8000，无鉴权）
    bool beginFunASR(const char* host, uint16_t port = 8000);

    // 任意 OpenAI 兼容 multipart 转写端点
    bool beginCustom(const char* host, uint16_t port, const char* path,
                     bool useHttps, const char* apiKey,
                     const char* defaultModel = nullptr);

    // -------------------------------------------------------------------
    // 请求参数（下一次 transcribe 生效）
    // -------------------------------------------------------------------
    void setModel(const char* model);            // nullptr 忽略
    void setModel(const String& model);
    void setLanguage(const char* lang);          // "zh"/"en"...；nullptr/"" 清除（默认不发送）
    void setPrompt(const char* prompt);          // 上下文提示词（OpenAI/智谱/GLM 支持）
    // TLS（默认 insecure=true 便于上手；生产请配 CA 并 setInsecure(false)）
    void setInsecure(bool yes) { _insecure = yes; }
    void setCACert(const char* pem) { _ca_cert = pem; _ca_valid = (pem != nullptr); }
    void setTimeouts(uint32_t connectMs, uint32_t tcpMs);

    // -------------------------------------------------------------------
    // 转写
    // -------------------------------------------------------------------
    // 通用：发送一段完整音频字节（wav/mp3/pcm...，filename 决定服务端识别的
    // 格式与扩展名，mime 为 Content-Type，如 "audio/wav"）。
    // 返回 AIKIT_OK 时 outText 为识别文本。
    int transcribe(const uint8_t* audio, size_t audioLen,
                   const char* filename, const char* mime,
                   String& outText);

    // 便捷：裸 PCM16 自动包装 44 字节 WAV 头再上传（所有厂商都接受 WAV；
    // StepFun 例外——它直接收裸 PCM，本方法内部自动处理）。
    int transcribePCM(const uint8_t* pcm, size_t pcmLen,
                      uint32_t sampleRate, uint16_t channels,
                      String& outText);

    // -------------------------------------------------------------------
    // 最近一次请求状态
    // -------------------------------------------------------------------
    int    lastStatusCode() const { return _http_status; }
    int    lastTransportError() const { return _transport_err; }
    String lastErrorBody() const { return _err_body; }
    const char* vendorName() const { return _vendor_name; }
    const char* model() const { return _model.c_str(); }

private:
    int  performMultipart(const uint8_t* audio, size_t audioLen,
                          const char* filename, const char* mime,
                          String& outText, const uint8_t* wavHdr = nullptr);
    int  performDashScope(const uint8_t* audio, size_t audioLen,
                          const char* filename, String& outText,
                          const uint8_t* wavHdr = nullptr,
                          uint32_t pcmLen = 0, uint32_t sampleRate = 16000,
                          uint16_t channels = 1);
    int  performStepFun(const uint8_t* audio, size_t audioLen,
                        const char* filename, String& outText);
    int  performMiMo(const uint8_t* audio, size_t audioLen,
                     const char* filename, String& outText,
                     const uint8_t* wavHdr = nullptr);
    int  finishRequest(int code, String& body, String& outText);
    int  parseTranscription(const String& body, String& outText);

    bool prepareHttp(bool sse);
    void appendErrBody(const uint8_t* data, size_t len);
    void logE(const char* fmt, ...) __attribute__((format(printf, 2, 3)));
    void logI(const char* fmt, ...) __attribute__((format(printf, 2, 3)));

    // 静态工具
    static size_t b64Encode(const uint8_t* in, size_t inLen, char* out);
    static void   buildWavHeader(uint8_t* hdr, uint32_t pcmLen,
                                 uint32_t sampleRate, uint16_t bits, uint16_t channels);
    static const char* mimeFromFilename(const char* filename);
    static const char* formatFromFilename(const char* filename);  // "wav"/"mp3"/"pcm"

    StreamHTTPClient _http;
    Print*           _log;
    AIKitLogLevel    _log_level;

    uint8_t  _vendor_id = (uint8_t)AIKIT_ASR_CUSTOM;
    const char* _vendor_name = "Custom";
    const AIKitASRVendorInfo* _preset = nullptr;   // begin() 保存的预设（含 style）
    String   _host;
    uint16_t _port = 443;
    String   _path;
    bool     _https = true;
    String   _api_key;
    String   _model;

    String   _language;             // 空 = 不发送
    String   _prompt;
    uint32_t _connect_timeout_ms = 10000;
    uint32_t _tcp_timeout_ms = AIKIT_ASR_HTTP_TIMEOUT_MS;

    bool         _insecure = true;
    const char*  _ca_cert = nullptr;
    bool         _ca_valid = false;
#if defined(ESP8266)
    BearSSL::X509List* _x509_ta = nullptr;
#endif

    int    _http_status = 0;
    int    _transport_err = 0;
    String _err_body;
};
