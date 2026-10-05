/*
  04_MultiTurnChat - 多轮对话

  演示内容：
    - 用 AIKitMessage 数组自管对话历史（user/assistant 交替）
    - system prompt 的两种来源：setSystemPrompt() 或显式传入 system 消息

  说明：本库不替你保管历史（MCU 内存有限），你可以在循环里把
  上一轮回复 append 到数组，或落盘到 LittleFS/SD。

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
const char* API_KEY   = "sk-your-zhipu-api-key";

AIKitClient ai(&Serial);

void setup() {
  Serial.begin(115200);
  delay(200);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) { delay(300); Serial.print("."); }
  Serial.println("\n[WiFi] connected");

  ai.begin(AIKIT_ZHIPU, API_KEY);          // 例：智谱 GLM（glm-4.7-flash 免费档）
  ai.setSystemPrompt("你是一个细心的对话助手，回答保持简洁。");

  // ---- 第一轮：静态历史 ----
  // 注意：AIKitMessage 只保存指针，传入的 String 须在 chat() 返回前保持有效
  AIKitMessage history[] = {
    { "user",      "我最喜欢的开发板是 ESP32。" },
    { "assistant", "记住了！ESP32 双核 240MHz，带 WiFi 和蓝牙，做 IoT 很合适。" },
    { "user",      "帮我起一个寝室灯光控制项目的名字。" },
  };

  String round1Reply;
  int err = ai.chat(history, 3, round1Reply);
  if (err != AIKIT_OK) {
    Serial.printf("[AIKit] error %d: %s\n", err, aikitErrorToString(err));
    return;
  }
  Serial.println("[Round 1] " + round1Reply);

  // ---- 第二轮：把第一轮回复追加进历史（String 生命周期在本次作用域内）----
  String userMsg2 = "再给两个英文名备选。";
  AIKitMessage turns[] = {
    history[0],
    history[1],
    history[2],
    { "assistant", round1Reply.c_str() },   // 第一轮的回复进入历史
    { "user",      userMsg2.c_str() },
  };

  String round2Reply;
  err = ai.chat(turns, 5, round2Reply);
  if (err == AIKIT_OK) {
    Serial.println("[Round 2] " + round2Reply);
  } else {
    Serial.printf("[AIKit] error %d: %s\n", err, aikitErrorToString(err));
  }
}

void loop() {}
