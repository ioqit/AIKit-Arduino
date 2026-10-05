/*
  07_CustomEndpoint - 接入任意 OpenAI 兼容端点

  演示内容：
    - beginCustom()：自建网关 / vLLM / Ollama(OpenAI兼容) / OneAPI /
      NewAPI / 任意中转站，只需 主机 + 端口 + 路径 + https 开关
    - HTTP（非加密）与 HTTPS 端点各一例

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

AIKitClient ai(&Serial);

void setup() {
  Serial.begin(115200);
  delay(200);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) { delay(300); Serial.print("."); }
  Serial.println("\n[WiFi] connected");

  // ---- 例 1：局域网 vLLM / Ollama（HTTP 明文，端口自定）----
  //   POST http://192.168.1.10:8000/v1/chat/completions
  ai.beginCustom("192.168.1.10",           // host
                 8000,                     // port
                 "/v1/chat/completions",   // path
                 false,                    // https = false
                 "dummy-key",              // vLLM 常可随意填；Ollama 默认无需鉴权
                 "Qwen/Qwen3-8B");         // 默认模型（也可后续 setModel）
  ai.setInsecure(false);                   // 明文 HTTP 本就不走 TLS

  String reply;
  int err = ai.chat("本地模型在线吗？回复一个词", reply);
  Serial.printf("[LAN] err=%d reply=%s\n", err, reply.c_str());

  // ---- 例 2：某个 HTTPS 中转网关 ----
  ai.beginCustom("gateway.example.com",
                 443,
                 "/v1/chat/completions",
                 true,                     // https
                 "sk-your-gateway-key",
                 "gpt-4o-mini");
  ai.setInsecure(true);                    // 演示用；生产请 setCACert()

  err = ai.chat("ping", reply);
  if (err == AIKIT_OK) {
    Serial.println("[Gateway] " + reply);
  } else {
    Serial.printf("[Gateway] error %d: %s\n", err, aikitErrorToString(err));
  }
}

void loop() {}
