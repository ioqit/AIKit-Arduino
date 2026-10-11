/*
  18_ASR_Transcribe - HTTP 一句话转写（云端 ASR）| One-shot HTTP transcription

  演示内容 | What this demonstrates:
    - AIKitASRClient：8 家厂商预设（OpenAI / Groq / Mistral / 智谱 /
      百炼 fun-asr-flash / 阶跃 / 小米 MiMo / FunASR 自部署）
    - transcribePCM()：裸 PCM16 自动包 WAV 头上传，返回识别文本
    - transcribe()：上传现成的 wav / mp3 文件字节

  音频来源：本示例合成 2 秒 440 Hz 正弦波做演示（云端会返回
  空文本或噪声）。实际使用时替换为 I2S / ADC 采集或文件数据。
  | The demo uploads a synthesized 2 s sine wave; replace it with
    real microphone or file data in your project.

  ⚠ ESP8266 堆小：音频请控制在 ~12 KB 内（16 kHz PCM16 约 6 秒），
    或调小 AIKIT_ASR_MAX_UPLOAD_BYTES。
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
const char* API_KEY   = "sk-your-dashscope-api-key";

AIKitASRClient asr(&Serial);

// ---- 合成演示音频：2 秒 440 Hz 正弦波（16 kHz / 16 bit / 单声道）----
// 32000 字节放不进 ESP8266 的 .data 段，放堆上生成 | build it on heap
uint8_t* makeDemoPcm(size_t& outLen) {
  const uint32_t kSamples = 16000 * 2;          // 2 秒
  const float kTwoPi = 6.2831853f;
  uint8_t* buf = (uint8_t*)malloc(kSamples * 2);
  if (buf == nullptr) return nullptr;
  int16_t* pcm = (int16_t*)buf;
  for (uint32_t i = 0; i < kSamples; i++) {
    // 定点正弦（避开 ESP8266 的软件浮点慢路径）
    pcm[i] = (int16_t)(12000.0f * sinf(kTwoPi * 440.0f * i / 16000.0f));
  }
  outLen = kSamples * 2;
  return buf;
}

void setup() {
  Serial.begin(115200);
  delay(200);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) { delay(300); Serial.print("."); }
  Serial.println("\n[WiFi] connected");

  // ---- 厂商预设（换一家只改这一行 + key）----
  // AIKIT_ASR_OPENAI     gpt-transcribe（multipart）
  // AIKIT_ASR_GROQ       whisper-large-v3-turbo（multipart，免费额度大）
  // AIKIT_ASR_MISTRAL    voxtral-mini-latest（x-api-key）
  // AIKIT_ASR_ZHIPU      glm-asr-2512（wav/mp3，单条 ≤30 秒）
  // AIKIT_ASR_DASHSCOPE  fun-asr-flash（SenseVoice 系；qwen3-asr-flash
  //                      可用 setModel("qwen3-asr-flash") 切换）
  // AIKIT_ASR_STEPFUN    stepaudio-3-asr-max（JSON+SSE）
  // AIKIT_ASR_MIMO       mimo-v2.5-asr（小米）
  // AIKIT_ASR_FUNASR     自部署：asr.beginFunASR("192.168.1.10")
  if (!asr.begin(AIKIT_ASR_DASHSCOPE, API_KEY)) {
    Serial.println("[ASR] begin() failed");
    return;
  }
  asr.setLanguage("zh");        // 可选 | optional hint
  asr.setInsecure(true);        // 生产环境请配置根证书并关掉
  // asr.setCACert(ISRG_ROOT_X1_PEM); asr.setInsecure(false);

  // ---- 转写 | transcribe ----
  size_t pcmLen = 0;
  uint8_t* pcm = makeDemoPcm(pcmLen);
  if (pcm == nullptr) { Serial.println("[Demo] OOM"); return; }

  String text;
  int err = asr.transcribePCM(pcm, pcmLen, /*sampleRate=*/16000,
                              /*channels=*/1, text);
  free(pcm);

  if (err == AIKIT_OK) {
    Serial.println("[ASR text] " + text);
  } else {
    Serial.printf("[ASR] error %d (%s) | HTTP %d\n", err,
                  aikitErrorToString(err), asr.lastStatusCode());
    Serial.println("       body: " + asr.lastErrorBody());
  }

  // ---- 上传现成音频文件字节 | upload ready-made audio bytes ----
  // uint8_t wav[] = { 0x52, 0x49, 0x46, 0x46, ... };   // 或从 LittleFS 读
  // asr.transcribe(wav, sizeof(wav), "clip.wav", "audio/wav", text);
}

void loop() {}
