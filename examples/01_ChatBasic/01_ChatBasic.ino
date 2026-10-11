/*
  01_ChatBasic - 最简非流式对话 | Minimal non-streaming chat

  演示内容：
    - 一键接入厂商（begin）
    - 非流式 chat()：发一句话，拿完整回复
    - 基本错误检查
  What this demonstrates:
    - One-line vendor onboarding (begin)
    - Non-streaming chat(): send one prompt, get the complete reply
    - Basic error checking

  改成你自己的 WiFi 与 API Key 即可运行 | Fill in your own WiFi & API key to run.
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

AIKitClient ai(&Serial);   // 日志输出到 Serial（传 nullptr 可完全静默） | log to Serial (nullptr = fully silent)

void setup() {
  Serial.begin(115200);
  delay(200);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) { delay(300); Serial.print("."); }
  Serial.println("\n[WiFi] connected");

  // 一键接入：厂商预设 + API Key | One-line onboarding: vendor preset + API key
  // 可选厂商见 README《厂商API规格表》：DEEPSEEK / MOONSHOT / QWEN_DASHSCOPE /
  // ZHIPU / SILICONFLOW / MINIMAX / OPENAI ...（含 _INTL 国际站变体）
  // Available vendors (see the README "Vendor API spec table"): DEEPSEEK / MOONSHOT /
  // QWEN_DASHSCOPE / ZHIPU / SILICONFLOW / MINIMAX / OPENAI ... (incl. _INTL variants)
  if (!ai.begin(AIKIT_DEEPSEEK, API_KEY)) {
    Serial.println("[AIKit] begin() failed, check API key");
    return;
  }

  String reply;
  int err = ai.chat("用一句话向 Arduino 玩家打个招呼", reply);
  if (err == AIKIT_OK) {
    Serial.println("\n[Reply] " + reply);
  } else {
    Serial.printf("[AIKit] error %d: %s\n", err, aikitErrorToString(err));
    Serial.printf("       HTTP status: %d\n", ai.lastStatusCode());
  }
}

void loop() {}
