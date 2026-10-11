/*
  14_FullFeatured - 综合演练：多轮 + 流式 + 自定义参数 + 错误处理 | Full-feature drill: multi-turn + streaming + custom parameters + error handling

  演示内容（大杂烩）：
    - 厂商预设 + setModel + system prompt + temperature/max_tokens
    - 自定义 HTTP 头（覆盖 + 追加）
    - 自定义 JSON 字段（追加 + 覆盖）
    - 非流式多轮 → 流式单轮（onDelta）→ 流式逐行读（AIKitStreamResponse）
    - 用量统计 / finish_reason / 完整错误处理
  What this demonstrates (a bit of everything):
    - Vendor preset + setModel + system prompt + temperature/max_tokens
    - Custom HTTP headers (override + append)
    - Custom JSON fields (append + override)
    - Non-streaming multi-turn → streaming single-turn (onDelta) →
      streaming line-by-line reading (AIKitStreamResponse)
    - Usage stats / finish_reason / complete error handling

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
AIKitStreamResponse resp;

bool ensureWiFi() {
  if (WiFi.status() == WL_CONNECTED) return true;
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - t0 > 20000) return false;
    delay(300);
  }
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(200);
  if (!ensureWiFi()) { Serial.println("[WiFi] failed"); return; }
  Serial.println("\n[WiFi] connected");

  // ---------- 配置 ---------- | Configuration
  ai.begin(AIKIT_DEEPSEEK, API_KEY);
  ai.setSystemPrompt("你是嵌入在 ESP32 里的助手，回答尽量短。");
  ai.setTemperature(0.5f);
  ai.setMaxTokens(256);
  ai.setTimeouts(10000, 30000);

  ai.addHeader("X-Client-Id", "esp32-demo", true);   // 覆盖式自定义头 | override-style custom header
  ai.addHeader("X-Session", "setup", false);         // 追加式自定义头 | append-style custom header
  ai.addJsonField("top_p", 0.9f);                    // JSON 追加 | JSON append
  ai.addJsonField("frequency_penalty", 0.0f);

  // ---------- 1) 非流式多轮 ---------- | 1) non-streaming multi-turn
  Serial.println("\n===== 1) 非流式多轮 =====");
  AIKitMessage history[] = {
    { "user", "记住暗号：香蕉。回复：已记住。" },
  };
  String r1;
  int e1 = ai.chat(history, 1, r1);
  if (e1 == AIKIT_OK) {
    Serial.println("[R1] " + r1);
  } else {
    Serial.printf("[R1] error %d: %s (HTTP %d)\n",
                  e1, aikitErrorToString(e1), ai.lastStatusCode());
  }

  String secret = "香蕉";   // 演示：多轮中 assistant 历史可来自 String | demo: assistant history can come from a String in multi-turn
  (void)secret;
  AIKitMessage turns[] = {
    history[0],
    { "assistant", r1.c_str() },
    { "user", "暗号是什么？只回答暗号。" },
  };
  AIKitChatResult r2;
  int e2 = ai.chat(turns, 3, r2);
  if (e2 == AIKIT_OK) {
    Serial.println("[R2] " + r2.content);
    if (r2.usage.valid()) {
      Serial.printf("[R2] tokens: %d (prompt %d + completion %d), finish=%s\n",
                    r2.usage.totalTokens, r2.usage.promptTokens,
                    r2.usage.completionTokens, r2.finishReason.c_str());
    }
  } else {
    Serial.printf("[R2] error %d: %s (HTTP %d)\n",
                  e2, aikitErrorToString(e2), ai.lastStatusCode());
  }

  // ---------- 2) 流式：onDelta ---------- | 2) streaming: onDelta
  Serial.println("\n===== 2) 流式 onDelta =====");
  ai.onDelta([](const char* d, size_t n) { Serial.write((const uint8_t*)d, n); });
  int err = ai.chatStream("用一句话总结今天学到的：流式响应");
  Serial.printf("\n[Meta] err=%d finish=%s\n", err, ai.lastFinishReason());

  // ---------- 3) 流式：逐行读 ---------- | 3) streaming: line-by-line reading
  Serial.println("\n===== 3) 流式逐行读 =====");
  err = ai.chatStreamTo("从 1 数到 5", resp);
  if (err == AIKIT_OK) {
    String line, piece;
    while (resp.readLine(line)) {
      if (AIKitStreamResponse::isDoneSentinel(line)) break;
      if (AIKitStreamResponse::extractDelta(line, piece)) Serial.print(piece);
    }
    resp.close();
    Serial.printf("\n[Meta] HTTP=%d done=%d\n", resp.statusCode(),
                  resp.doneSentinelSeen() ? 1 : 0);
  } else {
    Serial.printf("[AIKit] chatStreamTo error %d: %s\n", err, aikitErrorToString(err));
  }
}

void loop() {}
