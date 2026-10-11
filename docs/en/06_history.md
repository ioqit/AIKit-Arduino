# 06 - Chat History Management (AIKitHistory)

English | **[简体中文](../06_history.md)**

`AIKitHistory` (added in v1.2.0, [src/AIKitHistory.h](../../src/AIKitHistory.h)) tackles a classic MCU problem: **safely maintaining multi-turn conversation context within limited RAM**. Messages are deep-copied onto the heap, capacity doubles on demand, two safety valves keep the heap from being exhausted, and OpenAI messages-format JSON export / import is supported (persist to LittleFS / SD / EEPROM).

```cpp
AIKitHistory history;
history.appendSystem("You are an Arduino assistant.");
history.setWindow(8);                    // sliding window, prevents unbounded context growth

String reply;
ai.chat(history, "My name is Xiao Ming", reply);     // user into history → request → reply into history
ai.chat(history, "What is my name?", reply);         // the second turn remembers the previous one
```

## Complete API

### Appending messages

Strings are **deep-copied**; the source string can be released right after the call:

| Method | Description |
| --- | --- |
| `int append(const char* role, const char* content)` | Appends one message. `role` takes OpenAI-compatible values: `"system"` / `"user"` / `"assistant"` / `"tool"` |
| `int append(const char* role, const String& content)` | Same as above (`String` content overload) |
| `int append(const AIKitMessage& msg)` | Appends from the lightweight message struct (the `content` pointer's target is copied) |
| `int appendUser(content)` / `appendAssistant(content)` / `appendSystem(content)` | Convenience methods for the three common roles (each with `const char*` / `const String&` overloads) |

**Return values**: `AIKIT_OK` / `AIKIT_ERR_NULL_ARG` (null pointer or empty role) / `AIKIT_ERR_OOM` (heap memory insufficient) / `AIKIT_ERR_HISTORY_LIMIT` (a safety valve was hit — **never a silent drop**).

### Window control

| Method | Description |
| --- | --- |
| `void setWindow(size_t maxMessages, bool keepSystem = true)` | Enables the sliding window (off by default, `maxMessages=0`). Once enabled, an `append` past the limit **drops from the oldest message first** (FIFO emulating a fixed context window); with `keepSystem=true` trimming tries hard to keep the leading system message |
| `size_t windowSize() const` | Current window setting (0 = disabled) |

### Access & modification

| Method | Description |
| --- | --- |
| `size_t size() const` / `bool empty() const` | Current message count / whether empty |
| `const char* roleAt(size_t index) const` | Role of message `index` (out of range → `nullptr`) |
| `const char* contentAt(size_t index) const` | Content of message `index` (out of range → `nullptr`) |
| `int set(size_t index, const char* role, const char* content)` | Rewrites the given entry; an out-of-range index returns `AIKIT_ERR_PARSE`, null arguments return `AIKIT_ERR_NULL_ARG` |
| `int popBack()` | Removes the **last** message (typical use: roll back an unsatisfactory reply and retry); returns `AIKIT_ERR_NULL_ARG` on an empty list |
| `void clear()` | Clears all history and **releases heap memory** |
| `size_t totalBytes() const` | Total bytes of role + content (excluding struct overhead), for monitoring memory pressure |

### Conversion & persistence

| Method | Description |
| --- | --- |
| `const AIKitMessage* messages() const` | Returns a **view array** (elements `{role, content}` pointing into internal strings), directly usable as `ai.chat(history.messages(), history.size(), reply)`. The view stays valid until the next `append` / `set` / `clear` |
| `int exportJSON(String& out) const` / `String exportJSON() const` | Exports as `[{"role":"user","content":"..."}, ...]` (OpenAI messages format). Returns `AIKIT_OK` / `AIKIT_ERR_OOM` |
| `int importJSON(const char* json)` / `importJSON(const String& json)` | **Atomic import**: clears the existing history first, then restores message by message from the JSON array; a non-array JSON or an element missing `role`/`content` returns `AIKIT_ERR_PARSE` and **imports nothing** (state unchanged). Append-time errors (`AIKIT_ERR_OOM` / `AIKIT_ERR_HISTORY_LIMIT`) also pass through |

## Dynamic memory mechanics

| Mechanism | Behavior |
| --- | --- |
| Deep copy + heap storage | Each message's role / content is stored as a `String`; the caller's source buffers can be reused |
| Doubling growth | The internal array grows ×2 on demand and never shrinks; only `clear()` actually frees the heap |
| Safety valve 1: message count | `AIKIT_HISTORY_MAX_MESSAGES` (default **128 messages**); beyond it `append()` returns `AIKIT_ERR_HISTORY_LIMIT` |
| Safety valve 2: bytes | `AIKIT_HISTORY_MAX_BYTES` (default **48 KB** of total role+content); beyond it likewise `AIKIT_ERR_HISTORY_LIMIT` |

Both safety valves can be overridden at compile time (`-D AIKIT_HISTORY_MAX_MESSAGES=64` etc., see the macro table in [01_installation.md](01_installation.md)). Design trade-off: **fail loudly rather than silently drop** — on hitting a limit, the caller can decide whether to shrink the window, `popBack()` to free space, or export to disk.

## Working with AIKitClient

| Call | Done automatically |
| --- | --- |
| `ai.chat(history, userText, reply)` / `ai.chat(history, userText, result)` | user into history → request carrying the **entire history** → on success the reply goes into history; **a failed request automatically rolls back** this turn's user message (history stays consistent). A failure while appending the reply — e.g. a safety valve happens to be hit — only warns, it does not error |
| `ai.chatStream(history, userText)` | user into history → streaming request; **assemble the reply body yourself in `onDelta` and, on success, append it via `history.appendAssistant(...)`** (in streaming mode the library never sees the full body) |

One thing to note when combining both styles: the system message set by `setSystemPrompt()` **does not occupy history** (auto-prepended to every request, and deduplicated when the history array already starts with system); an `appendSystem()` entry stored **additionally** in `AIKitHistory` is sent as part of the history — pick one of the two.

## Persistence: surviving power loss

The output of `exportJSON()` is a plain-text OpenAI messages array — write it straight to a file system; calling `importJSON()` at startup restores the session. LittleFS example (~15 lines of core logic):

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
    String json = f.readString();          // well within the 48 KB cap — one read is fine
    f.close();
    history.importJSON(json.c_str());      // atomic import: on failure the state is unchanged
  }
}

void setup() {
  LittleFS.begin();
  loadHistory(history);                    // restore the previous session
  ai.chat(history, "Where did we leave off?", reply);
  saveHistory(history);                    // save every turn (or before losing power)
}
```

For an SD card, swap `LittleFS` for `SD` and keep the `/chat.json` path; for EEPROM you need to write in chunks yourself (mind the 48 KB cap and your EEPROM size).

## Full example

[examples/17_ChatHistory](../../examples/17_ChatHistory/17_ChatHistory.ino) walks through, in order: a persistent system persona + sliding window → two automatic multi-turn exchanges → export / import → `popBack()` rollback → manual assembly (`messages()` + `chat(messages, count, reply)`, completely independent of automatic bookkeeping).

> Further reading: chat overloads and failure-rollback details in [03_chat.md](03_chat.md); the full error-code table in [07_advanced.md](07_advanced.md).
