/*
 * AIKitRealtimeASR.h - 实时语音识别（WebSocket 流式 ASR，v1.2.0）
 *
 * 一句话上手：
 *   AIKitRealtimeASR rt(&Serial);
 *   rt.begin(AIKIT_RT_DASHSCOPE, "sk-...");
 *   rt.onFinal([](const char* text){ Serial.println(text); });
 *   rt.connect();
 *   rt.sendAudio(pcm, len);   // 16 kHz / 16 bit / 单声道 PCM 帧
 *   rt.loop();                // 主循环里驱动
 *   rt.finish();
 *
 * 支持的厂商（协议各异，内部自动适配；端点均经官方文档核实 2026-10）：
 *   字节火山   Seed-ASR / 豆包流式语音识别（二进制协议，响应自动 gunzip）
 *   阿里百炼   paraformer-realtime-v2（run-task 双工推理协议）
 *   百度       实时语音识别 realtime_asr（START / MID_TEXT / FIN_TEXT）
 *   FunASR     自部署 runtime 2pass（wss://host:10095，无鉴权）
 *   OpenAI     Realtime Transcription（session.update + base64 PCM24k，
 *              库内部自动完成 16k→24k 线性重采样）
 *   阶跃星辰   realtime/asr/stream（OpenAI 同构事件流）
 *   Mistral    Realtime Transcription（实验性：客户端上行事件协议未完全核实）
 *
 * ⚠ 依赖：ArduinoWebsockets（gilmaimon，v0.5+，可选依赖）。
 *   未安装时本类全部方法返回 AIKIT_ERR_NOT_SUPPORTED，库其余功能不受影响：
 *     arduino-cli lib install "ArduinoWebsockets"
 *
 * ⚠ 关于 Seed-ASR / SenseVoice / FireRedASR / Kimi-Audio / MiMo-Audio /
 *   Qwen-ASR / Fun-ASR / StepAudio / 百度语音 的完整支持矩阵（含哪些厂商
 *   仅提供 HTTP 转写、哪些仅有开源权重无云端 API），见 docs/05_asr.md。
 *
 * ⚠ ESP8266 + wss（TLS）需要 BearSSL 约 22 KB 堆；连接期间避免大内存操作。
 *   火山引擎响应 gunzip 复用 StreamHTTPClient 内置的 miniz。
 *
 * AIKit-Arduino (https://github.com/ioqit/AIKit-Arduino)
 */
#pragma once

#include <Arduino.h>
#include <functional>

#include "AIKitConfig.h"
#include "AIKitTypes.h"

#if AIKIT_HAS_RTASR
namespace websockets { class WebsocketsClient; }
#endif

// ---------------------------------------------------------------------------
// 实时 ASR 厂商预设编号
// ---------------------------------------------------------------------------
enum AIKitRTVendor : uint8_t {
    AIKIT_RT_VOLC      = 0,   // 字节跳动火山引擎（豆包 / Seed-ASR）
    AIKIT_RT_DASHSCOPE = 1,   // 阿里云百炼 paraformer-realtime-v2
    AIKIT_RT_BAIDU     = 2,   // 百度智能云实时语音识别
    AIKIT_RT_FUNASR    = 3,   // FunASR 自部署 runtime（2pass）
    AIKIT_RT_OPENAI    = 4,   // OpenAI Realtime Transcription
    AIKIT_RT_STEPFUN   = 5,   // 阶跃星辰 StepAudio 实时识别
    AIKIT_RT_MISTRAL   = 6,   // Mistral Realtime（实验性）
    AIKIT_RT_COUNT     = 7,
    AIKIT_RT_CUSTOM    = 0xFF // beginFunASR() 指定自部署 host
};

// ---------------------------------------------------------------------------
// 实时 ASR 客户端
//
// 使用顺序：begin() → onPartial/onFinal 回调 → connect() → 循环
// { sendAudio(...) + loop() } → finish()。connect/finish 阻塞，sendAudio 非阻塞。
// ---------------------------------------------------------------------------
class AIKitRealtimeASR {
public:
    explicit AIKitRealtimeASR(Print* log = nullptr);
    ~AIKitRealtimeASR();

    // -------------------------------------------------------------------
    // 接入
    // -------------------------------------------------------------------
    // 通用入口（凭据含义随厂商，见下）：
    //   DASHSCOPE / OPENAI / STEPFUN / MISTRAL：key1 = API Key（key2 忽略）
    //   VOLC：  key1 = App ID（控制台应用 ID），key2 = Access Token
    //   BAIDU： key1 = App ID（数字），     key2 = API Key
    //   FUNASR：无需凭据（请用 beginFunASR()）
    bool begin(AIKitRTVendor vendor, const char* key1, const char* key2 = nullptr);
    bool begin(AIKitRTVendor vendor, const String& key1, const String& key2 = String());

    // 火山引擎（可选自定义 Resource Id，默认小时版 volc.bigasr.sauc.duration）
    bool beginVolc(const char* appId, const char* accessToken,
                   const char* resourceId = nullptr);

    // 百度（appid 为数字字符串，如 "10523417"）
    bool beginBaidu(const char* appId, const char* appKey);

    // FunASR 自部署（runtime websocket 服务，默认端口 10095，无鉴权；
    // TLS 由服务端是否开启 wss 决定：useTls=true 时连 wss://）
    bool beginFunASR(const char* host, uint16_t port = 10095, bool useTls = false);

    // -------------------------------------------------------------------
    // 参数（connect() 前设置）
    // -------------------------------------------------------------------
    void setModel(const char* model);       // 见各厂商默认：paraformer-realtime-v2 /
    void setModel(const String& model);     // gpt-live-transcribe / stepaudio-2.5-asr-stream ...
    void setLanguage(const char* lang);     // "zh"/"en"/nullptr（部分厂商支持）
    // 百度 dev_pid：15372 普通话加强标点（默认）/ 15376 方言 / 17372 英语加强标点
    void setBaiduDevPid(int pid) { _baidu_dev_pid = pid; }
    void setInsecure(bool yes) { _insecure = yes; }         // wss 证书校验（默认 true=放行）
    void setCACert(const char* pem) { _ca_cert = pem; _ca_valid = (pem != nullptr); }

    // -------------------------------------------------------------------
    // 回调
    // -------------------------------------------------------------------
    // 中间结果：本句到目前为止的完整文本（识别中，可能被后续修正）
    void onPartial(std::function<void(const char* text)> cb) { _on_partial = cb; }
    // 定稿结果：一句话识别完成（句号级），文本不再变化
    void onFinal(std::function<void(const char* text)> cb) { _on_final = cb; }
    // 错误（协议/网络/服务端拒绝）；随后连接通常已关闭
    void onError(std::function<void(int err, const char* msg)> cb) { _on_error = cb; }
    // 连接就绪（握手 + 启动指令完成，可以开始 sendAudio）
    void onConnected(std::function<void()> cb) { _on_connected = cb; }
    // 连接关闭（含 finish() 正常收尾）
    void onClosed(std::function<void()> cb) { _on_closed = cb; }

    // -------------------------------------------------------------------
    // 生命周期
    // -------------------------------------------------------------------
    // 建立 WebSocket 连接并发送厂商启动指令（阻塞，含 TLS 握手；
    // 需等待就绪确认的厂商会阻塞至 task-started / session.created）。
    int connect();

    // 发送一帧音频（16 kHz / 16 bit / 单声道 PCM）。
    // 建议每帧 100~200 ms（3200~6400 字节），帧间隔与音频时长一致。
    // OpenAI 路径内部自动重采样 24 kHz 并 base64；火山路径自动封二进制帧。
    int sendAudio(const uint8_t* pcm, size_t len);

    // 手动提交一轮音频（仅 OpenAI / StepFun / Mistral 的手动轮次模式；
    // 其余厂商为 no-op）。服务端返回本轮 completed 后触发 onFinal。
    int commitTurn();

    // 结束识别：发送厂商结束信号并阻塞等待最终结果（waitMs 上限），
    // 随后关闭连接。返回 AIKIT_OK=收到最终结果；AIKIT_ERR_TIMEOUT=超时。
    int finish(uint32_t waitMs = 8000);

    // 主循环驱动：处理接收数据 + 空闲保活（ping / HEARTBEAT）。
    // 必须在 loop() 中高频调用。
    void loop();

    // 立即关闭（不等待剩余结果）
    void close();

    bool connected() const { return _connected; }
    bool ready() const { return _state == RT_READY; }

    // 最近一次错误（onError 也会给出）
    int lastErrorCode() const { return _last_err; }
    const char* lastErrorText() const { return _last_err_text.c_str(); }
    const char* vendorName() const;

private:
    enum RTState : uint8_t { RT_IDLE, RT_READY, RT_FINISHING, RT_CLOSED };

    // ---- 内部：厂商协议适配 ----
    String buildStartUrl();
    bool   applyAuthHeaders();          // Authorization / X-Api-* 头
    bool   sendStartCommand();          // 首包（JSON / 二进制）
    int    sendAudioBinary(const uint8_t* pcm, size_t len);
    int    sendAudioVolc(const uint8_t* pcm, size_t len, bool lastPacket);
    int    sendAudioB64(const uint8_t* pcm, size_t len);   // OpenAI/StepFun/Mistral
    void   sendFinishCommand();
    void   pump(uint32_t waitMs, bool untilFinished);

    // ---- 内部：消息分发 ----
    void handleMessageText(const char* data, size_t len);
    void handleMessageBinary(const uint8_t* data, size_t len);
    void handleVolcBinary(const uint8_t* data, size_t len);
    void emitPartial(const char* text);
    void emitFinal(const char* text);
    void emitError(int err, const char* msg);
    bool gunzipToString(const uint8_t* in, size_t len, String& out);

    void logE(const char* fmt, ...) __attribute__((format(printf, 2, 3)));
    void logI(const char* fmt, ...) __attribute__((format(printf, 2, 3)));

    // ---- 成员 ----
    Print*     _log;
    AIKitLogLevel _log_level;

    uint8_t    _vendor = (uint8_t)AIKIT_RT_CUSTOM;
    String     _key1;                 // API Key / AppID
    String     _key2;                 // Access Token / API Key(百度)
    String     _volc_resource = "volc.bigasr.sauc.duration";
    String     _model;                // 空 = 用厂商默认
    String     _language;
    int        _baidu_dev_pid = 15372;
    String     _task_id;              // DashScope run-task 任务号
    String     _funasr_host;          // FunASR 自部署
    uint16_t   _funasr_port = 10095;
    bool       _funasr_tls = false;

    bool       _insecure = true;
    const char* _ca_cert = nullptr;
    bool       _ca_valid = false;

    std::function<void(const char*)> _on_partial;
    std::function<void(const char*)> _on_final;
    std::function<void(int, const char*)> _on_error;
    std::function<void()> _on_connected;
    std::function<void()> _on_closed;

#if AIKIT_HAS_RTASR
    websockets::WebsocketsClient* _ws = nullptr;
#endif
    bool       _connected = false;
    RTState    _state = RT_IDLE;
    bool       _finished = false;      // finish() 语义的终态
    int32_t    _volc_seq = 0;          // 火山消息序号（首包=1，音频从 2 起）
    size_t     _volc_utt_reported = 0; // 已上报的定稿 utterance 数
    String     _sent_buf;              // 增量型厂商的句子累积（onPartial 统一语义）
    uint32_t   _last_audio_ms = 0;
    uint32_t   _last_ping_ms = 0;

    int        _last_err = AIKIT_OK;
    String     _last_err_text;
};
