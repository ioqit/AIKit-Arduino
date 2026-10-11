# 02 - 厂商预设总表

**[English](../en/02_vendors.md)** | 简体中文

以下端点信息均依据**各厂商官方文档**核实整理（核对时间 2026-10）。厂商迭代频繁，如与你所见文档不一致，请以官方文档为准——本库端点与模型均可用 `setModel()` / `beginCustom()` 自行覆盖。

## Chat Completions（19 家预设）

| 预设枚举 | 厂商 | Base URL | 路径 | 默认模型 | 鉴权 |
| --- | --- | --- | --- | --- | --- |
| `AIKIT_DEEPSEEK` | DeepSeek 深度求索 | `https://api.deepseek.com` | `/chat/completions` | `deepseek-flash` | Bearer |
| `AIKIT_MOONSHOT` | Moonshot AI（Kimi） | `https://api.moonshot.cn` | `/v1/chat/completions` | `kimi-k3` | Bearer |
| `AIKIT_MOONSHOT_INTL` | Moonshot（国际站） | `https://api.moonshot.ai` | `/v1/chat/completions` | `kimi-k3` | Bearer |
| `AIKIT_QWEN_DASHSCOPE` | 阿里云百炼（Qwen） | `https://dashscope.aliyuncs.com/compatible-mode` | `/v1/chat/completions` | `qwen-plus` | Bearer |
| `AIKIT_QWEN_DASHSCOPE_INTL` | 百炼（国际站） | `https://dashscope-intl.aliyuncs.com/compatible-mode` | `/v1/chat/completions` | `qwen-plus` | Bearer |
| `AIKIT_ZHIPU` | 智谱开放平台（GLM） | `https://open.bigmodel.cn/api` | `/paas/v4/chat/completions` | `glm-4.7-flash` | Bearer |
| `AIKIT_ZHIPU_ZAI` | Z.ai（智谱国际站） | `https://api.z.ai/api` | `/paas/v4/chat/completions` | `glm-4.7-flash` | Bearer |
| `AIKIT_SILICONFLOW` | 硅基流动 SiliconFlow | `https://api.siliconflow.cn` | `/v1/chat/completions` | `deepseek-ai/DeepSeek-V4-Flash` | Bearer |
| `AIKIT_MINIMAX` | MiniMax（国内） | `https://api.minimax.cn` | `/v1/text/chatcompletion_v2` | `MiniMax-M2.7` | Bearer |
| `AIKIT_MINIMAX_INTL` | MiniMax（国际） | `https://api.minimaxi.com` | `/v1/text/chatcompletion_v2` | `MiniMax-M2.7` | Bearer |
| `AIKIT_OPENAI` | OpenAI | `https://api.openai.com` | `/v1/chat/completions` | `gpt-5-mini` | Bearer |
| `AIKIT_GROQ` | Groq | `https://api.groq.com` | `/openai/v1/chat/completions` | `llama-3.3-70b-versatile` | Bearer |
| `AIKIT_OPENROUTER` | OpenRouter | `https://openrouter.ai` | `/api/v1/chat/completions` | `openrouter/auto` | Bearer |
| `AIKIT_MISTRAL` | Mistral AI | `https://api.mistral.ai` | `/v1/chat/completions` | `mistral-small-latest` | Bearer |
| `AIKIT_LLAMACPP` | llama.cpp（本地） | `http://localhost:8080` | `/v1/chat/completions` | `local-model` | 无 |
| `AIKIT_OLLAMA` | Ollama（本地） | `http://localhost:11434` | `/v1/chat/completions` | `qwen3:4b` | 无 |
| `AIKIT_VLLM` | vLLM（本地） | `http://localhost:8000` | `/v1/chat/completions` | `served-model` | 无 |
| `AIKIT_LMSTUDIO` | LM Studio（本地） | `http://localhost:1234` | `/v1/chat/completions` | `local-model` | 无 |
| `AIKIT_MODELSCOPE` | 魔搭 ModelScope | `https://api-inference.modelscope.cn` | `/v1/chat/completions` | `Qwen/Qwen3-8B` | Bearer |

云端预设均为 HTTPS(443) + `Authorization: Bearer <key>`，SSE 流式格式彼此兼容（`data: {json}` 行 + `data: [DONE]` 哨兵）。

## 各厂商要点（核对自官方文档）

**DeepSeek**（[官方文档](https://api-docs.deepseek.com/)）
- Base URL **不带 `/v1`**（官方推荐写法；旧式 `/v1` 前缀亦兼容）。
- 模型：`deepseek-flash`（推荐默认；旧名 `deepseek-v4-flash` 等仍被接受）、`deepseek-v4-pro`。

**Moonshot AI / Kimi**（[官方文档](https://platform.moonshot.cn/docs/api/chat)）
- 国内 `api.moonshot.cn`，国际 `api.moonshot.ai`；两站 API Key 不互通。
- ⚠️ `max_tokens` 已标记**弃用**，请用 `setMaxCompletionTokens()`。

**阿里云百炼 / Qwen**（[官方文档](https://help.aliyun.com/zh/model-studio/developer-reference/compatibility-of-openai-with-dashscope)）
- OpenAI 兼容模式完整前缀 `/compatible-mode/v1`，勿遗漏。
- 模型：`qwen-plus`（均衡默认）、`qwen-max`、`qwen-turbo`、`qwen-long`、`qwen3.8-max` 等。
- 国内外 Key 不互通。

**智谱 GLM**（[官方文档](https://docs.bigmodel.cn/)）
- 路径前缀特殊：`/api/paas/v4/chat/completions`；**`*-flash` 系列免费**（默认值即 `glm-4.7-flash`）。
- 思考模式：`addJsonField("thinking", "{\"type\":\"enabled\"}")`。

**SiliconFlow 硅基流动**（[官方文档](https://docs.siliconflow.cn/cn/api-reference/chat-completions/chat-completions)）
- 聚合平台，200+ 模型；**模型名必须带 `厂商/` 前缀**（如 `Qwen/Qwen3-8B`）。

**MiniMax**（[官方文档](https://platform.minimaxi.com/docs)）
- ⚠️ 路径为 `/v1/text/chatcompletion_v2`，**不是** `/v1/chat/completions`（预设已处理）。
- ⚠️ `max_tokens` 已标记弃用，请用 `setMaxCompletionTokens()`。

**OpenAI**（[官方文档](https://platform.openai.com/docs/api-reference/chat)）
- o 系列推理模型**只接受** `max_completion_tokens`，且 `temperature` 固定为 1——用自定义 JSON 字段覆盖即可。

**Groq**（[官方文档](https://console.groq.com/docs/openai)）*v1.2.0 新增*
- LPU 硬件推理，延迟极低；免费额度按分钟计。
- 模型：`llama-3.3-70b-versatile`（默认）、`openai/gpt-oss-120b`、`qwen/qwen3-32b` 等（注意部分带 `厂商/` 前缀）。

**OpenRouter**（[官方文档](https://openrouter.ai/docs/quickstart)）*v1.2.0 新增*
- 数百模型聚合网关；`openrouter/auto` 自动路由到可用且高性价比的模型。
- 需在网页后台预充值；也可 `setModel("anthropic/claude-...")` 等指定具体模型。

**Mistral**（[官方文档](https://docs.mistral.ai/)）*v1.2.0 新增*
- 标准_OpenAI 兼容 `/v1`；`mistral-small-latest` 为当前主力小模型，另有 `magistral` 系推理模型。

**ModelScope 魔搭**（[官方文档](https://modelscope.cn/)）*v1.2.0 新增*
- 阿里系模型社区 API-Inference，国内直连；模型名带 `组织/模型` 前缀（如 `Qwen/Qwen3-8B`、`deepseek-ai/DeepSeek-V4-Flash`）。
- 在模型页点「API 推理」获取访问令牌（`ms-` 前缀）；仅部分模型提供推理端点。

## 本地部署专题（无鉴权）

`AIKIT_LLAMACPP` / `AIKIT_OLLAMA` / `AIKIT_VLLM` / `AIKIT_LMSTUDIO` 四家预设对应**自己电脑上跑的推理服务**：

1. **无鉴权**：`begin()` 可传空 Key；请求不带 Authorization 头。
2. **localhost 的含义**：开发板上「localhost」是板子自己——预设默认值适用于 USB 网络共享等特殊场景；**局域网部署请改用 `beginCustom()`**：
   ```cpp
   ai.beginCustom("192.168.1.20", 11434, "/v1/chat/completions",
                  /*useHttps=*/false, /*apiKey=*/"", "qwen3:4b");
   ```
3. **模型名必须对**：
   - Ollama：`ollama pull qwen3:4b` 后 `setModel("qwen3:4b")`；
   - vLLM：`setModel()` 填启动参数 `--served-model-name` 的值；
   - llama.cpp / LM Studio：多数版本忽略或宽松校验，建议与实际加载模型一致。

启动参考：

```bash
# Ollama
ollama serve                 # 默认 11434
# vLLM
vllm serve Qwen/Qwen3-8B --served-model-name my-model   # 默认 8000
# llama.cpp
llama-server -m model.gguf --host 0.0.0.0 --port 8080
# LM Studio：Developer 标签页启动本地服务器（默认 1234）
```

> 未列出的厂商只要提供 OpenAI 兼容端点（Together / Fireworks / OneAPI / 中转站…），都可以用 `beginCustom()` 一行接入。
