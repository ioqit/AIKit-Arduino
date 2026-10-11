/*
  05_ChatStreamLineReader - 流式对话（Stream 逐行读） | Streaming chat (line-by-line Stream reading)

  演示内容：
    - chatStreamTo() + AIKitStreamResponse：像读串口一样逐行读取 SSE 流
    - readLine()：拿到原始行（如 "data: {...}"，不含换行）
    - extractDelta()：从一行中直接取出增量文本（无需自己写 JSON 解析）
    - isDoneSentinel()：识别 "data: [DONE]" 结束哨兵
    - 原始行也完全可自行用 ArduinoJson 解析（拿到 usage / finish_reason 等）
  What this demonstrates:
    - chatStreamTo() + AIKitStreamResponse: read the SSE stream line by line,
      just like reading a serial port
    - readLine(): get the raw line (e.g. "data: {...}", without the newline)
    - extractDelta(): pull the delta text straight out of a line (no JSON
      parsing of your own required)
    - isDoneSentinel(): recognize the "data: [DONE]" terminator
    - Raw lines can also be parsed yourself with ArduinoJson (to get
      usage / finish_reason etc.)

  平台差异：
    ESP32    ：请求由后台任务推进，readLine() 边收边读（真流式）
    ESP8266  ：请求在 chatStreamTo() 内同步完成，行入队后读出（带上限）
               长回复建议改用 02 示例的 onDelta 回调模式
  Platform differences:
    ESP32   : the request is driven by a background task; readLine() reads
              while data arrives (true streaming)
    ESP8266 : the request completes synchronously inside chatStreamTo();
              lines are queued then read out (bounded) — for long replies
              prefer the onDelta callback mode of example 02

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

AIKitClient ai(&Serial);
AIKitStreamResponse resp;    // 逐行读取器（继承自 Stream，可复用） | line reader (inherits from Stream, reusable)

void setup() {
  Serial.begin(115200);
  delay(200);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) { delay(300); Serial.print("."); }
  Serial.println("\n[WiFi] connected");

  ai.begin(AIKIT_DEEPSEEK, API_KEY);

  Serial.println("------- reply (line mode) -------");
  int err = ai.chatStreamTo("写一首关于 ESP32 的四行小诗", resp);
  if (err != AIKIT_OK && err != AIKIT_ERR_HTTP_STATUS) {
    Serial.printf("[AIKit] start error %d: %s\n", err, aikitErrorToString(err));
    return;
  }

  // ---- 逐行读取 ---- | Read line by line
  String line;
  String piece;
  while (resp.readLine(line)) {                 // 阻塞直至新行或流结束 | blocks until a new line or end of stream
    if (AIKitStreamResponse::isDoneSentinel(line)) {
      Serial.println("\n[AIKit] got [DONE] sentinel");
      break;
    }
    // 便捷提取：增量正文直接上屏 | convenience extraction: print the delta text directly
    if (AIKitStreamResponse::extractDelta(line, piece)) {
      Serial.print(piece);
      continue;
    }
    // 其它行（注释行 / 空行 / 含 usage 的末块）——需要时可自行解析： | Other lines (comment lines / blank lines / the final chunk with usage) — parse them yourself if needed:
    //   JsonDocument doc; deserializeJson(doc, line.c_str() + 6);
    Serial.print(".");                          // 这里仅示意非数据行 | just marking non-data lines here
  }
  Serial.println("\n--------------------------------");

  Serial.printf("[Meta] HTTP=%d error=%d(%s) done=%d\n",
                resp.statusCode(), resp.lastError(),
                aikitErrorToString(resp.lastError()),
                resp.doneSentinelSeen() ? 1 : 0);

  resp.close();   // 释放底层连接（自然读尽时也会自动调用，此处演示显式关闭） | release the underlying connection (auto-invoked when fully drained; explicit close shown here)
}

void loop() {}
