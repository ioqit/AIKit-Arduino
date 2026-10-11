/*
  12_ErrorHandling - 错误处理全流程 | Error handling, the full pipeline

  演示内容：
    - AIKIT_ERR_* 错误码体系与逐级排障：
        CONFIG        → 忘了 begin() / 参数缺失
        BUSY          → 上一条流还没 close()
        CONNECT       → 网络/TLS 失败（配合 lastTransportError()）
        HTTP_STATUS   → 4xx/5xx（配合 lastStatusCode() + lastErrorBody()）
        PARSE         → 响应 JSON 异常
    - setTimeouts()：连接/读取超时（推理模型首包可能很慢）
    - 简单的指数退避重试
  What this demonstrates:
    - The AIKIT_ERR_* error-code family and step-by-step troubleshooting:
        CONFIG        → begin() forgotten / missing arguments
        BUSY          → the previous stream is not close()d yet
        CONNECT       → network/TLS failure (pair with lastTransportError())
        HTTP_STATUS   → 4xx/5xx (pair with lastStatusCode() + lastErrorBody())
        PARSE         → malformed response JSON
    - setTimeouts(): connect/read timeouts (the first packet of a reasoning
      model can be very slow)
    - A simple exponential-backoff retry

  适用于 ESP32 / ESP8266 | Works on ESP32 / ESP8266.
*/
#if defined(ESP8266)
  #include <ESP8266WiFi.h>
#else
  #include <WiFi.h>
#endif
#include <AIKit.h>

const char* WIFI_SSID = "your-ssid";
const char* WIFI_PASS = "your-password";
const char* API_KEY   = "sk-your-deepseek-api-key";

AIKitClient ai(&Serial);

void reportError(int err) {
  Serial.printf("[AIKit] error %d: %s\n", err, aikitErrorToString(err));
  switch (err) {
    case AIKIT_ERR_CONNECT:
      // 底层传输错误码（SHC_ERROR_*），可精确定位：拒连/超时/TLS... | low-level transport error code (SHC_ERROR_*): pinpoint refused / timeout / TLS ...
      Serial.printf("       transport=%d (%s)\n",
                    ai.lastTransportError(),
                    StreamHTTPClient::errorToString(ai.lastTransportError()).c_str());
      break;
    case AIKIT_ERR_HTTP_STATUS:
      // HTTP 状态 + 服务端返回的错误体（截断保留） | HTTP status + the server's error body (kept truncated)
      Serial.printf("       HTTP %d, body: %s\n",
                    ai.lastStatusCode(), ai.lastErrorBody().c_str());
      break;
    default:
      break;
  }
}

// 带指数退避的重试 | retry with exponential backoff
int chatWithRetry(const char* q, String& reply, int maxRetries = 3) {
  for (int attempt = 1; attempt <= maxRetries; attempt++) {
    int err = ai.chat(q, reply);
    if (err == AIKIT_OK) return AIKIT_OK;

    reportError(err);
    // 配置类错误重试也没用，直接返回 | retrying config-style errors is pointless, return now
    if (err == AIKIT_ERR_CONFIG || err == AIKIT_ERR_NULL_ARG ||
        err == AIKIT_ERR_BAD_JSON || err == AIKIT_ERR_TOO_MANY) {
      return err;
    }
    if (attempt < maxRetries) {
      uint32_t backoff = 1000UL << (attempt - 1);   // 1s, 2s, 4s...
      Serial.printf("       retry %d/%d in %ums ...\n",
                    attempt, maxRetries, (unsigned)backoff);
      delay(backoff);
    } else {
      return err;
    }
  }
  return AIKIT_ERR_TIMEOUT;
}

void setup() {
  Serial.begin(115200);
  delay(200);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) { delay(300); Serial.print("."); }
  Serial.println("\n[WiFi] connected");

  // 演示 CONFIG 错误：未 begin 直接 chat | demo CONFIG error: chat called before begin
  Serial.println("\n== 演示：未配置就调用 ==");
  String reply;
  int err = ai.chat("hi", reply);
  reportError(err);            // AIKIT_ERR_CONFIG

  // 正常配置 | proper configuration
  ai.begin(AIKIT_DEEPSEEK, API_KEY);
  ai.setTimeouts(10000, 30000);   // 连接 10s；读取 30s（推理模型友好） | 10s connect; 30s read (friendly to reasoning models)

  Serial.println("\n== 正常请求（含重试） ==");
  err = chatWithRetry("只回复两个字：成功", reply);
  if (err == AIKIT_OK) {
    Serial.println("[Reply] " + reply);
  }

  // 演示 HTTP_STATUS：故意用一个错 key 的请求体效果（这里用错误模型名模拟） | demo HTTP_STATUS: same effect as a bad key (simulated here with a bogus model name)
  Serial.println("\n== 演示：HTTP 错误（错误模型名） ==");
  ai.setModel("no-such-model-000");
  err = ai.chat("hi", reply);
  reportError(err);            // 多为 HTTP 404/400，错误体含厂商报错信息 | usually HTTP 404/400; the body carries the vendor's error message
  ai.setModel("deepseek-flash");   // 恢复 | restore
}

void loop() {}
