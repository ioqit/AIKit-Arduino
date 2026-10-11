/*
  21_NewVendors - v1.2.0 新厂商预设演示 | New vendor presets demo

  一行 begin() 切换 8 家新厂商（详见 docs/02_vendors.md）：
    云端 | Cloud:
      AIKIT_GROQ        Groq LPU（llama-3.3-70b-versatile，超低延迟）
      AIKIT_OPENROUTER  聚合网关（"openrouter/auto" 自动路由数百模型）
      AIKIT_MISTRAL     Mistral AI（mistral-small-latest）
      AIKIT_MODELSCOPE  魔搭社区 API-Inference（Qwen/Qwen3-8B，免费额度）
    本地部署（无鉴权，localhost）| local, no API key:
      AIKIT_LLAMACPP    llama.cpp llama-server（默认 :8080）
      AIKIT_OLLAMA      Ollama（默认 :11434，须 setModel 已 pull 的模型）
      AIKIT_VLLM        vLLM（默认 :8000，须 setModel 为 served-model-name）
      AIKIT_LMSTUDIO    LM Studio（默认 :1234）

  本地服务须在本机网络内可达：把 localhost 换成你的内网 IP 需改用
  beginCustom()（预设里的 localhost 供同机 / USB 网络共享场景）。
  | For LAN servers, use beginCustom("192.168.x.x", ...) instead.

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
const char* API_KEY   = "sk-your-groq-api-key";        // gsk_...

AIKitClient ai(&Serial);

void setup() {
  Serial.begin(115200);
  delay(200);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) { delay(300); Serial.print("."); }
  Serial.println("\n[WiFi] connected");

  // ---- 云端厂商：与旧厂商用法完全一致 ----
  if (!ai.begin(AIKIT_GROQ, API_KEY)) {
    Serial.println("[AIKit] begin() failed");
    return;
  }
  ai.setModel("llama-3.3-70b-versatile");   // 可省略（即默认值）

  String reply;
  int err = ai.chat("In one short sentence, why is Groq fast?", reply);
  if (err == AIKIT_OK) {
    Serial.println("[GroQ Reply] " + reply);
  } else {
    Serial.printf("[Groq] error %d: %s\n", err, aikitErrorToString(err));
  }

  // ---- 切换到 OpenRouter（聚合网关）----
  ai.begin(AIKIT_OPENROUTER, "sk-or-your-openrouter-key");
  ai.setModel("openrouter/auto");           // 自动路由到高性价比模型
  err = ai.chat("用一个词解释什么是模型聚合", reply);
  if (err == AIKIT_OK) Serial.println("[OpenRouter] " + reply);

  // ---- 切换到 ModelScope（魔搭，国内免梯子）----
  ai.begin(AIKIT_MODELSCOPE, "ms-your-modelscope-token");
  ai.setModel("Qwen/Qwen3-8B");             // 模型名带 组织/模型 前缀
  err = ai.chat("你好，介绍一下你自己", reply);
  if (err == AIKIT_OK) Serial.println("[ModelScope] " + reply);

  // ---- 本地 Ollama（同一台机器 / USB 网络共享时 localhost 可达）----
  // 无需 API Key（传空串或 nullptr 均可）
  if (ai.begin(AIKIT_OLLAMA, "")) {
    ai.setModel("qwen3:4b");                // 换成你已 pull 的模型
    err = ai.chat("你好", reply);
    if (err == AIKIT_OK) Serial.println("[Ollama] " + reply);
    else Serial.printf("[Ollama] error %d（检查服务是否启动 / 模型名）\n", err);
  }

  // ---- 局域网内的 Ollama：改用 beginCustom 指向真实 IP ----
  // ai.beginCustom("192.168.1.20", 11434, "/v1/chat/completions",
  //                /*useHttps=*/false, "", "qwen3:4b");
}

void loop() {}
