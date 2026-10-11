/*
  11_ReasoningDelta - 推理模型思维链（reasoning_content） | Reasoning-model chain of thought (reasoning_content)

  演示内容：
    - onReasoningDelta()：思维链增量回调（推理模型专属）
    - onDelta()：正文增量回调（两者可同时使用）
    - 非流式模式下思维链保存在 AIKitChatResult.reasoningContent
  What this demonstrates:
    - onReasoningDelta(): incremental chain-of-thought callback
      (reasoning models only)
    - onDelta(): incremental answer callback (both can be used together)
    - In non-streaming mode the chain of thought is kept in
      AIKitChatResult.reasoningContent

  支持情况（输出 reasoning_content / thinking 的模型）随厂商而异：
    - DeepSeek 推理系列、Kimi 思考系列、GLM thinking、Qwen 思考模式 等
    - 是否开启思考常由模型名或请求参数决定，见各厂商文档
      （例如 GLM：addJsonField("thinking","{\"type\":\"enabled\"}")）
  Vendor support varies (models that emit reasoning_content / thinking):
    - DeepSeek reasoning series, Kimi thinking series, GLM thinking, Qwen
      thinking mode, etc.
    - Whether thinking is enabled is usually decided by the model name or
      request parameters; see each vendor's docs
      (e.g. GLM: addJsonField("thinking","{\"type\":\"enabled\"}"))

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
const char* API_KEY   = "sk-your-zhipu-api-key";

AIKitClient ai(&Serial);

void setup() {
  Serial.begin(115200);
  delay(200);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) { delay(300); Serial.print("."); }
  Serial.println("\n[WiFi] connected");

  ai.begin(AIKIT_ZHIPU, API_KEY);
  ai.setModel("glm-4.7-flash");

  // 思维链：灰色区域打印（这里用前缀区分） | chain-of-thought: print dimmed (distinguished by prefix here)
  ai.onReasoningDelta([](const char* delta, size_t len) {
    Serial.write((const uint8_t*)delta, len);
  });
  // 正文：正常打印 | final answer: print normally
  ai.onDelta([](const char* delta, size_t len) {
    Serial.write((const uint8_t*)delta, len);
  });

  Serial.println("----- thinking + answer -------");
  int err = ai.chatStream("一个笼子里有鸡兔共 35 个头、94 只脚，鸡兔各几只？写出推理。");
  Serial.println("\n-------------------------------");

  if (err != AIKIT_OK) {
    Serial.printf("[AIKit] error %d: %s\n", err, aikitErrorToString(err));
  } else {
    Serial.printf("[Meta] finish_reason=%s\n", ai.lastFinishReason());
  }

  // 非流式等价用法： | Non-streaming equivalent:
  //   AIKitChatResult r; ai.chat("...", r);
  //   r.reasoningContent 为完整思维链，r.content 为最终答案 | r.reasoningContent = full chain-of-thought, r.content = final answer
}

void loop() {}
