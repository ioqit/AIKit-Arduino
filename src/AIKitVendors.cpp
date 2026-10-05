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
 *
 * AIKit-Arduino (https://github.com/ioqite/AIKit-Arduino)
 */
#include "AIKitVendors.h"

// 说明：不使用 PROGMEM 存放该结构体数组——ESP8266 上 PROGMEM 数据须经 pgm_read*
// 访问，直接解引用会崩溃；而 ESP8266 核心会把字符串字面量自动放入 flash，
// 因此这里仅结构体本身（约 300 字节）驻留 RAM，换取两个平台的直接安全访问。
static const AIKitVendorInfo kAIKitVendors[AIKIT_VENDOR_COUNT] = {
    // vendorName            host                                    port  path                              defaultModel                        https
    { "DeepSeek",            "api.deepseek.com",                     443,  "/chat/completions",              "deepseek-flash",                   true },
    { "Moonshot AI (CN)",    "api.moonshot.cn",                      443,  "/v1/chat/completions",           "kimi-k3",                          true },
    { "Moonshot AI (Global)","api.moonshot.ai",                      443,  "/v1/chat/completions",           "kimi-k3",                          true },
    { "Alibaba Qwen (CN)",   "dashscope.aliyuncs.com",               443,  "/compatible-mode/v1/chat/completions", "qwen-plus",                   true },
    { "Alibaba Qwen (Global)","dashscope-intl.aliyuncs.com",         443,  "/compatible-mode/v1/chat/completions", "qwen-plus",                   true },
    { "Zhipu GLM (CN)",      "open.bigmodel.cn",                     443,  "/api/paas/v4/chat/completions",  "glm-4.7-flash",                    true },
    { "Z.ai GLM (Global)",   "api.z.ai",                             443,  "/api/paas/v4/chat/completions",  "glm-4.7-flash",                    true },
    { "SiliconFlow",         "api.siliconflow.cn",                   443,  "/v1/chat/completions",           "deepseek-ai/DeepSeek-V4-Flash",    true },
    { "MiniMax (CN)",        "api.minimax.cn",                       443,  "/v1/text/chatcompletion_v2",     "MiniMax-M2.7",                     true },
    { "MiniMax (Global)",    "api.minimaxi.com",                     443,  "/v1/text/chatcompletion_v2",     "MiniMax-M2.7",                     true },
    { "OpenAI",              "api.openai.com",                       443,  "/v1/chat/completions",           "gpt-5-mini",                       true },
};

const AIKitVendorInfo* aikitVendorInfo(uint8_t id) {
    if (id >= AIKIT_VENDOR_COUNT) return nullptr;
    return &kAIKitVendors[id];
}

const char* aikitVendorName(uint8_t id) {
    const AIKitVendorInfo* v = aikitVendorInfo(id);
    return v ? v->vendorName : "Unknown";
}
