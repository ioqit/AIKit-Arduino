# 06 - 对话历史管理（AIKitHistory）

**[English](../en/06_history.md)** | 简体中文

`AIKitHistory`（v1.2.0 新增，[src/AIKitHistory.h](../src/AIKitHistory.h)）解决一个 MCU 上的经典难题：**在有限的 RAM 里安全地维护多轮对话上下文**。消息以深拷贝方式保存在堆上、容量按需倍增，两道安全阀防止耗尽堆，并支持 OpenAI messages 格式的 JSON 导出 / 导入（可持久化到 LittleFS / SD / EEPROM）。

```cpp
AIKitHistory history;
history.appendSystem("你是一个 Arduino 助手。");
history.setWindow(8);                    // 滑动窗口，防上下文无限增长

String reply;
ai.chat(history, "我叫小明", reply);      // user 入史 → 请求 → 回复入史
ai.chat(history, "我叫什么名字？", reply); // 第二轮记得上一轮内容
```

## 完整 API

### 追加消息

字符串会被**深拷贝**，调用后源字符串可立即释放：

| 方法 | 说明 |
| --- | --- |
| `int append(const char* role, const char* content)` | 追加一条消息。`role` 取 OpenAI 兼容值：`"system"` / `"user"` / `"assistant"` / `"tool"` |
| `int append(const char* role, const String& content)` | 同上（`String` 正文重载） |
| `int append(const AIKitMessage& msg)` | 从轻量消息结构追加（`content` 指针内容会被拷贝） |
| `int appendUser(content)` / `appendAssistant(content)` / `appendSystem(content)` | 三种常见角色的便捷方法（各有 `const char*` / `const String&` 重载） |

**返回值**：`AIKIT_OK` / `AIKIT_ERR_NULL_ARG`（空指针或空 role）/ `AIKIT_ERR_OOM`（堆内存不足）/ `AIKIT_ERR_HISTORY_LIMIT`（触及安全阀，**不静默丢弃**）。

### 窗口控制

| 方法 | 说明 |
| --- | --- |
| `void setWindow(size_t maxMessages, bool keepSystem = true)` | 启用滑动窗口（默认关闭，`maxMessages=0`）。启用后每次 `append` 若超限，**从最旧一条开始丢弃**（FIFO 模拟固定上下文窗口）；`keepSystem=true` 时裁剪会尽量保留开头的 system 消息 |
| `size_t windowSize() const` | 当前窗口设置（0 = 未启用） |

### 访问与修改

| 方法 | 说明 |
| --- | --- |
| `size_t size() const` / `bool empty() const` | 当前条数 / 是否为空 |
| `const char* roleAt(size_t index) const` | 第 index 条的角色（越界返回 `nullptr`） |
| `const char* contentAt(size_t index) const` | 第 index 条的正文（越界返回 `nullptr`） |
| `int set(size_t index, const char* role, const char* content)` | 改写指定条目；index 越界返回 `AIKIT_ERR_PARSE`，参数空返回 `AIKIT_ERR_NULL_ARG` |
| `int popBack()` | 删除**最后一条**（典型用途：回滚一条不理想的回复再重试）；空表返回 `AIKIT_ERR_NULL_ARG` |
| `void clear()` | 清空全部历史并**释放堆内存** |
| `size_t totalBytes() const` | role + content 总字节数（不含结构开销），用于监控内存水位 |

### 转换与持久化

| 方法 | 说明 |
| --- | --- |
| `const AIKitMessage* messages() const` | 返回**视图数组**（元素 `{role, content}` 指向内部字符串），可直接喂给 `ai.chat(history.messages(), history.size(), reply)`。视图在下一次 `append` / `set` / `clear` 之前保持有效 |
| `int exportJSON(String& out) const` / `String exportJSON() const` | 导出为 `[{"role":"user","content":"..."}, ...]`（OpenAI messages 格式）。返回 `AIKIT_OK` / `AIKIT_ERR_OOM` |
| `int importJSON(const char* json)` / `importJSON(const String& json)` | **原子导入**：先清空现有历史，再从 JSON 数组逐条恢复；非 JSON 数组或元素缺 `role`/`content` 时返回 `AIKIT_ERR_PARSE` 且**整体不导入**（现状不变）。也可能透传 append 期错误（`AIKIT_ERR_OOM` / `AIKIT_ERR_HISTORY_LIMIT`） |

## 动态内存机制

| 机制 | 行为 |
| --- | --- |
| 深拷贝 + 堆存储 | 每条消息的 role / content 各存为一个 `String`；调用方源缓冲可复用 |
| 倍增扩容 | 内部数组按需 ×2 扩容，只增不减；`clear()` 才真正释放堆 |
| 安全阀 1：条数 | `AIKIT_HISTORY_MAX_MESSAGES`（默认 **128 条**），超出 `append()` 返回 `AIKIT_ERR_HISTORY_LIMIT` |
| 安全阀 2：字节 | `AIKIT_HISTORY_MAX_BYTES`（默认 **48 KB**，role+content 总量），超出同样返回 `AIKIT_ERR_HISTORY_LIMIT` |

两道安全阀均可在编译期覆盖（`-D AIKIT_HISTORY_MAX_MESSAGES=64` 等，见 [docs/01_installation.md](01_installation.md) 宏总表）。设计取舍：**宁可报错也不静默丢弃**——超限时调用方可以决定是缩小窗口、`popBack()` 腾空间，还是导出落盘。

## 与 AIKitClient 协同

| 调用 | 自动完成 |
| --- | --- |
| `ai.chat(history, userText, reply)` / `ai.chat(history, userText, result)` | user 入史 → 携带**全部历史**请求 → 成功后回复入史；**请求失败自动回滚**本次 user 消息（历史保持一致）。回复入史失败——例如恰好触到安全阀——仅告警不报错 |
| `ai.chatStream(history, userText)` | user 入史 → 流式请求；**回复正文请自行在 `onDelta` 中拼接，成功后 `history.appendAssistant(...)` 入史**（流式模式下库内拿不到完整正文） |

两种用法叠加时注意：`setSystemPrompt()` 设置的 system 消息**不占历史**（每次请求自动前置，且历史数组以 system 开头时会自动去重）；`AIKitHistory` 内**额外**存的 `appendSystem()` 条目则随历史一起发送，二选一即可。

## 持久化：断电续聊

`exportJSON()` 的输出是纯文本的 OpenAI messages 数组，直接写文件系统；启动时 `importJSON()` 即可恢复会话。LittleFS 示例（约 15 行核心逻辑）：

```cpp
#include <LittleFS.h>

void saveHistory(AIKitHistory& history) {
  String json = history.exportJSON();
  File f = LittleFS.open("/chat.json", "w");
  if (f) { f.print(json); f.close(); }
}

void loadHistory(AIKitHistory& history) {
  if (!LittleFS.exists("/chat.json")) return;
  File f = LittleFS.open("/chat.json", "r");
  if (f) {
    String json = f.readString();          // 48KB 上限内一次读完没有压力
    f.close();
    history.importJSON(json.c_str());      // 原子导入：失败保持现状
  }
}

void setup() {
  LittleFS.begin();
  loadHistory(history);                    // 恢复上次会话
  ai.chat(history, "我们刚才聊到哪了？", reply);
  saveHistory(history);                    // 每轮（或掉电前）保存
}
```

SD 卡把 `LittleFS` 换成 `SD` + `/chat.json` 路径同理；EEPROM 则需自行分块写入（注意 48 KB 上限与 EEPROM 容量）。

## 完整示例

[examples/17_ChatHistory](../examples/17_ChatHistory/17_ChatHistory.ino) 依次演示：常驻 system 人设 + 滑动窗口 → 两轮自动多轮对话 → 导出 / 导入 → `popBack()` 回滚 → 手动组装（`messages()` + `chat(messages, count, reply)`，完全不依赖自动入史）。

> 相关阅读：Chat 重载与失败回滚细节见 [docs/03_chat.md](03_chat.md)；错误码全表见 [docs/07_advanced.md](07_advanced.md)。
