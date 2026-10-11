/*
 * AIKit.h - AIKit-Arduino 统一入口
 *
 * 一站式 Arduino AI 客户端：一个 begin() 接入主流 OpenAI 兼容 API，
 * 附带云端语音转文字（HTTP + WebSocket 实时）与对话历史管理。
 *
 *   AIKitClient ai(&Serial);
 *   ai.begin(AIKIT_DEEPSEEK, "sk-...");
 *   String reply;
 *   ai.chat("你好", reply);
 *
 *   AIKitASRClient asr(&Serial);          // HTTP 一句话转写
 *   asr.begin(AIKIT_ASR_DASHSCOPE, "sk-...");
 *   asr.transcribePCM(pcm, len, 16000, 1, text);
 *
 *   AIKitRealtimeASR rt(&Serial);         // WebSocket 实时识别（可选依赖
 *   rt.begin(AIKIT_RT_DASHSCOPE, "sk-...");//  ArduinoWebsockets）
 *
 * 详见 README.md（中文）/ README_EN.md（English）、docs/ 与 examples/。
 *
 * AIKit-Arduino (https://github.com/ioqit/AIKit-Arduino)
 */
#pragma once

#include "AIKitConfig.h"
#include "AIKitTypes.h"
#include "AIKitVendors.h"
#include "AIKitStream.h"
#include "AIKitHistory.h"
#include "AIKitASR.h"
#include "AIKitRealtimeASR.h"
#include "AIKitClient.h"
