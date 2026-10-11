/*
  08_CustomHeaders - 自定义 HTTP 请求头（覆盖 / 追加） | Custom HTTP request headers (override / append)

  演示内容：
    - addHeader(name, value, replace)
        replace=true （默认）：同名头覆盖为单行（可覆盖库默认的
                             Content-Type / Accept 等标准头）
        replace=false        ：同名头追加为重复行（多值头场景）
    - removeHeader() / clearHeaders()
    - 请求头在每次请求时自动重新应用（库内部处理，无需重复添加）
  What this demonstrates:
    - addHeader(name, value, replace)
        replace=true (default): a same-name header is replaced with a
                             single line (can override the library's default
                             Content-Type / Accept standard headers)
        replace=false       : a same-name header is appended as a duplicate
                             line (multi-value header scenarios)
    - removeHeader() / clearHeaders()
    - Headers are re-applied automatically on every request (handled inside
      the library, no need to re-add them)

  典型用途：网关鉴权头、链路追踪头、自定义 User 标识等。
  Typical uses: gateway auth headers, trace headers, custom user markers, etc.

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
const char* API_KEY   = "sk-your-siliconflow-key";

AIKitClient ai(&Serial);

void setup() {
  Serial.begin(115200);
  delay(200);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) { delay(300); Serial.print("."); }
  Serial.println("\n[WiFi] connected");

  ai.begin(AIKIT_SILICONFLOW, API_KEY);

  // ---- 覆盖库默认头（replace=true，默认值）---- | Override library default headers (replace=true, the default)
  // 例：把 Content-Type 改成带 charset 的版本 | e.g. switch Content-Type to a charset-aware version
  ai.addHeader("Content-Type", "application/json; charset=utf-8");

  // ---- 追加业务自定义头 ---- | Append custom business headers
  ai.addHeader("X-Client-Id", "esp32-weather-station-01");

  // ---- 多值头：replace=false 允许同名头重复出现 ---- | Multi-value headers: replace=false lets same-name headers repeat
  ai.addHeader("X-Tag", "iot",     false);
  ai.addHeader("X-Tag", "esp32",   false);

  // ---- 移除某个头（按名字，忽略大小写）---- | Remove a header (by name, case-insensitive)
  // ai.removeHeader("X-Client-Id");

  String reply;
  int err = ai.chat("回复 ok 两个字母即可", reply);
  if (err == AIKIT_OK) {
    Serial.println("[Reply] " + reply);
  } else {
    Serial.printf("[AIKit] error %d: %s (HTTP %d)\n",
                  err, aikitErrorToString(err), ai.lastStatusCode());
    Serial.println("[Body] " + ai.lastErrorBody());
  }
}

void loop() {}
