# 01 - 安装与依赖

**[English](../en/01_installation.md)** | 简体中文

本库**不是零依赖库**：核心功能（对话 / 流式 / 历史管理 / HTTP 转写）依赖两个必装库，实时语音识别另有一个可选库。装齐依赖后，5 分钟即可跑通第一个示例。

## 依赖一览

| 库 | 必要性 | 用途 | 获取方式 |
| --- | --- | --- | --- |
| [ArduinoJson](https://arduinojson.org/) **v7+** | **必装** | 请求 JSON 构建 / 响应解析 | 库管理器直接安装 |
| [StreamHTTPClient](https://github.com/ioqit/StreamHTTPClient) **v1.0+** | **必装** | HTTP/HTTPS 传输层（同步 / 异步 / SSE / gzip） | **未上架库管理器**，从 GitHub 安装 |
| [ArduinoWebsockets](https://github.com/gilmaimon/ArduinoWebsockets) **v0.5+** | **可选** | 仅实时语音识别（WebSocket）需要 | 库管理器直接安装 |

三个库各司其职：

- **StreamHTTPClient** —— 负责所有 HTTP/HTTPS 收发：非流式的同步请求、SSE 流式的分块回调 / 逐行推送、gzip 解压、TLS（ESP8266 走 BearSSL）。本库的执行模型（ESP32 后台任务 / ESP8266 同步）也由它决定，详见 [docs/04_streaming.md](04_streaming.md) 的平台差异表。
- **ArduinoJson** —— 负责请求体拼装与响应解析（v7 的 `JsonDocument` API）。未装或版本过低会直接编译失败。
- **ArduinoWebsockets** —— 仅 `AIKitRealtimeASR`（实时识别）用到，详见下文「可选依赖机制」与 [docs/05_asr.md](05_asr.md)。

## 方式一：Arduino IDE

1. 安装 ArduinoJson：*工具 → 管理库*，搜索 `ArduinoJson` 安装（需要 v7+）。
2. 安装 StreamHTTPClient：它**暂未收录在库管理器索引中**，请下载 [zip 包](https://github.com/ioqit/StreamHTTPClient/archive/refs/heads/master.zip)，然后 *项目 → 导入库 → 添加 .ZIP 库*。
3. 安装 AIKit-Arduino 本体：同样下载 [zip](https://github.com/ioqit/AIKit-Arduino/archive/refs/heads/master.zip) 后走 *添加 .ZIP 库*；升级时先删除旧的库目录再重新导入。
4. （可选，仅实时 ASR 需要）在库管理器中搜索 `ArduinoWebsockets` 安装。

## 方式二：arduino-cli

```bash
# 必装依赖（ArduinoWebsockets 一并装上，覆盖实时 ASR 示例）
arduino-cli lib install ArduinoJson ArduinoWebsockets

# StreamHTTPClient 不在索引中，git clone 到用户库目录
git clone https://github.com/ioqit/StreamHTTPClient.git ~/Arduino/libraries/StreamHTTPClient

# 本库
git clone https://github.com/ioqit/AIKit-Arduino.git ~/Arduino/libraries/AIKit-Arduino
```

Windows 下用户库目录一般是 `Documents\Arduino\libraries\`。

## 方式三：git clone（适合跟踪上游 / 参与开发）

```bash
cd ~/Arduino/libraries
git clone https://github.com/ioqit/StreamHTTPClient.git
git clone https://github.com/ioqit/AIKit-Arduino.git
cd AIKit-Arduino && git checkout v1.2.0    # 固定到发布版本（可选）
```

## PlatformIO 用户

`platformio.ini` 的 `lib_deps` 写法（三个库一次配齐）：

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
lib_deps =
  bblanchon/ArduinoJson@^7
  https://github.com/ioqit/StreamHTTPClient.git
  gilmaimon/ArduinoWebsockets@^0.5
```

- GitHub 直链形式的依赖（StreamHTTPClient）每次构建会执行 `git clone`，首次较慢属正常现象；
- 不需要实时语音识别时可删掉 `ArduinoWebsockets` 一行，其余功能不受影响；
- ESP8266 工程把 `platform`/`board` 换成 `espressif8266` / `nodemcuv2` 即可（注意内存，见 [docs/07_advanced.md](07_advanced.md)）。

## 可选依赖机制（ArduinoWebsockets）

实时 ASR 采用「**装了自动启用，没装优雅降级**」的检测机制，无需任何手工配置：

| 环节 | 行为 |
| --- | --- |
| 编译期检测 | `AIKitConfig.h` 用 `__has_include(<ArduinoWebsockets.h>)` 探测该库：装了 → `AIKIT_HAS_RTASR=1`；没装 → `=0` |
| 未安装时 | 库的其余功能（对话 / 流式 / 历史 / HTTP 转写）**完全不受影响**；仅 `AIKitRealtimeASR` 的方法统一返回 `AIKIT_ERR_NOT_SUPPORTED`，日志给出提示 |
| 强制开关 | 编译期定义 `AIKIT_FORCE_RTASR=1`（强制启用）或 `AIKIT_FORCE_RTASR=0`（强制禁用），覆盖自动检测——例如装了库但想暂时关闭实时 ASR 编译路径 |

```ini
; PlatformIO 强制关闭实时 ASR 的示例
build_flags = -D AIKIT_FORCE_RTASR=0
```

## 编译期配置宏总表

所有宏均可在包含 `AIKit.h` 之前通过 `-D` 或 `build_flags` 覆盖（`#ifndef` 保护，无需修改库源码）。未特别标注的宏两个平台通用。

### 基础宏（v0.1 起提供）

| 宏 | 默认值 | 用途 |
| --- | --- | --- |
| `AIKIT_MAX_CUSTOM_HEADERS` | 8 | 自定义 HTTP 请求头容量（`addHeader`），超出返回 `AIKIT_ERR_TOO_MANY` |
| `AIKIT_MAX_JSON_FIELDS` | 12 | 自定义 JSON 字段容量（`addJsonField`） |
| `AIKIT_SSE_LINE_MAX` | 2048 | SSE 单行最大字节数；超长行被丢弃并在下个换行处重新同步（置 `AIKIT_ERR_LINE_TOO_LONG`）。模型单块输出很大时可调大 |
| `AIKIT_LINE_RING_SLOTS` | 16 | [仅 ESP32] 逐行读模式的行环形缓冲槽数。消费者慢时生产者会背压等待，无需很大 |
| `AIKIT_LINE_PUSH_TIMEOUT_MS` | 30000 | [仅 ESP32] 环形缓冲写满时生产者的最长等待（ms），超时置 `AIKIT_ERR_BUFFER_OVERFLOW` 优雅降级 |
| `AIKIT_LINE_QUEUE_MAX_LINES` | 96 | [仅非 ESP32] 逐行读模式行队列的行数上限 |
| `AIKIT_LINE_QUEUE_MAX_BYTES` | 32768 | [仅非 ESP32] 行队列的字节上限（防长回复耗尽 ESP8266 堆） |
| `AIKIT_ERRBODY_MAX` | 384 | 非 2xx 时保留的错误响应体长度（`lastErrorBody()`） |
| `AIKIT_LOG_BUF` | 160 | 单条日志最大长度（含前缀） |
| `AIKIT_DEFAULT_CONNECT_TIMEOUT_MS` | 10000 | 默认连接超时（`setTimeouts` 未调用时） |
| `AIKIT_DEFAULT_TCP_TIMEOUT_MS` | 30000 | 默认读超时（推理模型首字节可达数十秒，故给足） |

### v1.2.0 新增宏

| 宏 | 默认值 | 用途 |
| --- | --- | --- |
| `AIKIT_DEFAULT_MAX_RETRIES` | 2 | 未调用 `setRetry()` 时失败自动重试的默认次数 |
| `AIKIT_RETRY_BASE_DELAY_MS` | 1000 | 指数退避起始等待（第 n 次重试前等 `base << n` 毫秒） |
| `AIKIT_RETRY_MAX_DELAY_MS` | 30000 | 指数退避封顶等待 |
| `AIKIT_HISTORY_MAX_MESSAGES` | 128 | 对话历史条数安全阀（`AIKitHistory`），超出 `append` 返回 `AIKIT_ERR_HISTORY_LIMIT` |
| `AIKIT_HISTORY_MAX_BYTES` | 48 KB | 对话历史 role+content 总字节数安全阀 |
| `AIKIT_ASR_HTTP_TIMEOUT_MS` | 90000 | HTTP 一句话转写整体超时（音频上传 + 推理） |
| `AIKIT_ASR_MAX_UPLOAD_BYTES` | 65536 | HTTP 转写请求体（multipart / base64）拼装上限，超限返回 `AIKIT_ERR_PAYLOAD_TOO_BIG`。ESP8266 建议调小（如 16384） |
| `AIKIT_RTASR_RECV_BUF` | 4096 | 实时识别单帧接收 / 解压缓冲上限 |
| `AIKIT_RTASR_CONNECT_MS` | 10000 | 实时识别 WebSocket 连接超时 |
| `AIKIT_RTASR_PING_MS` | 15000 | 实时识别空闲期的协议层 ping 间隔（保活） |
| `AIKIT_FORCE_RTASR` | （未定义） | 强制启用 / 禁用实时 ASR 编译路径，覆盖 `__has_include` 自动检测，见上文 |

另有只读的版本宏 `AIKIT_VERSION`（`"1.2.0"`）、`AIKIT_VERSION_MAJOR/MINOR/PATCH`，供条件编译使用。

## 验证安装

装好后编译一个最小示例（即 [examples/01_ChatBasic](../examples/01_ChatBasic/01_ChatBasic.ino)）：

```cpp
#include <AIKit.h>

AIKitClient ai(&Serial);

void setup() {
  Serial.begin(115200);
  // ...连接 WiFi 后...
  ai.begin(AIKIT_DEEPSEEK, "sk-xxx");   // 19 家预设见 docs/02_vendors.md
  String reply;
  ai.chat("你好", reply);
}
void loop() {}
```

- 若报 `AIKit.h: No such file or directory` → 本库没装对位置；
- 若报 `StreamHTTPClient.h: No such file or directory` → 传输层依赖缺失（注意它**不在**库管理器索引里）；
- 若报 ArduinoJson 相关模板错误 → 检查 ArduinoJson 是否 v7+（v6 的 API 不同）。

下一步：[docs/02_vendors.md](02_vendors.md) 挑厂商，或 [docs/03_chat.md](03_chat.md) 查完整 API。
