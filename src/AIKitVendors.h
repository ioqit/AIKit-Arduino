/*
 * AIKitVendors.h - 内置厂商预设表
 *
 * 所有端点均依据各厂商官方文档核实（2026-10），详见 README《厂商 API 规格表》。
 * 所有预设均为 HTTPS + Bearer 鉴权的 OpenAI 兼容 chat completions 端点。
 *
 * AIKit-Arduino (https://github.com/ioqite/AIKit-Arduino)
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
    AIKIT_VENDOR_COUNT       = 11,
    AIKIT_CUSTOM             = 0xFF // beginCustom() 自定义端点
};

// ---------------------------------------------------------------------------
// 厂商预设信息
// ---------------------------------------------------------------------------
struct AIKitVendorInfo {
    const char* vendorName;   // 展示名，如 "DeepSeek"
    const char* host;         // API 主机名
    uint16_t    port;         // 端口（443）
    const char* path;         // chat completions 路径
    const char* defaultModel; // 推荐默认模型（可 setModel() 更换）
    bool        https;        // 是否 HTTPS
};

// 取厂商预设信息；id 越界返回 nullptr（调用方负责判空，不抛异常）
const AIKitVendorInfo* aikitVendorInfo(uint8_t id);

// 厂商编号转展示名（未知返回 "Unknown"）
const char* aikitVendorName(uint8_t id);
