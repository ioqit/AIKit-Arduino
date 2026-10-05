/*
 * AIKit.h - AIKit-Arduino 统一入口
 *
 * 一站式 Arduino LLM 客户端：一个 begin() 接入主流 OpenAI 兼容 API。
 *
 *   AIKitClient ai(&Serial);
 *   ai.begin(AIKIT_DEEPSEEK, "sk-...");
 *   String reply;
 *   ai.chat("你好", reply);
 *
 * 详见 README.md（中文）/ README_EN.md（English）与 examples/ 目录。
 *
 * AIKit-Arduino (https://github.com/ioqite/AIKit-Arduino)
 */
#pragma once

#include "AIKitConfig.h"
#include "AIKitTypes.h"
#include "AIKitVendors.h"
#include "AIKitStream.h"
#include "AIKitClient.h"
