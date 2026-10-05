/*
  02_ChatStreamCallback - 流式对话（onDelta 回调）

  演示内容：
    - chatStream() + onDelta()：回复逐字上屏，体验“打字机”效果
    - 两个平台（ESP32/ESP8266）行为一致：回调在请求期间被增量触发

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

void setup() {
  Serial.begin(115200);
  delay(200);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) { delay(300); Serial.print("."); }
  Serial.println("\n[WiFi] connected");

  ai.begin(AIKIT_DEEPSEEK, API_KEY);

  // 每收到一块增量文本就触发（参数：未解码 UTF-8 字节指针与长度）
  ai.onDelta([](const char* delta, size_t len) {
    Serial.write((const uint8_t*)delta, len);   // 直接写串口，无需拼接
  });

  Serial.println("\n[AIKit] streaming ...");
  Serial.println("------- reply -------");
  int err = ai.chatStream("从 1 数到 10，每个数字占一行");
  Serial.println("\n---------------------");

  if (err == AIKIT_OK) {
    Serial.printf("[AIKit] done. finish_reason=%s\n", ai.lastFinishReason());
  } else {
    Serial.printf("[AIKit] error %d: %s (HTTP %d)\n",
                  err, aikitErrorToString(err), ai.lastStatusCode());
  }
}

void loop() {}
