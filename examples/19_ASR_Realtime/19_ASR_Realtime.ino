/*
  19_ASR_Realtime - 实时语音识别（WebSocket 流式 ASR）
                | Real-time streaming speech recognition

  ⚠ 依赖 ArduinoWebsockets（可选依赖，安装后自动启用）：
       arduino-cli lib install "ArduinoWebsockets"

  演示内容 | What this demonstrates:
    - AIKitRealtimeASR：7 家厂商的实时识别（字节火山 Seed-ASR /
      阿里百炼 paraformer / 百度 / FunASR 自部署 / OpenAI Realtime /
      阶跃 StepAudio / Mistral 实验性）
    - onPartial（本句迄今文本）/ onFinal（一句话定稿）回调
    - sendAudio()：16 kHz / 16 bit / 单声道 PCM，建议 100~200 ms 一帧
    - loop() 驱动接收与保活；finish() 发结束信号并等最终结果

  音频来源：
    ESP32  —— I2S 数字麦克风（INMP441 / MSM261，约 ¥10），接线见下方
    ESP8266 —— 无 I2S 便捷驱动，本示例发送合成正弦波演示协议流程
  | Audio source: ESP32 reads an INMP441 I2S mic; ESP8266 (no easy
    I2S) sends a synthesized sine wave to demonstrate the protocol.

  接线（INMP441 → ESP32）| wiring:
    VDD→3V3  GND→GND  SCK→GPIO26  WS→GPIO25(字选)  SD→GPIO22  L/R→GND

  适用于 ESP32 / ESP8266。
*/
#if defined(ESP8266)
  #include <ESP8266WiFi.h>
#else
  #include <WiFi.h>
  #include <driver/i2s.h>        // legacy I2S 驱动（3.x 可用，仅弃用警告）
#endif
#include <AIKit.h>

const char* WIFI_SSID = "your-ssid";
const char* WIFI_PASS = "your-password";
const char* API_KEY   = "sk-your-dashscope-api-key";

AIKitRealtimeASR rt(&Serial);

const uint32_t kSampleRate = 16000;
const uint32_t kRunSeconds = 20;              // 采集时长 | capture duration
uint32_t t_start = 0;

#if defined(ESP32)
// ---- INMP441：32 bit 帧取高 16 位 ----
static void micInit() {
  i2s_config_t cfg = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = kSampleRate,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,   // L/R 接地时用 LEFT
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 256,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };
  i2s_pin_config_t pins = {
    .bck_io_num = 26,      // SCK
    .ws_io_num = 25,       // WS
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = 22      // SD
  };
  i2s_driver_install(I2S_NUM_0, &cfg, 0, nullptr);
  i2s_set_pin(I2S_NUM_0, &pins);
  i2s_start(I2S_NUM_0);
}

// 读一帧 PCM16（160 ms ≈ 5120 B；32bit 原始 → 高 16 位）
static size_t micRead(uint8_t* out, size_t outCap) {
  static int32_t raw[512];                       // 512 样本 ≈ 160 ms
  size_t bytes = 0;
  if (i2s_read(I2S_NUM_0, raw, sizeof(raw), &bytes, portMAX_DELAY) != ESP_OK) {
    return 0;
  }
  size_t samples = bytes / 4;
  int16_t* pcm = (int16_t*)out;
  size_t n = (samples * 2 <= outCap) ? samples : (outCap / 2);
  for (size_t i = 0; i < n; i++) pcm[i] = (int16_t)(raw[i] >> 16);
  return n * 2;
}
#else
// ---- ESP8266：合成正弦波模拟（实际项目请自行采集/重采样）----
static size_t micRead(uint8_t* out, size_t outCap) {
  static uint32_t phase = 0;
  const size_t kSamples = outCap / 2;
  int16_t* pcm = (int16_t*)out;
  for (size_t i = 0; i < kSamples; i++) {
    pcm[i] = (int16_t)(9000 * sinf(6.2831853f * 440.0f * phase / kSampleRate));
    phase++;
  }
  delay(160);                                    // 实时速率
  return kSamples * 2;
}
#endif

void setup() {
  Serial.begin(115200);
  delay(200);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) { delay(300); Serial.print("."); }
  Serial.println("\n[WiFi] connected");

#if defined(ESP32)
  micInit();
#endif

  // ---- 厂商选择（换一家只改这里）| pick a vendor here ----
  // 阿里百炼 | DashScope:        rt.begin(AIKIT_RT_DASHSCOPE, API_KEY);
  // 字节火山 | Volcengine:       rt.beginVolc("appid", "access-token");
  // 百度     | Baidu:            rt.beginBaidu("1234567", "app-key");
  // FunASR 自部署 | self-hosted: rt.beginFunASR("192.168.1.10", 10095);
  // OpenAI Realtime:             rt.begin(AIKIT_RT_OPENAI, "sk-...");
  // 阶跃星辰 | StepFun:           rt.begin(AIKIT_RT_STEPFUN, "sk-...");
  if (!rt.begin(AIKIT_RT_DASHSCOPE, API_KEY)) {
    Serial.println("[RTASR] begin() failed");
    return;
  }
  rt.setLanguage("zh");
  rt.setInsecure(true);         // 生产环境请配置根证书并关掉

  // ---- 回调 | callbacks ----
  rt.onPartial([](const char* text) {           // 本句迄今（会变化）
    Serial.printf("[partial] %s\r\n", text);
  });
  rt.onFinal([](const char* text) {             // 一句话定稿
    Serial.printf("[FINAL  ] %s\r\n", text);
  });
  rt.onError([](int err, const char* msg) {
    Serial.printf("[error  ] %d %s | %s\r\n", err,
                  aikitErrorToString(err), msg);
  });

  // ---- 连接（阻塞握手 + 启动指令）----
  Serial.println("[RTASR] connecting...");
  int rc = rt.connect();
  if (rc != AIKIT_OK) {
    Serial.printf("[RTASR] connect failed: %s\r\n", aikitErrorToString(rc));
    return;
  }
  Serial.println("[RTASR] connected, streaming audio...");
  t_start = millis();
}

void loop() {
  if (!rt.connected()) { delay(1000); return; }

  // 采集 160 ms 一帧并发送 | capture & send one 160 ms frame
  static uint8_t frame[5120];
  size_t n = micRead(frame, sizeof(frame));
  if (n > 0) {
    if (rt.sendAudio(frame, n) != AIKIT_OK) {
      Serial.println("[RTASR] sendAudio failed");
    }
  }

  rt.loop();                                    // 驱动接收 / 保活（务必调用）

  // 采集时长到 → 结束识别（发结束信号并等最终结果）
  if (millis() - t_start > kRunSeconds * 1000UL) {
    Serial.println("[RTASR] finishing...");
    int frc = rt.finish(/*waitMs=*/8000);
    Serial.printf("[RTASR] finish: %s\r\n", aikitErrorToString(frc));
    while (true) delay(1000);
  }
}
