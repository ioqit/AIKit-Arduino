/*
 * AIKitHistory.h - 对话历史管理（v1.2.0）
 *
 * 设计目标：在 RAM 受限的 MCU 上安全地维护多轮对话上下文。
 *   - 消息以深拷贝方式保存在堆上，容量按需倍增（动态内存拓展）
 *   - 两个安全阀防止耗尽堆：条数上限 + 总字节上限（AIKitConfig.h）
 *   - JSON 导出 / 导入（ArduinoJson），可持久化到 LittleFS/SD/EEPROM
 *   - messages() 返回可直接喂给 AIKitClient::chat() 的视图数组
 *
 * 与 AIKitClient 协同：
 *   AIKitHistory history;
 *   ai.chat(history, "你好", reply);   // 自动：user 入历史 → 请求 → assistant 入历史
 *
 * AIKit-Arduino (https://github.com/ioqit/AIKit-Arduino)
 */
#pragma once

#include <Arduino.h>
#include "AIKitConfig.h"
#include "AIKitTypes.h"

class AIKitHistory {
public:
    AIKitHistory();
    ~AIKitHistory();

    // -------------------------------------------------------------------
    // 追加消息（字符串被深拷贝，调用后源可释放）
    // -------------------------------------------------------------------
    // role："system" / "user" / "assistant" / "tool"（OpenAI 兼容取值）
    // 返回 AIKIT_OK / AIKIT_ERR_NULL_ARG / AIKIT_ERR_OOM / AIKIT_ERR_HISTORY_LIMIT
    int append(const char* role, const char* content);
    int append(const char* role, const String& content);
    // 便捷方法：三种常见角色
    int appendUser(const char* content);
    int appendUser(const String& content);
    int appendAssistant(const char* content);
    int appendAssistant(const String& content);
    int appendSystem(const char* content);
    int appendSystem(const String& content);
    // 从轻量消息结构追加（content 指针内容会被拷贝）
    int append(const AIKitMessage& msg);

    // -------------------------------------------------------------------
    // 窗口控制（超出后丢弃最旧消息，模拟固定上下文窗口）
    // -------------------------------------------------------------------
    // maxMessages=0：不启用窗口（默认）。启用后每次 append 若超限，
    // 从头删除最旧条目（保持 FIFO 窗口）。删除 system 消息与否由
    // keepSystem 决定（默认 true：优先保留开头的 system 消息）。
    void setWindow(size_t maxMessages, bool keepSystem = true);
    size_t windowSize() const { return _window_max; }

    // -------------------------------------------------------------------
    // 访问
    // -------------------------------------------------------------------
    size_t size() const { return _count; }
    bool   empty() const { return _count == 0; }
    const char* roleAt(size_t index) const;       // 越界返回 nullptr
    const char* contentAt(size_t index) const;     // 越界返回 nullptr

    // 修改指定条目（index 越界返回 AIKIT_ERR_NULL_ARG 之外的参数错误）
    int set(size_t index, const char* role, const char* content);

    // 删除最后一条（回滚一次错误回复）；空表返回 AIKIT_ERR_NULL_ARG
    int popBack();

    // 清空全部历史并释放堆内存
    void clear();

    // 角色与正文总字节数（不含结构开销），用于监控内存水位
    size_t totalBytes() const { return _bytes; }

    // -------------------------------------------------------------------
    // 转换为 AIKitClient 可用的消息数组
    // -------------------------------------------------------------------
    // 返回内部视图数组指针：元素 {role, content} 指向本对象内部字符串，
    // 在下一次 append/set/clear 之前保持有效。
    //   ai.chat(history.messages(), history.size(), reply);
    const AIKitMessage* messages() const { return _view; }

    // -------------------------------------------------------------------
    // JSON 导出 / 导入
    // -------------------------------------------------------------------
    // 导出为 [{"role":"user","content":"..."}, ...]（OpenAI messages 格式）
    // 返回 AIKIT_OK / AIKIT_ERR_OOM
    int exportJSON(String& out) const;
    String exportJSON() const;

    // 导入：清空现有历史，从 JSON 数组逐条恢复。
    // 返回 AIKIT_OK / AIKIT_ERR_PARSE / 追加期错误（见 append 返回值）。
    // 非 JSON 数组或元素缺 role/content 时返回 AIKIT_ERR_PARSE（整体不导入）。
    int importJSON(const char* json);
    int importJSON(const String& json);

private:
    void syncView(size_t index);                // 同步视图数组第 i 项

    struct Item {
        String role;
        String content;
    };

    Item*         _items = nullptr;   // 堆上动态数组
    AIKitMessage* _view  = nullptr;   // 与 _items 平行的 {c_str(), c_str()} 视图
    size_t        _cap   = 0;         // 容量
    size_t        _count = 0;         // 当前条数
    size_t        _bytes = 0;         // role+content 总字节数
    size_t        _window_max = 0;    // 0=禁用窗口
    bool          _window_keep_system = true;
};
