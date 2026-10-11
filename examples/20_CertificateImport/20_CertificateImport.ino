/*
  20_CertificateImport - 导入根证书做 TLS 校验 | Import root CA for TLS

  为什么 | Why:
    默认 setInsecure(true) 跳过证书校验，适合快速上手；生产环境应导入
    服务端根证书（CA），防范中间人攻击。本示例展示：
      1. 如何获得 PEM 证书（浏览器导出 / openssl 一行命令）
      2. 如何放进 PROGMEM 并在两个平台分别启用

  如何导出证书 | How to get the PEM:
    方法 A（浏览器）：打开 https://api.deepseek.com → 地址栏锁形图标 →
      查看证书 → 证书路径 → 选中根证书 → 导出为 Base64 PEM
    方法 B（命令行，推荐）：
      openssl s_client -connect api.deepseek.com:443 -showcerts \
        </dev/null 2>/dev/null | awk '/BEGIN CERT/,/END CERT/' > ca.pem
    ⚠ 下载最末一段（签发链的根 CA），不是叶子证书。
    ⚠ 根证书一般 5~10 年更换；厂商换根时需更新数组。

  验证 | Verify:
    运行后观察日志不再出现 "TLS certificate validation is DISABLED"。
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

// ---- 根证书存 PROGMEM（两平台 flash 均省 RAM）----
// 此处以 Let's Encrypt ISRG Root X1 为占位示例（api.deepseek.com 的
// 证书链根即签发自公共 CA，具体请按上方方法自行导出并替换整段 PEM）
static const char ROOT_CA_PEM[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwMDM5
WhcNMzUwNjA0MTEwMDM5WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfz54JyytF5
m4qTfD3LA97CbRvKlHNDdRh2S9FyHY4VJwzSSwtSgcc4s8C0jYb7
（示例截断——请用真实完整 PEM 替换本段）
（truncated placeholder - replace with your real full PEM）
MIIC5zCCAc+gAwIBAgIQBrh9mBh3vvd8AvE8iJk=
-----END CERTIFICATE-----
)EOF";

AIKitClient ai(&Serial);

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

  // ---- 启用证书校验 | enable certificate validation ----
#if defined(ESP8266)
  // ESP8266：PROGMEM 版本走 setCACert_P()，库内部惰性解析为
  // BearSSL::X509List 并缓存（PEM 必须完整且格式正确）
  ai.setCACert_P(ROOT_CA_PEM);
#else
  // ESP32：字符串字面量本身已在 flash，直接 setCACert()
  ai.setCACert(ROOT_CA_PEM);
#endif
  ai.setInsecure(false);   // 关闭放行 | disable the bypass

  Serial.println("[TLS] root CA installed, validation enabled");

  String reply;
  int err = ai.chat("你好", reply);
  if (err == AIKIT_OK) {
    Serial.println("[Reply] " + reply);
  } else {
    // 若此处报握手失败：多为 PEM 导错（叶子证书/截断）或已换根
    Serial.printf("[AIKit] error %d (%s), HTTP %d\n", err,
                  aikitErrorToString(err), ai.lastStatusCode());
    Serial.println("       检查 PEM 是否为完整根证书 | check your PEM");
  }
}

void loop() {}
