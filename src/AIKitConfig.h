/*
 * AIKitConfig.h - 编译期可调参数
 *
 * 所有宏均可在包含 AIKit.h 之前通过 -D 或 build_flags 覆盖。
 * 命名统一为 AIKIT_ 前缀，避免与其它库冲突。
 *
 * AIKit-Arduino (https://github.com/ioqite/AIKit-Arduino)
 */
#pragma once

// ---------------------------------------------------------------------------
// 自定义 HTTP 请求头容量（每个约占 2 个 String 的内存）
// ---------------------------------------------------------------------------
#ifndef AIKIT_MAX_CUSTOM_HEADERS
#define AIKIT_MAX_CUSTOM_HEADERS 8
#endif

// ---------------------------------------------------------------------------
// 自定义 JSON 字段容量（每个含 key + 原始 JSON 值文本）
// ---------------------------------------------------------------------------
#ifndef AIKIT_MAX_JSON_FIELDS
#define AIKIT_MAX_JSON_FIELDS 12
#endif

// ---------------------------------------------------------------------------
// SSE 单行最大长度（字节）。
// chat completions 的一个 data: 行通常在 100~2000 字节之间。
// 超过该长度的行会被丢弃并在下个换行处重新同步，同时置
// AIKIT_ERR_LINE_TOO_LONG。若你的模型单块输出很大（如 base64 图像），
// 可在构建选项中调大。
// ---------------------------------------------------------------------------
#ifndef AIKIT_SSE_LINE_MAX
#define AIKIT_SSE_LINE_MAX 2048
#endif

// ---------------------------------------------------------------------------
// [ESP32] 逐行读模式的行环形缓冲槽数。
// 消费者读取速度慢于服务端下发速度时，生产者会节流等待（背压），
// 因此该值不需要很大。每槽为一个 String。
// ---------------------------------------------------------------------------
#ifndef AIKIT_LINE_RING_SLOTS
#define AIKIT_LINE_RING_SLOTS 16
#endif

// ---------------------------------------------------------------------------
// [ESP32] 行环形缓冲写满时生产者的最长等待（毫秒）。
// 超时后置 AIKIT_ERR_BUFFER_OVERFLOW 并丢弃后续行（优雅降级，不崩溃）。
// ---------------------------------------------------------------------------
#ifndef AIKIT_LINE_PUSH_TIMEOUT_MS
#define AIKIT_LINE_PUSH_TIMEOUT_MS 30000
#endif

// ---------------------------------------------------------------------------
// [非 ESP32（如 ESP8266）] 逐行读模式行队列上限（行数）与字节上限。
// 这些平台上请求在 begin 阶段同步执行完毕，行先全部入队再由用户读出，
// 因此需要上限防止长回复耗尽内存。超限同样置 AIKIT_ERR_BUFFER_OVERFLOW。
// ---------------------------------------------------------------------------
#ifndef AIKIT_LINE_QUEUE_MAX_LINES
#define AIKIT_LINE_QUEUE_MAX_LINES 96
#endif
#ifndef AIKIT_LINE_QUEUE_MAX_BYTES
#define AIKIT_LINE_QUEUE_MAX_BYTES 32768
#endif

// ---------------------------------------------------------------------------
// 非 2xx 响应时保存的错误响应体最大字节数（lastErrorBody()）
// ---------------------------------------------------------------------------
#ifndef AIKIT_ERRBODY_MAX
#define AIKIT_ERRBODY_MAX 384
#endif

// ---------------------------------------------------------------------------
// 单条日志最大长度（含前缀）
// ---------------------------------------------------------------------------
#ifndef AIKIT_LOG_BUF
#define AIKIT_LOG_BUF 160
#endif

// ---------------------------------------------------------------------------
// 默认超时（毫秒）：连接 / 读
// LLM 首字节延迟可能较高（推理模型可达数十秒），读超时给足。
// ---------------------------------------------------------------------------
#ifndef AIKIT_DEFAULT_CONNECT_TIMEOUT_MS
#define AIKIT_DEFAULT_CONNECT_TIMEOUT_MS 10000
#endif
#ifndef AIKIT_DEFAULT_TCP_TIMEOUT_MS
#define AIKIT_DEFAULT_TCP_TIMEOUT_MS 30000
#endif

// ---------------------------------------------------------------------------
// 库版本
// ---------------------------------------------------------------------------
#define AIKIT_VERSION "1.2.0"
#define AIKIT_VERSION_MAJOR 1
#define AIKIT_VERSION_MINOR 2
#define AIKIT_VERSION_PATCH 0

// ---------------------------------------------------------------------------
// [v1.2.0 新增] 失败自动重试默认参数
//
// setRetry() 未被调用时：最多重试 AIKIT_DEFAULT_MAX_RETRIES 次，
// 指数退避起始 AIKIT_RETRY_BASE_DELAY_MS，封顶 AIKIT_RETRY_MAX_DELAY_MS。
// 触发重试的错误类型：连接/传输失败、超时、HTTP 429 / 5xx；
// 已收到流式增量后不再重试（避免重复输出）。
// ---------------------------------------------------------------------------
#ifndef AIKIT_DEFAULT_MAX_RETRIES
#define AIKIT_DEFAULT_MAX_RETRIES 2
#endif
#ifndef AIKIT_RETRY_BASE_DELAY_MS
#define AIKIT_RETRY_BASE_DELAY_MS 1000
#endif
#ifndef AIKIT_RETRY_MAX_DELAY_MS
#define AIKIT_RETRY_MAX_DELAY_MS 30000
#endif

// ---------------------------------------------------------------------------
// [v1.2.0 新增] 对话历史管理内存上限
//
// AIKitHistory 使用堆内存动态扩容，但设置两个安全阀防止耗尽堆：
//   AIKIT_HISTORY_MAX_MESSAGES —— 消息条数上限（默认 128 条）
//   AIKIT_HISTORY_MAX_BYTES    —— 角色与正文总字节数上限（默认 48 KB）
// 超限时 append() 返回 AIKIT_ERR_HISTORY_LIMIT，不静默丢弃。
// ---------------------------------------------------------------------------
#ifndef AIKIT_HISTORY_MAX_MESSAGES
#define AIKIT_HISTORY_MAX_MESSAGES 128
#endif
#ifndef AIKIT_HISTORY_MAX_BYTES
#define AIKIT_HISTORY_MAX_BYTES (48UL * 1024UL)
#endif

// ---------------------------------------------------------------------------
// [v1.2.0 新增] ASR（语音转文字）参数
//
// AIKIT_ASR_HTTP_TIMEOUT_MS  —— HTTP 转写整体超时（音频上传 + 推理）
// AIKIT_RTASR_RECV_BUF       —— 实时识别单帧接收/解压缓冲上限
// AIKIT_RTASR_CONNECT_MS     —— WebSocket 连接超时
// AIKIT_RTASR_PING_MS        —— 空闲时协议层 ping 间隔（保活）
// ---------------------------------------------------------------------------
#ifndef AIKIT_ASR_HTTP_TIMEOUT_MS
#define AIKIT_ASR_HTTP_TIMEOUT_MS 90000
#endif
// HTTP 转写请求体（multipart 或 base64 JSON）整体拼装的上限。
// 超限返回 AIKIT_ERR_PAYLOAD_TOO_BIG。ESP8266 无 PSRAM，建议调小
// （例如 16384）或将音频控制在数秒内；ESP32 可按堆余量放大。
#ifndef AIKIT_ASR_MAX_UPLOAD_BYTES
#define AIKIT_ASR_MAX_UPLOAD_BYTES 65536UL
#endif
#ifndef AIKIT_RTASR_RECV_BUF
#define AIKIT_RTASR_RECV_BUF 4096
#endif
#ifndef AIKIT_RTASR_CONNECT_MS
#define AIKIT_RTASR_CONNECT_MS 10000
#endif
#ifndef AIKIT_RTASR_PING_MS
#define AIKIT_RTASR_PING_MS 15000
#endif

// ---------------------------------------------------------------------------
// [v1.2.0 新增] 实时 ASR（WebSocket）是否可用：检测 ArduinoWebsockets 库
//
// 实时语音识别依赖 ArduinoWebsockets（gilmaimon，v0.5+）。该库为「可选依赖」：
// 安装后本库自动启用（AIKIT_HAS_RTASR=1）；未安装时库其余功能不受影响，
// AIKitRealtimeASR 的方法将返回 AIKIT_ERR_NOT_SUPPORTED。
// 手动强制开关：-D AIKIT_FORCE_RTASR=1 / 0
// ---------------------------------------------------------------------------
#if !defined(AIKIT_HAS_RTASR)
#  if defined(AIKIT_FORCE_RTASR)
#    define AIKIT_HAS_RTASR AIKIT_FORCE_RTASR
#  elif defined(__has_include)
#    if __has_include(<ArduinoWebsockets.h>)
#      define AIKIT_HAS_RTASR 1
#    else
#      define AIKIT_HAS_RTASR 0
#    endif
#  else
#    define AIKIT_HAS_RTASR 0
#  endif
#endif
