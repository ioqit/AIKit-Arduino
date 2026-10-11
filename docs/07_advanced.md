# 07 - 进阶：错误码、内存、CI 与路线图

**[English](../en/07_advanced.md)** | 简体中文

## 错误码全表

`chat*` / `append*` 等调用返回 `AIKIT_OK(0)` 或负数 `AIKIT_ERR_*`；非 2xx HTTP 统一映射为 `AIKIT_ERR_HTTP_STATUS`，具体状态码与响应体经 `lastStatusCode()` / `lastErrorBody()` 查询。取值区间（-101 起）与底层 StreamHTTPClient 的 `SHC_ERROR_*`（-1 ~ -20）**刻意错开**，日志中一眼可分；底层错误原样保存在 `lastTransportError()`。`aikitErrorToString(err)` 返回可读描述（静态字符串，不分配内存）。

| 错误码 | 值 | 含义 / 排查方向 |
| --- | --- | --- |
| `AIKIT_OK` | 0 | 成功 |
| `AIKIT_ERR_CONFIG` | -101 | 未 `begin()` / 缺 API Key / 端点未配置 |
| `AIKIT_ERR_NULL_ARG` | -102 | 传入空指针（或空 role 等退化参数） |
| `AIKIT_ERR_TOO_MANY` | -103 | 自定义请求头 / JSON 字段超出容量（见 [docs/01](01_installation.md) 宏表） |
| `AIKIT_ERR_BAD_JSON` | -104 | 自定义 JSON 字段值不是合法 JSON（设置时即报） |
| `AIKIT_ERR_CONNECT` | -105 | 连接 / 发送失败 → 查 `lastTransportError()` |
| `AIKIT_ERR_HTTP_STATUS` | -106 | HTTP 非 2xx → 查 `lastStatusCode()` + `lastErrorBody()` |
| `AIKIT_ERR_PARSE` | -107 | 响应 JSON 解析失败或结构不符 |
| `AIKIT_ERR_OOM` | -108 | 内存不足（JSON 构建 / 任务创建） |
| `AIKIT_ERR_TIMEOUT` | -109 | 超时（默认读超时 30 s，见 `setTimeouts`） |
| `AIKIT_ERR_BUSY` | -110 | 已有请求 / 流进行中 → 先 `resp.close()` 或 `end()` |
| `AIKIT_ERR_LINE_TOO_LONG` | -111 | SSE 单行超过 `AIKIT_SSE_LINE_MAX`（该行丢弃，流继续重新同步） |
| `AIKIT_ERR_BUFFER_OVERFLOW` | -112 | 逐行读缓冲溢出（平台差异见 [docs/04](04_streaming.md)） |
| `AIKIT_ERR_STREAM_CLOSED` | -113 | 流已关闭 |
| `AIKIT_ERR_NOT_SUPPORTED` | -114 | 当前平台 / 配置不支持（如未装 ArduinoWebsockets 时的实时 ASR） |
| `AIKIT_ERR_ASR_AUDIO` | -115 | *v1.2.0* 音频数据为空 / 格式或参数不被该厂商接受 |
| `AIKIT_ERR_ASR_PROTOCOL` | -116 | *v1.2.0* ASR 响应协议解析失败（结构不符预期） |
| `AIKIT_ERR_WS` | -117 | *v1.2.0* WebSocket 连接 / 握手 / 通信失败 |
| `AIKIT_ERR_NOT_CONNECTED` | -118 | *v1.2.0* WebSocket 未连接（先 `connect()`） |
| `AIKIT_ERR_INFLATE` | -119 | *v1.2.0* gzip 响应解压失败（火山引擎二进制协议） |
| `AIKIT_ERR_HISTORY_LIMIT` | -120 | *v1.2.0* 对话历史超出容量安全阀（见 [docs/06](06_history.md)） |
| `AIKIT_ERR_PAYLOAD_TOO_BIG` | -121 | *v1.2.0* 音频 / 请求体超出可发送上限（`AIKIT_ASR_MAX_UPLOAD_BYTES`） |

```cpp
int err = ai.chat("你好", reply);
if (err != AIKIT_OK) {
  Serial.printf("[AIKit] %d: %s\n", err, aikitErrorToString(err));
  if (err == AIKIT_ERR_HTTP_STATUS) {
    Serial.printf("HTTP %d, body: %s\n", ai.lastStatusCode(), ai.lastErrorBody().c_str());
  }
}
```

错误处理完整示范见 [examples/12_ErrorHandling](../examples/12_ErrorHandling/12_ErrorHandling.ino)。

## 内存使用建议

| 平台 | 建议 |
| --- | --- |
| **ESP32** | 非流式响应由传输层全量缓冲——数千 token 的回复约几十 KB，通常无需担心。逐行读模式峰值 ≈ 槽数 × 单行长度（默认 16 × ~1 KB）；开了 PSRAM 更宽裕 |
| **ESP8266** | 用户堆约 40 KB，BearSSL TLS 握手独占 ~25 KB 空闲堆。长回复**用 onDelta 回调**（边收边出、不驻留内存，见 [docs/04](04_streaming.md)）；`setMaxTokens()` 限制回复长度；逐行读模式关注 `AIKIT_LINE_QUEUE_*` 上限；避免与大缓冲库共存 |
| **ASR（音频上传）** | 请求体在堆上**整体拼装**（multipart / base64），上限 `AIKIT_ASR_MAX_UPLOAD_BYTES`（默认 64 KB）。**ESP8266 建议音频 ≤12 KB**（16 kHz PCM16 约 6 秒）或编译期调小该宏；ESP32 可按堆余量放大。超限返回 `AIKIT_ERR_PAYLOAD_TOO_BIG` 而非 OOM 崩溃 |
| **对话历史** | 两道安全阀：默认 128 条 / 48 KB（[docs/06](06_history.md)），`totalBytes()` 监控水位，`setWindow()` 控制增长 |

所有错误路径均优雅降级（返回错误码），**不存在 OOM 崩溃或重启路径**。相关宏的编译期覆盖方法见 [docs/01_installation.md](01_installation.md)。

## GitHub Actions CI（手动编译矩阵）

本仓库带一个**仅手动触发**的编译验证工作流（`.github/workflows/compile.yml`）——不设 push / PR / 定时触发，按需运行，不消耗公共 runner 配额：

| 项 | 配置 |
| --- | --- |
| 触发方式 | `workflow_dispatch` **手动**（无 push / PR / schedule） |
| 矩阵 | 4 板卡：`esp32` / `esp32s3` / `esp32c3` / `nodemcuv2`（ESP8266）× **全部示例**（21 个） |
| 依赖 | ArduinoJson + ArduinoWebsockets 走库管理器；StreamHTTPClient 从 GitHub clone |
| 产物 | 每板卡一份 `compile-report-<板名>` 编译报告（每个示例的 PASS/FAIL 与体积）；任一失败则任务失败 |

**用法**：仓库页 → *Actions* 标签 → 左侧选 **Compile Matrix** → 右侧 **Run workflow** → 可勾选 `Also compile ESP8266 (nodemcuv2)`（默认勾选；去掉可跳过 ESP8266、只跑三块 ESP32，节省时间）→ 点绿色按钮。

适用场景：改动了库源码 / 新增示例后验证全平台可编译；或 fork 后验证你自己的改动。

## Roadmap

v0.1 时代 Roadmap 列出的下列能力**已在 v1.2.0 交付**，从待办移除：✅ 用量统计（`totalUsage`）、✅ 自动重试（`setRetry`）、✅ 对话历史管理（`AIKitHistory`）、✅ ASR（HTTP 转写 + 实时识别）、✅ CI 编译矩阵。

当前待办（按优先级，欢迎在 Issues 提需求 / 投票）：

| 方向 | 说明 | 现阶段的过渡方案 |
| --- | --- | --- |
| **Tool Calls 一等公民 API** | 请求构建 + 流式 `tool_calls` delta 聚合 | `addJsonField("tools", ...)` 透传，见 [examples/09_CustomJsonFields](../examples/09_CustomJsonFields/09_CustomJsonFields.ino) |
| **Vision 多模态** | `content` 数组（`image_url`）便捷构建器 | 同上，自定义 JSON 字段透传 |
| **Embeddings** | `/v1/embeddings` 端点 | `beginCustom()` + 自行发请求 |
| **模型列表** | 拉取并解析 `/v1/models` | — |
| **Pico W / SAMD 实测** | 架构兼容（跟随 StreamHTTPClient），出官方支持声明 | 欢迎反馈实测结果 |
| **Keep-Alive 连接池** | 多厂商实例并发复用连接 | 每实例独立连接 |
| **TTS 语音合成** | 文本转语音端点 | — |

架构预留：`AIKitMessage` 保持轻量指针语义，便于日后挂多模态 `content`；自定义 JSON 通道是所有未落地功能的通用透传口；`AIKIT_ERR_NOT_SUPPORTED` 是未来接口占位的统一返回值。

## 已知限制

| 限制 | 说明 / 缓解 |
| --- | --- |
| 默认 `insecure=true` | TLS 证书校验默认跳过（无 NTP / 无根证书环境快速上手的取舍）。**生产环境务必 `setCACert()` + `setInsecure(false)`**，见 [examples/10_TLS_CustomCert](../examples/10_TLS_CustomCert/10_TLS_CustomCert.ino) 与 [examples/20_CertificateImport](../examples/20_CertificateImport/20_CertificateImport.ino) |
| 非流式全量载入内存 | 非流式响应在解析前完整缓冲（见上文内存建议）；超长回复请改用流式 |
| ESP8266 逐行读为缓冲模式 | 请求在 `chatStreamTo()` 内同步完成、行入队后读出（带上限）；真增量需求用 onDelta 回调，见 [docs/04](04_streaming.md) |
| Mistral 实时 ASR 实验性 | 上行事件协议按 OpenAI 同构实现，未经真机验证（详见 [docs/05_asr.md](05_asr.md)） |
| API Key 明文传输 | 请求内 Key 为明文——**不要公开包含 Key 的 DEBUG 级日志**；本地部署预设（Ollama 等）无此问题 |

---

更多主题：[01 安装与依赖](01_installation.md) · [02 厂商预设](02_vendors.md) · [03 Chat API](03_chat.md) · [04 流式](04_streaming.md) · [05 语音识别](05_asr.md) · [06 对话历史](06_history.md)
