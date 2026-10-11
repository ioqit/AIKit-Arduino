/*
  06_VendorSwitch - 一行代码切换厂商 | Switch vendors in one line of code

  演示内容：
    - 同一个 AIKitClient 复用，begin() 不同厂商预设即可切换
    - 各厂商默认模型（详见 README《厂商API规格表》）
    - 自定义模型名：setModel()
  What this demonstrates:
    - Reuse one AIKitClient; switch by calling begin() with a different
      vendor preset
    - Each vendor's default model (see the README "Vendor API spec table")
    - Custom model names: setModel()

  说明：切换厂商会重置 model 为新厂商默认值（你 setModel() 指定的也会被
  重置——因为旧厂商的模型名在新厂商处无效；如需保留请切换后再 setModel）。
  Note: switching vendors resets the model to the new vendor's default (your
  setModel() choice is reset too — old vendor model names are invalid there;
  re-apply setModel after switching if you want to keep one).

  适用于 ESP32 / ESP8266 | Works on ESP32 / ESP8266.
*/
#if defined(ESP8266)
  #include <ESP8266WiFi.h>
#else
  #include <WiFi.h>
#endif
#include <AIKit.h>

const char* WIFI_SSID  = "your-ssid";
const char* WIFI_PASS  = "your-password";

// 各厂商一把钥匙（演示用占位；实际请分别填入） | one key per vendor (placeholders for the demo; fill in your own)
const char* KEY_DEEPSEEK     = "sk-your-deepseek-key";
const char* KEY_ZHIPU        = "your-zhipu-key";
const char* KEY_SILICONFLOW  = "sk-your-siliconflow-key";
const char* KEY_QWEN         = "sk-your-bailian-key";

AIKitClient ai(&Serial);

String ask(const char* question) {
  String reply;
  int err = ai.chat(question, reply);
  if (err != AIKIT_OK) {
    Serial.printf("[%s] error %d: %s (HTTP %d)\n",
                  ai.vendorName(), err, aikitErrorToString(err), ai.lastStatusCode());
    return String("");
  }
  return reply;
}

void setup() {
  Serial.begin(115200);
  delay(200);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) { delay(300); Serial.print("."); }
  Serial.println("\n[WiFi] connected");

  // ---- 厂商 A：DeepSeek（默认模型 deepseek-flash）---- | Vendor A: DeepSeek (default model deepseek-flash)
  ai.begin(AIKIT_DEEPSEEK, KEY_DEEPSEEK);
  Serial.println("\n=== " + String(ai.vendorName()) + " (" + ai.model() + ") ===");
  Serial.println(ask("用五个字形容今天"));

  // ---- 厂商 B：智谱 GLM（免费档 glm-4.7-flash）---- | Vendor B: Zhipu GLM (free tier glm-4.7-flash)
  ai.begin(AIKIT_ZHIPU, KEY_ZHIPU);
  Serial.println("\n=== " + String(ai.vendorName()) + " (" + ai.model() + ") ===");
  Serial.println(ask("推荐一款入门传感器"));

  // ---- 厂商 C：硅基流动（聚合平台，模型名带 厂商/ 前缀）---- | Vendor C: SiliconFlow (aggregator; model names carry a vendor/ prefix)
  ai.begin(AIKIT_SILICONFLOW, KEY_SILICONFLOW);
  ai.setModel("Qwen/Qwen3-8B");            // 任意平台内模型自行指定 | pick any model available on the platform
  Serial.println("\n=== " + String(ai.vendorName()) + " (" + ai.model() + ") ===");
  Serial.println(ask("I2C 总线最多挂多少设备？"));

  // ---- 厂商 D：阿里百炼 Qwen（国际站用 AIKIT_QWEN_DASHSCOPE_INTL）---- | Vendor D: Alibaba Bailian Qwen (international site: AIKIT_QWEN_DASHSCOPE_INTL)
  ai.begin(AIKIT_QWEN_DASHSCOPE, KEY_QWEN);
  Serial.println("\n=== " + String(ai.vendorName()) + " (" + ai.model() + ") ===");
  Serial.println(ask("一句话解释 PWM"));
}

void loop() {}
