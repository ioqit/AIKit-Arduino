/*
 * AIKitHistory.cpp - 对话历史管理实现（v1.2.0）
 *
 * AIKit-Arduino (https://github.com/ioqit/AIKit-Arduino)
 */
#include "AIKitHistory.h"
#include <ArduinoJson.h>

// ===========================================================================
// 构造 / 析构
// ===========================================================================

AIKitHistory::AIKitHistory() {}

AIKitHistory::~AIKitHistory() {
    clear();
}

// ===========================================================================
// 追加
// ===========================================================================

int AIKitHistory::append(const char* role, const char* content) {
    if (role == nullptr || content == nullptr || role[0] == '\0') {
        return AIKIT_ERR_NULL_ARG;
    }

    const size_t addBytes = strlen(role) + strlen(content);

    // ---- 安全阀：总量上限 ----
    if (_count + 1 > AIKIT_HISTORY_MAX_MESSAGES) {
        return AIKIT_ERR_HISTORY_LIMIT;
    }
    if (_bytes + addBytes > AIKIT_HISTORY_MAX_BYTES) {
        return AIKIT_ERR_HISTORY_LIMIT;
    }

    // ---- 窗口裁剪（追加后总数将超窗）----
    if (_window_max > 0) {
        while (_count + 1 > _window_max) {
            // FIFO：删除最旧一条；keepSystem 时跳过开头的 system
            size_t victim = 0;
            if (_window_keep_system && _count > 0 &&
                strcmp(_items[0].role.c_str(), "system") == 0) {
                if (_count < 2) break;              // 只剩 system，不再删
                victim = 1;
            }
            _bytes -= _items[victim].role.length() + _items[victim].content.length();
            for (size_t i = victim; i + 1 < _count; i++) {
                _items[i] = _items[i + 1];           // String 移动赋值
            }
            _items[_count - 1].role = "";
            _items[_count - 1].content = "";
            _count--;
            // 重建视图（发生了整体搬移）
            for (size_t i = 0; i < _count; i++) syncView(i);
        }
    }

    // ---- 扩容（倍增）----
    if (_count == _cap) {
        size_t newCap = (_cap == 0) ? 4 : _cap * 2;
        if (newCap > AIKIT_HISTORY_MAX_MESSAGES) newCap = AIKIT_HISTORY_MAX_MESSAGES;
        Item* ni = new (std::nothrow) Item[newCap];
        if (ni == nullptr) return AIKIT_ERR_OOM;
        for (size_t i = 0; i < _count; i++) ni[i] = _items[i];
        delete[] _items;
        _items = ni;

        AIKitMessage* nv = new (std::nothrow) AIKitMessage[newCap];
        if (nv == nullptr) return AIKIT_ERR_OOM;   // items 已扩成功，view 下次再试
        if (_view != nullptr) {
            for (size_t i = 0; i < _count; i++) nv[i] = _view[i];
        }
        delete[] _view;
        _view = nv;
        _cap = newCap;
    }

    // ---- 写入 ----
    Item& it = _items[_count];
    it.role = role;
    it.content = content;
    if (it.role.length() != strlen(role) ||
        it.content.length() != strlen(content)) {
        // String 赋值失败（OOM）：清掉本次写入，返回错误
        it.role = "";
        it.content = "";
        return AIKIT_ERR_OOM;
    }
    _bytes += addBytes;
    syncView(_count);
    _count++;
    return AIKIT_OK;
}

int AIKitHistory::append(const char* role, const String& content) {
    return append(role, content.c_str());
}

int AIKitHistory::appendUser(const char* content)      { return append("user", content); }
int AIKitHistory::appendUser(const String& content)    { return append("user", content); }
int AIKitHistory::appendAssistant(const char* content) { return append("assistant", content); }
int AIKitHistory::appendAssistant(const String& content){ return append("assistant", content); }
int AIKitHistory::appendSystem(const char* content)    { return append("system", content); }
int AIKitHistory::appendSystem(const String& content)  { return append("system", content); }

int AIKitHistory::append(const AIKitMessage& msg) {
    return append(msg.role, msg.content);
}

// ===========================================================================
// 窗口控制
// ===========================================================================

void AIKitHistory::setWindow(size_t maxMessages, bool keepSystem) {
    _window_max = maxMessages;
    _window_keep_system = keepSystem;
    // 立即裁剪到新窗口
    if (_window_max > 0) {
        while (_count > _window_max) {
            size_t victim = 0;
            if (_window_keep_system && _count > 0 &&
                strcmp(_items[0].role.c_str(), "system") == 0) {
                if (_count < 2) break;
                victim = 1;
            }
            _bytes -= _items[victim].role.length() + _items[victim].content.length();
            for (size_t i = victim; i + 1 < _count; i++) {
                _items[i] = _items[i + 1];
            }
            _items[_count - 1].role = "";
            _items[_count - 1].content = "";
            _count--;
            for (size_t i = 0; i < _count; i++) syncView(i);
        }
    }
}

// ===========================================================================
// 访问 / 修改
// ===========================================================================

const char* AIKitHistory::roleAt(size_t index) const {
    return (index < _count) ? _items[index].role.c_str() : nullptr;
}

const char* AIKitHistory::contentAt(size_t index) const {
    return (index < _count) ? _items[index].content.c_str() : nullptr;
}

int AIKitHistory::set(size_t index, const char* role, const char* content) {
    if (role == nullptr || content == nullptr || role[0] == '\0') {
        return AIKIT_ERR_NULL_ARG;
    }
    if (index >= _count) return AIKIT_ERR_PARSE;   // 越界（参数错误）
    _bytes -= _items[index].role.length() + _items[index].content.length();
    _items[index].role = role;
    _items[index].content = content;
    _bytes += strlen(role) + strlen(content);
    if (_bytes > AIKIT_HISTORY_MAX_BYTES) {
        // 回滚（尽力）
        _items[index].role = "?";
        _items[index].content = "";
        return AIKIT_ERR_HISTORY_LIMIT;
    }
    syncView(index);
    return AIKIT_OK;
}

int AIKitHistory::popBack() {
    if (_count == 0) return AIKIT_ERR_NULL_ARG;
    _count--;
    _bytes -= _items[_count].role.length() + _items[_count].content.length();
    _items[_count].role = "";
    _items[_count].content = "";
    return AIKIT_OK;
}

void AIKitHistory::clear() {
    delete[] _items;
    delete[] _view;
    _items = nullptr;
    _view = nullptr;
    _cap = 0;
    _count = 0;
    _bytes = 0;
}

// ===========================================================================
// JSON 导出 / 导入
// ===========================================================================

int AIKitHistory::exportJSON(String& out) const {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (size_t i = 0; i < _count; i++) {
        JsonObject m = arr.add<JsonObject>();
        m["role"] = _items[i].role;
        m["content"] = _items[i].content;
    }
    if (doc.overflowed()) return AIKIT_ERR_OOM;
    out = "";
    serializeJson(doc, out);
    return (out.length() > 0 || _count == 0) ? AIKIT_OK : AIKIT_ERR_OOM;
}

String AIKitHistory::exportJSON() const {
    String s;
    exportJSON(s);
    return s;
}

int AIKitHistory::importJSON(const char* json) {
    if (json == nullptr) return AIKIT_ERR_NULL_ARG;
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json);
    if (err != DeserializationError::Ok || !doc.is<JsonArray>()) {
        return AIKIT_ERR_PARSE;
    }
    // 先在临时对象上逐条构建，全部成功后一次性接管（原子导入）
    AIKitHistory tmp;
    for (JsonVariantConst m : doc.as<JsonArrayConst>()) {
        const char* role = m["role"];
        const char* content = m["content"];
        if (role == nullptr || content == nullptr || role[0] == '\0') {
            return AIKIT_ERR_PARSE;   // 整体不导入
        }
        int rc = tmp.append(role, content);
        if (rc != AIKIT_OK) return rc;
    }
    // 交换（把 tmp 的内部指针接管过来）
    delete[] _items;
    delete[] _view;
    _items = tmp._items;
    _view = tmp._view;
    _cap = tmp._cap;
    _count = tmp._count;
    _bytes = tmp._bytes;
    tmp._items = nullptr;
    tmp._view = nullptr;
    tmp._cap = 0;
    tmp._count = 0;
    tmp._bytes = 0;
    return AIKIT_OK;
}

int AIKitHistory::importJSON(const String& json) {
    return importJSON(json.c_str());
}

// ===========================================================================
// 内部
// ===========================================================================

void AIKitHistory::syncView(size_t index) {
    if (_view != nullptr && index < _cap) {
        _view[index].role = _items[index].role.c_str();
        _view[index].content = _items[index].content.c_str();
    }
}
