/*
 * AIKitVendors.h - 内置厂商预设表
 *
 * 所有端点均依据各厂商官方文档核实（2026-10），详见 docs/02_vendors.md。
 * chat completions 预设均为 OpenAI 兼容端点；本地部署厂商（llama.cpp /
 * Ollama / vLLM / LM Studio）无需 API Key，且请用 setModel() 指定服务端
 * 实际加载的模型名。
 *
 * AIKit-Arduino (https://github.com/ioqit/AIKit-Arduino)
 */
#pragma once

#include <Arduino.h>

// ---------------------------------------------------------------------------
// 厂商预设编号
// 国际站以 _INTL / 地区后缀区分；任意 OpenAI 兼容端点可用 beginCustom() 接入。
// ---------------------------------------------------------------------------
enum AIKitVendor : uint8_t {
    AIKIT_DEEPSEEK           = 0,   // DeepSeek（深度求索）
    AIKIT_MOONSHOT           = 1,   // Moonshot AI / Kimi（国内）
    AIKIT_MOONSHOT_INTL      = 2,   // Moonshot AI / Kimi（国际）
    AIKIT_QWEN_DASHSCOPE     = 3,   // 阿里云百炼 DashScope（国内，Qwen 系列）
    AIKIT_QWEN_DASHSCOPE_INTL = 4,  // 阿里云百炼 DashScope（国际）
    AIKIT_ZHIPU              = 5,   // 智谱开放平台 bigmodel.cn（国内，GLM 系列）
    AIKIT_ZHIPU_ZAI          = 6,   // Z.ai（智谱国际站）
    AIKIT_SILICONFLOW        = 7,   // 硅基流动 SiliconFlow
    AIKIT_MINIMAX            = 8,   // MiniMax（国内）
    AIKIT_MINIMAX_INTL       = 9,   // MiniMax（国际）
    AIKIT_OPENAI             = 10,  // OpenAI
    // ---- v1.2.0 新增 ----
    AIKIT_GROQ               = 11,  // Groq（超低延迟 LPU 推理）
    AIKIT_OPENROUTER         = 12,  // OpenRouter（聚合网关，数百模型）
    AIKIT_MISTRAL            = 13,  // Mistral AI（国际）
    AIKIT_LLAMACPP           = 14,  // llama.cpp llama-server（本地部署，无鉴权）
    AIKIT_OLLAMA             = 15,  // Ollama（本地部署，无鉴权）
    AIKIT_VLLM               = 16,  // vLLM（本地部署，无鉴权）
    AIKIT_LMSTUDIO           = 17,  // LM Studio（本地部署，无鉴权）
    AIKIT_MODELSCOPE         = 18,  // 魔搭社区 ModelScope API-Inference（国内）
    AIKIT_VENDOR_COUNT       = 19,
    AIKIT_CUSTOM             = 0xFF // beginCustom() 自定义端点
};

// ---------------------------------------------------------------------------
// 厂商预设信息
// ---------------------------------------------------------------------------
struct AIKitVendorInfo {
    const char* vendorName;   // 展示名，如 "DeepSeek"
    const char* host;         // API 主机名
    uint16_t    port;         // 端口（https=443 / http=80 / 本地服务为各自默认端口）
    const char* path;         // chat completions 路径
    const char* defaultModel; // 推荐默认模型（可 setModel() 更换）
    bool        https;        // 是否 HTTPS
    bool        noAuth;       // true：本地部署无鉴权（begin() 允许空 API Key）
};

// 取厂商预设信息；id 越界返回 nullptr（调用方负责判空，不抛异常）
const AIKitVendorInfo* aikitVendorInfo(uint8_t id);

// 厂商编号转展示名（未知返回 "Unknown"）
const char* aikitVendorName(uint8_t id);
