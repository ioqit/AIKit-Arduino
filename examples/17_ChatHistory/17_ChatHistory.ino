/*
  17_ChatHistory - 对话历史管理 | Conversation history management

  演示内容 | What this demonstrates:
    - AIKitHistory：动态扩容的多轮对话上下文（自动 user/assistant 入史）
    - ai.chat(history, ...)：一条调用完成「入史 → 请求 → 回复入史」
    - setWindow()：滑动窗口（保留开头 system 消息，丢弃最旧对话）
    - exportJSON() / importJSON()：OpenAI messages 格式的导出 / 导入，
      可持久化到 LittleFS / SD / EEPROM 后恢复会话
    - popBack()：回滚最近一条（例如删除不理想的回复重试）

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
AIKitHistory history;

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

  // ---- 1) 常驻 system 人设 + 滑动窗口 ----
  // keepSystem=true 时裁剪窗口会尽量保留开头的 system 消息
  history.appendSystem("你是一个 Arduino 助手，回答保持在 30 字以内。");
  history.setWindow(8, /*keepSystem=*/true);   // 最多 8 条消息

  // ---- 2) 自动多轮对话（两轮演示）----
  // 每次调用：user 入史 → 携带全部历史请求 → 回复入史
  String reply;
  ai.chat(history, "我叫小明，用 ESP32 做智能家居", reply);
  Serial.println("[Turn1] " + reply);

  // 第二轮：上一轮的称呼被记住（历史在上下文里）
  ai.chat(history, "我叫什么名字？", reply);
  Serial.println("[Turn2] " + reply);

  // ---- 3) 导出 / 导入（可保存到文件系统后恢复会话）----
  String json = history.exportJSON();
  Serial.printf("[Export] %u messages, %u bytes:\n%s\n",
                (unsigned)history.size(), (unsigned)history.totalBytes(),
                json.c_str());

  history.clear();
  int rc = history.importJSON(json);           // 原子导入：失败不变现状
  Serial.printf("[Import] rc=%d, restored %u messages\n",
                rc, (unsigned)history.size());

  // ---- 4) 回滚一条 | roll back one entry ----
  history.popBack();
  Serial.printf("[PopBack] now %u messages\n", (unsigned)history.size());

  // ---- 5) 手动组装（也可以完全不依赖自动入史）----
  // history.appendUser("手动提问");
  // ai.chat(history.messages(), history.size(), reply);
}

void loop() {}
