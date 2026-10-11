# 02 - Vendor Preset Reference

English | **[简体中文](../02_vendors.md)**

All endpoint information below was compiled and verified against **each vendor's official documentation** (checked 2026-10). Vendors iterate quickly — if anything here differs from the docs you see, trust the official docs. Both endpoints and models can be overridden in this library via `setModel()` / `beginCustom()`.

## Chat Completions (19 presets)

| Preset enum | Vendor | Base URL | Path | Default model | Auth |
| --- | --- | --- | --- | --- | --- |
| `AIKIT_DEEPSEEK` | DeepSeek | `https://api.deepseek.com` | `/chat/completions` | `deepseek-flash` | Bearer |
| `AIKIT_MOONSHOT` | Moonshot AI (Kimi) | `https://api.moonshot.cn` | `/v1/chat/completions` | `kimi-k3` | Bearer |
| `AIKIT_MOONSHOT_INTL` | Moonshot (international site) | `https://api.moonshot.ai` | `/v1/chat/completions` | `kimi-k3` | Bearer |
| `AIKIT_QWEN_DASHSCOPE` | Alibaba Cloud Bailian / DashScope (Qwen) | `https://dashscope.aliyuncs.com/compatible-mode` | `/v1/chat/completions` | `qwen-plus` | Bearer |
| `AIKIT_QWEN_DASHSCOPE_INTL` | Bailian (international site) | `https://dashscope-intl.aliyuncs.com/compatible-mode` | `/v1/chat/completions` | `qwen-plus` | Bearer |
| `AIKIT_ZHIPU` | Zhipu BigModel platform (GLM) | `https://open.bigmodel.cn/api` | `/paas/v4/chat/completions` | `glm-4.7-flash` | Bearer |
| `AIKIT_ZHIPU_ZAI` | Z.ai (Zhipu international site) | `https://api.z.ai/api` | `/paas/v4/chat/completions` | `glm-4.7-flash` | Bearer |
| `AIKIT_SILICONFLOW` | SiliconFlow | `https://api.siliconflow.cn` | `/v1/chat/completions` | `deepseek-ai/DeepSeek-V4-Flash` | Bearer |
| `AIKIT_MINIMAX` | MiniMax (China) | `https://api.minimax.cn` | `/v1/text/chatcompletion_v2` | `MiniMax-M2.7` | Bearer |
| `AIKIT_MINIMAX_INTL` | MiniMax (international) | `https://api.minimaxi.com` | `/v1/text/chatcompletion_v2` | `MiniMax-M2.7` | Bearer |
| `AIKIT_OPENAI` | OpenAI | `https://api.openai.com` | `/v1/chat/completions` | `gpt-5-mini` | Bearer |
| `AIKIT_GROQ` | Groq | `https://api.groq.com` | `/openai/v1/chat/completions` | `llama-3.3-70b-versatile` | Bearer |
| `AIKIT_OPENROUTER` | OpenRouter | `https://openrouter.ai` | `/api/v1/chat/completions` | `openrouter/auto` | Bearer |
| `AIKIT_MISTRAL` | Mistral AI | `https://api.mistral.ai` | `/v1/chat/completions` | `mistral-small-latest` | Bearer |
| `AIKIT_LLAMACPP` | llama.cpp (local) | `http://localhost:8080` | `/v1/chat/completions` | `local-model` | none |
| `AIKIT_OLLAMA` | Ollama (local) | `http://localhost:11434` | `/v1/chat/completions` | `qwen3:4b` | none |
| `AIKIT_VLLM` | vLLM (local) | `http://localhost:8000` | `/v1/chat/completions` | `served-model` | none |
| `AIKIT_LMSTUDIO` | LM Studio (local) | `http://localhost:1234` | `/v1/chat/completions` | `local-model` | none |
| `AIKIT_MODELSCOPE` | ModelScope | `https://api-inference.modelscope.cn` | `/v1/chat/completions` | `Qwen/Qwen3-8B` | Bearer |

All cloud presets use HTTPS (443) + `Authorization: Bearer <key>`, and their SSE streaming formats are mutually compatible (`data: {json}` lines + the `data: [DONE]` sentinel).

## Per-vendor notes (verified against official docs)

**DeepSeek** ([official docs](https://api-docs.deepseek.com/))
- The Base URL carries **no `/v1`** (the officially recommended form; the legacy `/v1` prefix also works).
- Models: `deepseek-flash` (recommended default; legacy names such as `deepseek-v4-flash` are still accepted), `deepseek-v4-pro`.

**Moonshot AI / Kimi** ([official docs](https://platform.moonshot.cn/docs/api/chat))
- China: `api.moonshot.cn`, international: `api.moonshot.ai`; API keys are not interchangeable between the two sites.
- ⚠️ `max_tokens` is marked **deprecated** — use `setMaxCompletionTokens()`.

**Alibaba Cloud Bailian / Qwen** ([official docs](https://help.aliyun.com/zh/model-studio/developer-reference/compatibility-of-openai-with-dashscope))
- The OpenAI-compatible mode requires the full prefix `/compatible-mode/v1` — don't miss it.
- Models: `qwen-plus` (balanced default), `qwen-max`, `qwen-turbo`, `qwen-long`, `qwen3.8-max`, etc.
- Keys for the China and international sites are not interchangeable.

**Zhipu GLM** ([official docs](https://docs.bigmodel.cn/))
- Unusual path prefix: `/api/paas/v4/chat/completions`; the **`*-flash` series is free** (the default is `glm-4.7-flash`).
- Thinking mode: `addJsonField("thinking", "{\"type\":\"enabled\"}")`.

**SiliconFlow** ([official docs](https://docs.siliconflow.cn/cn/api-reference/chat-completions/chat-completions))
- An aggregation platform with 200+ models; **model names must carry the `vendor/` prefix** (e.g. `Qwen/Qwen3-8B`).

**MiniMax** ([official docs](https://platform.minimaxi.com/docs))
- ⚠️ The path is `/v1/text/chatcompletion_v2`, **not** `/v1/chat/completions` (the preset already handles this).
- ⚠️ `max_tokens` is marked deprecated — use `setMaxCompletionTokens()`.

**OpenAI** ([official docs](https://platform.openai.com/docs/api-reference/chat))
- The o-series reasoning models **only accept** `max_completion_tokens`, and `temperature` is fixed at 1 — override it via custom JSON fields.

**Groq** ([official docs](https://console.groq.com/docs/openai)) *added in v1.2.0*
- LPU hardware inference with extremely low latency; the free tier is metered per minute.
- Models: `llama-3.3-70b-versatile` (default), `openai/gpt-oss-120b`, `qwen/qwen3-32b`, etc. (note that some carry a `vendor/` prefix).

**OpenRouter** ([official docs](https://openrouter.ai/docs/quickstart)) *added in v1.2.0*
- An aggregation gateway to hundreds of models; `openrouter/auto` automatically routes to an available, cost-effective model.
- Requires pre-paid credit from the web console; you can also pin a specific model, e.g. `setModel("anthropic/claude-...")`.

**Mistral** ([official docs](https://docs.mistral.ai/)) *added in v1.2.0*
- Standard OpenAI-compatible `/v1`; `mistral-small-latest` is the current workhorse small model, plus the `magistral` reasoning series.

**ModelScope** ([official docs](https://modelscope.cn/)) *added in v1.2.0*
- API-Inference of Alibaba's model community, directly reachable from mainland China; model names carry an `org/model` prefix (e.g. `Qwen/Qwen3-8B`, `deepseek-ai/DeepSeek-V4-Flash`).
- Get the access token (`ms-` prefix) via "API Inference" on a model page; only some models expose an inference endpoint.

## Local deployment topic (no authentication)

The four presets `AIKIT_LLAMACPP` / `AIKIT_OLLAMA` / `AIKIT_VLLM` / `AIKIT_LMSTUDIO` point at **an inference service running on your own computer**:

1. **No authentication**: `begin()` accepts an empty key; requests carry no Authorization header.
2. **What "localhost" means**: on the board, "localhost" is the board itself — the preset defaults only fit special setups such as USB network tethering; **for LAN deployment use `beginCustom()` instead**:
   ```cpp
   ai.beginCustom("192.168.1.20", 11434, "/v1/chat/completions",
                  /*useHttps=*/false, /*apiKey=*/"", "qwen3:4b");
   ```
3. **Model names must match**:
   - Ollama: after `ollama pull qwen3:4b`, call `setModel("qwen3:4b")`;
   - vLLM: pass the value of the `--served-model-name` startup flag to `setModel()`;
   - llama.cpp / LM Studio: most versions ignore it or validate loosely — matching the actually loaded model is still recommended.

Startup reference:

```bash
# Ollama
ollama serve                 # default port 11434
# vLLM
vllm serve Qwen/Qwen3-8B --served-model-name my-model   # default port 8000
# llama.cpp
llama-server -m model.gguf --host 0.0.0.0 --port 8080
# LM Studio: start the local server from the Developer tab (default port 1234)
```

> Any vendor not listed above works as well, as long as it exposes an OpenAI-compatible endpoint (Together / Fireworks / OneAPI / relay gateways…) — one `beginCustom()` call and you are in.
