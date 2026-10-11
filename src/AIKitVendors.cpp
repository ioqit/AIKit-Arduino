/*
 * AIKitVendors.cpp - 内置厂商预设表实现
 *
 * 端点信息核对自各厂商官方文档（2026-10）：
 *   DeepSeek    https://api-docs.deepseek.com/
 *   Moonshot     https://platform.moonshot.cn/docs/api/chat （国际站 api.moonshot.ai）
 *   百炼/Qwen   https://help.aliyun.com/zh/model-studio/developer-reference/compatibility-of-openai-with-dashscope
 *   智谱         https://open.bigmodel.cn / https://docs.bigmodel.cn （国际站 api.z.ai）
 *   SiliconFlow  https://docs.siliconflow.cn/cn/api-reference/chat-completions/chat-completions
 *   MiniMax      https://platform.minimaxi.com/docs （国内 api.minimax.cn，国际 api.minimaxi.com）
 *   OpenAI       https://platform.openai.com/docs/api-reference/chat
 *   ---- v1.2.0 新增 ----
 *   Groq         https://console.groq.com/docs/openai （OpenAI 兼容：/openai/v1）
 *   OpenRouter   https://openrouter.ai/docs/quickstart （OpenAI 兼容：/api/v1）
 *   Mistral      https://docs.mistral.ai/ （OpenAI 兼容：/v1）
 *   llama.cpp    https://github.com/ggml-org/llama.cpp （llama-server，默认 :8080）
 *   Ollama       https://github.com/ollama/ollama （OpenAI 兼容层，默认 :11434）
 *   vLLM         https://docs.vllm.ai/ （OpenAI 兼容服务器，默认 :8000）
 *   LM Studio    https://lmstudio.ai/ （本地服务器，默认 :1234）
 *   ModelScope   https://modelscope.cn/ （API-Inference，/v1/chat/completions）
 *
 * AIKit-Arduino (https://github.com/ioqit/AIKit-Arduino)
 */
#include "AIKitVendors.h"

// 说明：不使用 PROGMEM 存放该结构体数组——ESP8266 上 PROGMEM 数据须经 pgm_read*
// 访问，直接解引用会崩溃；而 ESP8266 核心会把字符串字面量自动放入 flash，
// 因此这里仅结构体本身（约 600 字节）驻留 RAM，换取两个平台的直接安全访问。
static const AIKitVendorInfo kAIKitVendors[AIKIT_VENDOR_COUNT] = {
    // vendorName             host                                     port  path                                     defaultModel                     https  noAuth
    { "DeepSeek",             "api.deepseek.com",                      443,  "/chat/completions",                      "deepseek-flash",                true,  false },
    { "Moonshot AI (CN)",     "api.moonshot.cn",                       443,  "/v1/chat/completions",                   "kimi-k3",                       true,  false },
    { "Moonshot AI (Global)", "api.moonshot.ai",                       443,  "/v1/chat/completions",                   "kimi-k3",                       true,  false },
    { "Alibaba Qwen (CN)",    "dashscope.aliyuncs.com",                443,  "/compatible-mode/v1/chat/completions",   "qwen-plus",                     true,  false },
    { "Alibaba Qwen (Global)","dashscope-intl.aliyuncs.com",           443,  "/compatible-mode/v1/chat/completions",   "qwen-plus",                     true,  false },
    { "Zhipu GLM (CN)",       "open.bigmodel.cn",                      443,  "/api/paas/v4/chat/completions",          "glm-4.7-flash",                 true,  false },
    { "Z.ai GLM (Global)",    "api.z.ai",                              443,  "/api/paas/v4/chat/completions",          "glm-4.7-flash",                 true,  false },
    { "SiliconFlow",          "api.siliconflow.cn",                    443,  "/v1/chat/completions",                   "deepseek-ai/DeepSeek-V4-Flash", true,  false },
    { "MiniMax (CN)",         "api.minimax.cn",                        443,  "/v1/text/chatcompletion_v2",             "MiniMax-M2.7",                  true,  false },
    { "MiniMax (Global)",     "api.minimaxi.com",                      443,  "/v1/text/chatcompletion_v2",             "MiniMax-M2.7",                  true,  false },
    { "OpenAI",               "api.openai.com",                        443,  "/v1/chat/completions",                   "gpt-5-mini",                    true,  false },
    // ---- v1.2.0 新增 ----
    // Groq：LPU 超低延迟推理；llama-3.3-70b-versatile 为通用主力，
    // 亦可选 openai/gpt-oss-120b 等（setModel 更换）
    { "Groq",                 "api.groq.com",                          443,  "/openai/v1/chat/completions",            "llama-3.3-70b-versatile",       true,  false },
    // OpenRouter：数百模型聚合网关；"openrouter/auto" 自动路由，
    // 计费模型须在网页后台预充值
    { "OpenRouter",           "openrouter.ai",                         443,  "/api/v1/chat/completions",               "openrouter/auto",               true,  false },
    // Mistral：OpenAI 兼容 /v1；mistral-small-latest 为当前主力小模型
    { "Mistral AI",           "api.mistral.ai",                        443,  "/v1/chat/completions",                   "mistral-small-latest",          true,  false },
    // llama.cpp：llama-server 启动后监听 8080；model 字段多数版本被忽略，
    // 但建议 setModel() 与 --model 别名一致
    { "llama.cpp (local)",    "localhost",                             8080, "/v1/chat/completions",                   "local-model",                   false, true },
    // Ollama：OpenAI 兼容层必须 setModel() 为已 pull 的模型名（如 "qwen3:4b"）
    { "Ollama (local)",       "localhost",                           11434,  "/v1/chat/completions",                   "qwen3:4b",                      false, true },
    // vLLM：须 setModel() 为 --served-model-name 指定的模型名
    { "vLLM (local)",         "localhost",                            8000,  "/v1/chat/completions",                   "served-model",                  false, true },
    // LM Studio：须 setModel() 为已加载模型标识（如 "qwen2.5-7b-instruct"）
    { "LM Studio (local)",    "localhost",                            1234,  "/v1/chat/completions",                   "local-model",                   false, true },
    // ModelScope：魔搭社区 API-Inference；模型名带 "组织/模型" 前缀
    { "ModelScope",           "api-inference.modelscope.cn",           443,  "/v1/chat/completions",                   "Qwen/Qwen3-8B",                 true,  false },
};

const AIKitVendorInfo* aikitVendorInfo(uint8_t id) {
    if (id >= AIKIT_VENDOR_COUNT) return nullptr;
    return &kAIKitVendors[id];
}

const char* aikitVendorName(uint8_t id) {
    const AIKitVendorInfo* v = aikitVendorInfo(id);
    return v ? v->vendorName : "Unknown";
}
