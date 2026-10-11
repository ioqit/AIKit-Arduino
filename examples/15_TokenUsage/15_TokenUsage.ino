/*
  15_TokenUsage - Token 用量统计 | Token usage statistics

  演示内容 | What this demonstrates:
    - 非流式请求的 usage（prompt/completion/total tokens）
    - 流式请求的 usage（自动附带 stream_options.include_usage，
      若厂商拒收会自动降级重发一次）
    - totalUsage()：跨请求累计用量（会话级账单估算）

  改成你自己的 WiFi 与 API Key 即可运行 | Fill in your WiFi & API key.
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
const char* API_KEY   = "sk-your-deepseek-api-key";

AIKitClient ai(&Serial);

// 打印一条 usage | Print one usage record
void printUsage(const char* tag, AIKitUsage u) {
  if (u.valid()) {
    // 多数厂商按 token 计费：total = prompt + completion | Most vendors bill by token
    Serial.printf("[%s] prompt=%d completion=%d total=%d\n",
                  tag, u.promptTokens, u.completionTokens, u.totalTokens);
  } else {
    Serial.printf("[%s] usage not reported by server\n", tag);
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) { delay(300); Serial.print("."); }
  Serial.println("\n[WiFi] connected");

  if (!ai.begin(AIKIT_DEEPSEEK, API_KEY)) {
    Serial.println("[AIKit] begin() failed");
    return;
  }

  // ---- 1) 非流式：usage 直接来自响应 JSON | non-stream: usage from response ----
  AIKitChatResult r1;
  int err = ai.chat("用一个词形容 Arduino", r1);
  if (err == AIKIT_OK) {
    Serial.println("[Reply] " + r1.content);
    printUsage("non-stream", r1.usage);
  }

  // ---- 2) 流式：默认已开启 include_usage | stream: enabled by default ----
  // 若个别厂商对 stream_options 报 400，库会自动去掉该字段并重发一次
  ai.onDelta([](const char* delta, size_t len) {
    Serial.write(delta, len);
  });
  Serial.println("\n[stream reply]");
  err = ai.chatStream("数到五，用逗号分隔");
  if (err == AIKIT_OK) printUsage("stream", ai.lastUsage());

  // ---- 3) 累计统计 | cumulative statistics ----
  // 两次请求的 token 都已计入 | tokens of both requests accumulated
  printUsage("TOTAL", ai.totalUsage());
  ai.resetTotalUsage();          // 需要时清零 | reset when needed
}

void loop() {}
