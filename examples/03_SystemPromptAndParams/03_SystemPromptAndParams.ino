/*
  03_SystemPromptAndParams - system 提示词与常用参数 | System prompt & common parameters

  演示内容：
    - setSystemPrompt()：人设/角色设定（自动作为第一条 system 消息前置）
    - setTemperature() / setMaxTokens() / setTopP()
    - setMaxCompletionTokens()：Moonshot / OpenAI 新规范字段
    - AIKitChatResult：完整结果（正文 + finish_reason + token 用量）
  What this demonstrates:
    - setSystemPrompt(): persona / role setup (automatically prepended as
      the first system message)
    - setTemperature() / setMaxTokens() / setTopP()
    - setMaxCompletionTokens(): the newer Moonshot / OpenAI spec field
    - AIKitChatResult: the full result (content + finish_reason + token usage)

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
const char* API_KEY   = "sk-your-moonshot-api-key";

AIKitClient ai(&Serial);

void setup() {
  Serial.begin(115200);
  delay(200);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) { delay(300); Serial.print("."); }
  Serial.println("\n[WiFi] connected");

  ai.begin(AIKIT_MOONSHOT, API_KEY);       // 例：月之暗面 Kimi | e.g. Moonshot Kimi
  ai.setModel("kimi-k3");                  // 不设则用厂商默认模型 | omit to use the vendor's default model

  // ---- 人设与参数（begin 后任意时刻可改，下一次请求生效）---- | Persona & params (changeable any time after begin; effective on the next request)
  ai.setSystemPrompt("你是一个只说文言文的 Arduino 电路管家，回答不超过两句。");
  ai.setTemperature(0.7f);                 // 0~2，越高越发散 | 0~2, higher = more random
  ai.setMaxTokens(512);                    // 写入 max_tokens 字段 | writes the max_tokens field
  // Moonshot / OpenAI 新模型已弃用 max_tokens，改用 max_completion_tokens： | Newer Moonshot / OpenAI models deprecate max_tokens in favor of max_completion_tokens:
  // ai.setMaxCompletionTokens(512);
  ai.setTopP(0.9f);                        // 可选；也可用 addJsonField 注入 | optional; can also be injected via addJsonField

  AIKitChatResult result;
  int err = ai.chat("灯泡不亮了，可能是什么原因？", result);
  if (err == AIKIT_OK) {
    Serial.println("\n[Reply] " + result.content);
    Serial.printf("[Meta] finish_reason=%s\n", result.finishReason.c_str());
    if (result.usage.valid()) {
      Serial.printf("[Meta] tokens: prompt=%d completion=%d total=%d\n",
                    result.usage.promptTokens,
                    result.usage.completionTokens,
                    result.usage.totalTokens);
    }
  } else {
    Serial.printf("[AIKit] error %d: %s\n", err, aikitErrorToString(err));
  }
}

void loop() {}
