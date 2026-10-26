#pragma once
// A small JSON value with parse / dump, for the clan state file (compatible with the Node server's clan.json).
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace json {
class Value {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };
    Value() = default;
    Value(std::nullptr_t) {}
    Value(bool b) : type_(Type::Bool), number_(b ? 1 : 0) {}
    Value(int n) : type_(Type::Number), number_(double(n)) {}
    Value(std::int64_t n) : type_(Type::Number), number_(double(n)) {}
    Value(double n) : type_(Type::Number), number_(n) {}
    Value(const char *s) : type_(Type::String), string_(s) {}
    Value(std::string s) : type_(Type::String), string_(std::move(s)) {}
    static Value array() {
        Value v;
        v.type_ = Type::Array;
        return v;
    }
    static Value object() {
        Value v;
        v.type_ = Type::Object;
        return v;
    }

    Type type() const { return type_; }
    bool isNull() const { return type_ == Type::Null; }
    bool isArray() const { return type_ == Type::Array; }
    bool isObject() const { return type_ == Type::Object; }
    bool isString() const { return type_ == Type::String; }
    bool isNumber() const { return type_ == Type::Number; }
    bool boolean(bool fallback = false) const {
        return type_ == Type::Bool || type_ == Type::Number ? number_ != 0 : fallback;
    }
    std::int64_t integer(std::int64_t fallback = 0) const {
        return type_ == Type::Number ? std::int64_t(number_) : fallback;
    }
    double number(double fallback = 0) const { return type_ == Type::Number ? number_ : fallback; }
    const std::string &str() const { return string_; }
    std::string str(const std::string &fallback) const { return type_ == Type::String ? string_ : fallback; }

    // Arrays.
    std::vector<Value> &items() { return items_; }
    const std::vector<Value> &items() const { return items_; }
    void push(Value v) {
        type_ = Type::Array;
        items_.push_back(std::move(v));
    }
    // Objects (insertion order kept).
    bool has(const std::string &key) const;
    const Value &operator[](const std::string &key) const;
    Value &operator[](const std::string &key);
    const std::vector<std::pair<std::string, Value>> &members() const { return members_; }

    std::string dump() const;
    static Value parse(const std::string &text, bool *ok = nullptr);

private:
    Type type_ = Type::Null;
    double number_ = 0;
    std::string string_;
    std::vector<Value> items_;
    std::vector<std::pair<std::string, Value>> members_;
};
} // namespace json
