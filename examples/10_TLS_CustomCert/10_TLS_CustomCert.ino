/*
  10_TLS_CustomCert - 自定义根 CA 证书（生产环境推荐）

  演示内容：
    - setCACert(pem)：仅信任你指定的根证书（fail-closed，安全默认）
    - setInsecure(true/false)：跳过证书校验的开关（默认 true，便于上手）
    - ESP8266 下证书放 PROGMEM 的写法：setCACert_P(PSTR(...))

  证书获取：
    1. 浏览器打开厂商 API 域名（如 https://api.deepseek.com），
       查看证书链，导出“根 CA”的 PEM 文本；
    2. 或从厂商官方文档 / 公共根证书库获取；
    3. 粘贴到下方 ROOT_CA（BEGIN/END CERTIFICATE 两行都要）。

  注意：
    - 证书过期需更换；ESP32 亦可用 NTP 对时提升校验成功率。
    - insecure=true 仅建议开发期使用。

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

// ↓↓↓ 替换为你实际导出的根 CA 证书（PEM 格式）↓↓↓
const char* ROOT_CA =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw\n"
    "... 在这里粘贴完整的根证书行 ...\n"
    "-----END CERTIFICATE-----\n";

AIKitClient ai(&Serial);

void setup() {
  Serial.begin(115200);
  delay(200);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) { delay(300); Serial.print("."); }
  Serial.println("\n[WiFi] connected");

  ai.begin(AIKIT_DEEPSEEK, API_KEY);

  // ---- 生产姿势：指定根 CA 并关闭 insecure ----
  // 普通字符串字面量在 ESP8266 上由核心自动放入 flash（内存映射，可直接读取），
  // 两个平台都可用 setCACert()；若你显式使用了 PSTR()/PROGMEM，请改用
  // ESP8266 专属的 setCACert_P()。
  ai.setCACert(ROOT_CA);
  ai.setInsecure(false);        // 严格校验：证书不匹配则连接失败（fail-closed）

  // ---- 开发姿势（库的默认状态）：跳过校验，快速联调 ----
  // ai.setInsecure(true);

  String reply;
  int err = ai.chat("回复“握手成功”四个字", reply);
  if (err == AIKIT_OK) {
    Serial.println("[TLS OK] " + reply);
  } else {
    // 证书不匹配/过期/域名不对都会在这里体现为连接错误
    Serial.printf("[AIKit] error %d: %s\n", err, aikitErrorToString(err));
    Serial.printf("       transport: %d (%s)\n",
                  ai.lastTransportError(),
                  StreamHTTPClient::errorToString(ai.lastTransportError()).c_str());
  }
}

void loop() {}
