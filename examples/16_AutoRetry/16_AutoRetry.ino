/*
  16_AutoRetry - 失败自动重试（可配置次数与指数退避）
             | Automatic retry on failure (configurable count & backoff)

  演示内容 | What this demonstrates:
    - setRetry(maxRetries, baseDelayMs, maxDelayMs)：最多重试 N 次，
      第 n 次重试前等待 base << n 毫秒（封顶 maxDelayMs）
    - onRetry()：重试即将发生时的回调（记录日志 / 点灯 / 上报）
    - 触发条件：连接失败、超时、HTTP 429/5xx；以及厂商拒收
      stream_options.include_usage 时的自动降级重发
    - 已收到流式增量的请求不会重试（避免重复输出）；
      逐行读模式（chatStreamTo）不自动重试。

  本示例故意使用一个无效端点触发失败，观察重试日志。
  | This sketch intentionally uses a dead endpoint so you can watch
    the retry log; swap in a real vendor for the success path.
  适用于 ESP32 / ESP8266。
*/
#if defined(ESP8266)
  #include <ESP8266WiFi.h>
#else
  #include <WiFi.h>
#endif
#include <AIKit.h>

const char* WIFI_SSID = "your-ssid";
const char* WIFI_PASS = "your-password";

AIKitClient ai(&Serial);

void setup() {
  Serial.begin(115200);
  delay(200);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) { delay(300); Serial.print("."); }
  Serial.println("\n[WiFi] connected");

  // 故意指向一个连不上的端口，制造连接失败 | dead endpoint on purpose
  ai.beginCustom("192.0.2.1", 9999, "/v1/chat/completions",
                 /*useHttps=*/false, "dummy-key", "any-model");

  // 重试策略：最多 3 次，1s 起步指数退避，封顶 8s
  // | retry policy: up to 3 retries, exponential backoff from 1s, cap 8s
  ai.setRetry(3, 1000, 8000);

  // 重试回调（attempt 从 1 起 | attempt counts from 1）
  ai.onRetry([](uint8_t attempt, int lastErr) {
    Serial.printf("[Retry] attempt %u, last error %d (%s)\n",
                  attempt, lastErr, aikitErrorToString(lastErr));
  });

  Serial.println("[Demo] calling a dead endpoint, watch retries:");
  uint32_t t0 = millis();
  String reply;
  int err = ai.chat("hello", reply);
  uint32_t dt = millis() - t0;

  Serial.printf("[Demo] final error: %d (%s) after %ums\n",
                err, aikitErrorToString(err), (unsigned)dt);
  Serial.println("[Demo] 默认策略为最多 2 次重试（AIKIT_DEFAULT_MAX_RETRIES），");
  Serial.println("       setRetryEnabled(false) 可完全关闭自动重试。");
  Serial.println("       Default: up to 2 retries; disable via setRetryEnabled(false).");

  // ---- 真实场景：对真实厂商启用重试（反注释使用 | real-world usage) ----
  // ai.begin(AIKIT_DEEPSEEK, "sk-...");
  // ai.setRetry(3, 500, 10000);
  // ai.chat("hello", reply);
}

void loop() {}
