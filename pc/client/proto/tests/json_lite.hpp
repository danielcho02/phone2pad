// Tiny recursive-descent JSON reader for loading test vectors. Supports the
// subset used by proto/test-vectors/*.json: objects, arrays, strings, integers,
// booleans, null. Not a general-purpose parser (no floats/escapes beyond the
// basics, no unicode escapes). Throws std::runtime_error on malformed input.
#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace phantompad::json {

class Value;
using Array = std::vector<Value>;
using Object = std::map<std::string, Value>;

enum class Type { Null, Bool, Int, String, Array, Object };

class Value {
public:
    Value() : type_(Type::Null) {}
    explicit Value(bool b) : type_(Type::Bool), bool_(b) {}
    explicit Value(std::int64_t i) : type_(Type::Int), int_(i) {}
    explicit Value(std::string s) : type_(Type::String), str_(std::move(s)) {}
    explicit Value(Array a) : type_(Type::Array), arr_(std::move(a)) {}
    explicit Value(Object o) : type_(Type::Object), obj_(std::move(o)) {}

    Type type() const { return type_; }

    bool as_bool() const { expect(Type::Bool); return bool_; }
    std::int64_t as_int() const { expect(Type::Int); return int_; }
    const std::string& as_string() const { expect(Type::String); return str_; }
    const Array& as_array() const { expect(Type::Array); return arr_; }
    const Object& as_object() const { expect(Type::Object); return obj_; }

    bool contains(const std::string& key) const {
        return type_ == Type::Object && obj_.find(key) != obj_.end();
    }
    const Value& at(const std::string& key) const {
        expect(Type::Object);
        auto it = obj_.find(key);
        if (it == obj_.end()) throw std::runtime_error("json: missing key '" + key + "'");
        return it->second;
    }

private:
    void expect(Type t) const {
        if (type_ != t) throw std::runtime_error("json: type mismatch");
    }

    Type type_;
    bool bool_ = false;
    std::int64_t int_ = 0;
    std::string str_;
    Array arr_;
    Object obj_;
};

class Parser {
public:
    explicit Parser(const std::string& text) : s_(text) {}

    Value parse() {
        skip_ws();
        Value v = parse_value();
        skip_ws();
        return v;
    }

private:
    const std::string& s_;
    std::size_t i_ = 0;

    [[noreturn]] void fail(const std::string& msg) const {
        throw std::runtime_error("json: " + msg + " at offset " + std::to_string(i_));
    }

    void skip_ws() {
        while (i_ < s_.size()) {
            char c = s_[i_];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') ++i_;
            else break;
        }
    }

    char peek() const { return i_ < s_.size() ? s_[i_] : '\0'; }

    Value parse_value() {
        skip_ws();
        char c = peek();
        switch (c) {
            case '{': return parse_object();
            case '[': return parse_array();
            case '"': return Value(parse_string());
            case 't': case 'f': return parse_bool();
            case 'n': return parse_null();
            default:
                if (c == '-' || (c >= '0' && c <= '9')) return parse_int();
                fail("unexpected character");
        }
    }

    Value parse_object() {
        Object obj;
        ++i_;  // consume '{'
        skip_ws();
        if (peek() == '}') { ++i_; return Value(std::move(obj)); }
        for (;;) {
            skip_ws();
            if (peek() != '"') fail("expected object key");
            std::string key = parse_string();
            skip_ws();
            if (peek() != ':') fail("expected ':'");
            ++i_;
            obj.emplace(std::move(key), parse_value());
            skip_ws();
            char c = peek();
            if (c == ',') { ++i_; continue; }
            if (c == '}') { ++i_; break; }
            fail("expected ',' or '}'");
        }
        return Value(std::move(obj));
    }

    Value parse_array() {
        Array arr;
        ++i_;  // consume '['
        skip_ws();
        if (peek() == ']') { ++i_; return Value(std::move(arr)); }
        for (;;) {
            arr.push_back(parse_value());
            skip_ws();
            char c = peek();
            if (c == ',') { ++i_; continue; }
            if (c == ']') { ++i_; break; }
            fail("expected ',' or ']'");
        }
        return Value(std::move(arr));
    }

    std::string parse_string() {
        ++i_;  // consume opening quote
        std::string out;
        while (i_ < s_.size()) {
            char c = s_[i_++];
            if (c == '"') return out;
            if (c == '\\') {
                if (i_ >= s_.size()) fail("unterminated escape");
                char e = s_[i_++];
                switch (e) {
                    case '"': out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    case '/': out.push_back('/'); break;
                    case 'n': out.push_back('\n'); break;
                    case 't': out.push_back('\t'); break;
                    case 'r': out.push_back('\r'); break;
                    case 'b': out.push_back('\b'); break;
                    case 'f': out.push_back('\f'); break;
                    default: fail("unsupported escape");
                }
            } else {
                out.push_back(c);
            }
        }
        fail("unterminated string");
    }

    Value parse_bool() {
        if (s_.compare(i_, 4, "true") == 0) { i_ += 4; return Value(true); }
        if (s_.compare(i_, 5, "false") == 0) { i_ += 5; return Value(false); }
        fail("invalid literal");
    }

    Value parse_null() {
        if (s_.compare(i_, 4, "null") == 0) { i_ += 4; return Value(); }
        fail("invalid literal");
    }

    Value parse_int() {
        std::size_t start = i_;
        if (peek() == '-') ++i_;
        while (i_ < s_.size() && s_[i_] >= '0' && s_[i_] <= '9') ++i_;
        if (i_ == start) fail("invalid number");
        return Value(static_cast<std::int64_t>(std::stoll(s_.substr(start, i_ - start))));
    }
};

inline Value parse(const std::string& text) { return Parser(text).parse(); }

}  // namespace phantompad::json
