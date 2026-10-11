/*
  13_LoggingControl - 日志路由与级别控制 | Log routing and level control

  演示内容：
    - 日志目标由构造函数传入：任意 Print/Stream（Serial / Serial0 /
      蓝牙串口 / 网络 Telnet 流 / 文件 ...）
    - setLogLevel()：NONE / ERROR / WARN / INFO / DEBUG
    - DEBUG 级会打印完整请求体（注意别把含敏感信息的日志发到公网）
    - 传 nullptr = 完全静默（量产模式）
  What this demonstrates:
    - The log target is passed to the constructor: any Print/Stream
      (Serial / Serial0 / Bluetooth serial / a network Telnet stream /
      a file ...)
    - setLogLevel(): NONE / ERROR / WARN / INFO / DEBUG
    - DEBUG prints the full request body (never publish logs containing
      secrets to the public internet)
    - Pass nullptr = fully silent (production mode)

  适用于 ESP32 / ESP8266（ESP32 可用 Serial0/Serial1/Serial2） | Works on ESP32 / ESP8266 (Serial0/Serial1/Serial2 available on ESP32).
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

// 日志走默认串口 | logs go to the default serial port
AIKitClient ai(&Serial);
// 量产静音：AIKitClient ai(nullptr); | production silence: AIKitClient ai(nullptr);
// ESP32 双串口路由示例（日志走 Serial0，业务走 USB 串口）： | ESP32 dual-serial routing (logs to Serial0, business on the USB serial):
//   HardwareSerial& LogPort = Serial0;
//   LogPort.begin(115200);
//   AIKitClient ai(&LogPort);

void setup() {
  Serial.begin(115200);
  delay(200);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) { delay(300); Serial.print("."); }
  Serial.println("\n[WiFi] connected");

  // ---- 级别演示 ---- | log-level demo
  ai.setLogLevel(AIKIT_LOG_INFO);      // 默认：关键节点（begin / 流结束） | default: key milestones (begin / stream end)
  ai.begin(AIKIT_DEEPSEEK, API_KEY);

  String reply;
  ai.chat("只回复：好", reply);
  Serial.println("[Reply] " + reply);

  // DEBUG：额外打印请求/响应体片段 —— 排查参数透传问题的利器 | DEBUG: also prints request/response body snippets — a great aid for debugging parameter passthrough
  Serial.println("\n== 切到 DEBUG 级别再请求一次 ==");
  ai.setLogLevel(AIKIT_LOG_DEBUG);
  ai.chat("只回复：好", reply);

  // 静音： | silence:
  // ai.setLogLevel(AIKIT_LOG_NONE);
}

void loop() {}
