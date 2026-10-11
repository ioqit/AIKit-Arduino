/*
  09_CustomJsonFields - 自定义 JSON 字段（覆盖 / 追加到最终请求体） | Custom JSON fields (override / append into the final request body)

  演示内容：
    - addJsonField(key, jsonValue, replace)
        replace=true ：覆盖库已生成的同名字段（model/temperature/...均可改写）
        replace=false：仅当字段不存在时追加（不动库生成的参数）
    - 值必须是合法 JSON 文本：数字 / 带引号字符串 / 对象 / 数组 / bool
    - 便捷重载：float / int / bool / addJsonFieldString(自动加引号转义)
    - 高级参数透传：tools / response_format / 思考开关 等厂商扩展参数
  What this demonstrates:
    - addJsonField(key, jsonValue, replace)
        replace=true : override a same-name field the library already
                       generated (model/temperature/... can all be rewritten)
        replace=false: append only when the field is absent (library-generated
                       parameters stay untouched)
    - Values must be valid JSON text: numbers / quoted strings / objects /
      arrays / bool
    - Convenience overloads: float / int / bool / addJsonFieldString
      (auto-quotes and escapes)
    - Advanced parameter passthrough: tools / response_format / thinking
      toggles and other vendor extension parameters

  这是本库的“逃生舱”：官方 JSON 参数没做成 setter 的，都能从这里进。
  This is the library's "escape hatch": any official JSON parameter without a
  dedicated setter can go in here.

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
const char* API_KEY   = "sk-your-bailian-key";

AIKitClient ai(&Serial);

void setup() {
  Serial.begin(115200);
  delay(200);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) { delay(300); Serial.print("."); }
  Serial.println("\n[WiFi] connected");

  ai.begin(AIKIT_QWEN_DASHSCOPE, API_KEY);
  ai.setSystemPrompt("你是 JSON 输出机器，只输出 JSON，不要多余文字。");
  ai.setTemperature(0.7f);        // 库生成 temperature=0.7 | the library emits temperature=0.7

  // ---- 数字 / 布尔便捷重载（replace 默认 true，但字段不存在时等效追加）---- | Numeric / bool overloads (replace defaults to true; appends when the field is absent)
  ai.addJsonField("top_p", 0.9f);              // 浮点 → "top_p":0.9 | float → "top_p":0.9
  ai.addJsonField("presence_penalty", 0.1f);   // 惩罚参数直通 | penalty parameter passthrough
  ai.addJsonField("enable_thinking", false);   // 布尔 → false | bool → false

  // ---- 覆盖库生成的字段（replace=true 时直接改写）---- | Override a library-generated field (replace=true rewrites it)
  ai.addJsonField("temperature", "0.2", true); // 0.7 → 0.2，最终生效 0.2 | 0.7 → 0.2, the effective value is 0.2

  // ---- 原始 JSON 透传：对象 / 数组 / 字符串 ---- | Raw JSON passthrough: objects / arrays / strings
  // 强制 JSON 输出模式： | force JSON output mode:
  ai.addJsonField("response_format", "{\"type\":\"json_object\"}");
  // 透传 tools（Tool Calls 预留通道，未来版本将有一等公民 API）： | passthrough tools (reserved channel for tool calls; a first-class API is coming)
  ai.addJsonField("tools",
      "[{\"type\":\"function\",\"function\":{"
      "\"name\":\"get_weather\",\"description\":\"查询城市天气\","
      "\"parameters\":{\"type\":\"object\","
      "\"properties\":{\"city\":{\"type\":\"string\"}},"
      "\"required\":[\"city\"]}}}]");
  // 业务字符串字段（自动加引号并转义）： | business string field (auto-quoted and escaped):
  ai.addJsonFieldString("user_metadata", "device=esp32 fw=1.0.0");

  // 想移除某字段： | to remove a field:
  // ai.removeJsonField("tools");

  String reply;
  int err = ai.chat("返回 {\"city\":\"上海\",\"temp\":23} 这样的天气对象", reply);
  if (err == AIKIT_OK) {
    Serial.println("[Reply] " + reply);
  } else {
    Serial.printf("[AIKit] error %d: %s\n", err, aikitErrorToString(err));
    Serial.println("[Body] " + ai.lastErrorBody());
  }
}

void loop() {}
