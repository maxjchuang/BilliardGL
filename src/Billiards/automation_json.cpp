#include "automation_json.h"

#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace billiardgl { namespace json {

Value::Value() : type_(Type::Null), boolean_(false), number_(0.0) {}
Value::Value(bool value) : type_(Type::Bool), boolean_(value), number_(0.0) {}
Value::Value(double value) : type_(Type::Number), boolean_(false), number_(value) {}
Value::Value(int value) : type_(Type::Number), boolean_(false), number_(value) {}
Value::Value(const std::string& value) : type_(Type::String), boolean_(false), number_(0.0), string_(value) {}
Value::Value(const char* value) : Value(std::string(value)) {}
Value::Value(const Array& value) : type_(Type::Array), boolean_(false), number_(0.0), array_(value) {}
Value::Value(const Object& value) : type_(Type::Object), boolean_(false), number_(0.0), object_(value) {}
Value Value::array() { return Value(Array{}); }
Value Value::object() { return Value(Object{}); }

bool Value::asBool() const { if (!isBool()) throw std::runtime_error("not bool"); return boolean_; }
double Value::asNumber() const { if (!isNumber()) throw std::runtime_error("not number"); return number_; }
int Value::asInt() const
{
    if (!isNumber() || std::floor(number_) != number_ || number_ < std::numeric_limits<int>::min() || number_ > std::numeric_limits<int>::max())
        throw std::runtime_error("not int");
    return static_cast<int>(number_);
}
const std::string& Value::asString() const { if (!isString()) throw std::runtime_error("not string"); return string_; }
const Value::Array& Value::asArray() const { if (!isArray()) throw std::runtime_error("not array"); return array_; }
Value::Array& Value::asArray() { if (!isArray()) throw std::runtime_error("not array"); return array_; }
const Value::Object& Value::asObject() const { if (!isObject()) throw std::runtime_error("not object"); return object_; }
Value::Object& Value::asObject() { if (!isObject()) throw std::runtime_error("not object"); return object_; }
const Value& Value::at(const std::string& key) const
{
    const Object& object = asObject();
    Object::const_iterator found = object.find(key);
    if (found == object.end()) throw std::out_of_range(key);
    return found->second;
}
Value& Value::operator[](const std::string& key) { return asObject()[key]; }
bool Value::has(const std::string& key) const { return isObject() && object_.find(key) != object_.end(); }

namespace {

void appendUtf8(std::string& out, unsigned code)
{
    if (code <= 0x7f) out.push_back(static_cast<char>(code));
    else if (code <= 0x7ff) { out.push_back(static_cast<char>(0xc0 | code >> 6)); out.push_back(static_cast<char>(0x80 | code & 0x3f)); }
    else { out.push_back(static_cast<char>(0xe0 | code >> 12)); out.push_back(static_cast<char>(0x80 | code >> 6 & 0x3f)); out.push_back(static_cast<char>(0x80 | code & 0x3f)); }
}

class Parser {
public:
    explicit Parser(const std::string& text) : text_(text) {}
    Value run()
    {
        if (text_.size() > 1024 * 1024) fail("message too large");
        Value value = parseValue(0);
        whitespace();
        if (position_ != text_.size()) fail("trailing data");
        return value;
    }

private:
    Value parseValue(int depth)
    {
        if (depth > 64) fail("nesting too deep");
        whitespace();
        if (position_ >= text_.size()) fail("expected value");
        const char c = text_[position_];
        if (c == '{') return parseObject(depth + 1);
        if (c == '[') return parseArray(depth + 1);
        if (c == '"') return Value(parseString());
        if (c == 't') { literal("true"); return Value(true); }
        if (c == 'f') { literal("false"); return Value(false); }
        if (c == 'n') { literal("null"); return Value(); }
        if (c == '-' || (c >= '0' && c <= '9')) return parseNumber();
        fail("invalid value"); return Value();
    }

    Value parseObject(int depth)
    {
        ++position_;
        Value result = Value::object();
        whitespace();
        if (consume('}')) return result;
        while (true) {
            whitespace();
            if (position_ >= text_.size() || text_[position_] != '"') fail("expected object key");
            std::string key = parseString();
            whitespace();
            if (!consume(':')) fail("expected colon");
            result[key] = parseValue(depth);
            whitespace();
            if (consume('}')) return result;
            if (!consume(',')) fail("expected comma");
        }
    }

    Value parseArray(int depth)
    {
        ++position_;
        Value result = Value::array();
        whitespace();
        if (consume(']')) return result;
        while (true) {
            result.asArray().push_back(parseValue(depth));
            whitespace();
            if (consume(']')) return result;
            if (!consume(',')) fail("expected comma");
        }
    }

    std::string parseString()
    {
        ++position_;
        std::string out;
        while (position_ < text_.size()) {
            unsigned char c = text_[position_++];
            if (c == '"') return out;
            if (c < 0x20) fail("control character in string");
            if (c != '\\') { out.push_back(static_cast<char>(c)); continue; }
            if (position_ >= text_.size()) fail("bad escape");
            const char escaped = text_[position_++];
            const char* escapes = "\"\\/bfnrt";
            const char* values = "\"\\/\b\f\n\r\t";
            const char* found = std::strchr(escapes, escaped);
            if (found) { out.push_back(values[found - escapes]); continue; }
            if (escaped != 'u' || position_ + 4 > text_.size()) fail("bad unicode escape");
            unsigned code = 0;
            for (int i = 0; i < 4; ++i) {
                const char h = text_[position_++];
                code <<= 4;
                if (h >= '0' && h <= '9') code += h - '0';
                else if (h >= 'a' && h <= 'f') code += h - 'a' + 10;
                else if (h >= 'A' && h <= 'F') code += h - 'A' + 10;
                else fail("bad unicode hex");
            }
            appendUtf8(out, code);
        }
        fail("unterminated string"); return out;
    }

    Value parseNumber()
    {
        const std::size_t start = position_;
        if (consume('-') && position_ >= text_.size()) fail("bad number");
        if (consume('0')) { if (position_ < text_.size() && std::isdigit(text_[position_])) fail("leading zero"); }
        else { if (position_ >= text_.size() || !std::isdigit(text_[position_])) fail("bad number"); while (position_ < text_.size() && std::isdigit(text_[position_])) ++position_; }
        if (consume('.')) { if (position_ >= text_.size() || !std::isdigit(text_[position_])) fail("bad fraction"); while (position_ < text_.size() && std::isdigit(text_[position_])) ++position_; }
        if (position_ < text_.size() && (text_[position_] == 'e' || text_[position_] == 'E')) {
            ++position_; if (position_ < text_.size() && (text_[position_] == '+' || text_[position_] == '-')) ++position_;
            if (position_ >= text_.size() || !std::isdigit(text_[position_])) fail("bad exponent");
            while (position_ < text_.size() && std::isdigit(text_[position_])) ++position_;
        }
        const double number = std::strtod(text_.substr(start, position_ - start).c_str(), nullptr);
        if (!std::isfinite(number)) fail("non-finite number");
        return Value(number);
    }

    void literal(const char* value)
    {
        const std::size_t length = std::strlen(value);
        if (text_.compare(position_, length, value) != 0) fail("bad literal");
        position_ += length;
    }
    void whitespace() { while (position_ < text_.size() && (text_[position_] == ' ' || text_[position_] == '\n' || text_[position_] == '\r' || text_[position_] == '\t')) ++position_; }
    bool consume(char c) { if (position_ < text_.size() && text_[position_] == c) { ++position_; return true; } return false; }
    void fail(const char* message) const { throw std::runtime_error(message); }
    const std::string& text_;
    std::size_t position_ = 0;
};

std::string quote(const std::string& value)
{
    std::ostringstream out;
    out << '"';
    for (unsigned char c : value) {
        switch (c) {
        case '"': out << "\\\""; break; case '\\': out << "\\\\"; break;
        case '\b': out << "\\b"; break; case '\f': out << "\\f"; break;
        case '\n': out << "\\n"; break; case '\r': out << "\\r"; break; case '\t': out << "\\t"; break;
        default: if (c < 0x20) out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c) << std::dec; else out << c;
        }
    }
    out << '"'; return out.str();
}

}  // namespace

ParseResult parse(const std::string& text)
{
    ParseResult result;
    try { result.value = Parser(text).run(); result.ok = true; }
    catch (const std::exception& error) { result.error = error.what(); }
    return result;
}

std::string stringify(const Value& value)
{
    if (value.isNull()) return "null";
    if (value.isBool()) return value.asBool() ? "true" : "false";
    if (value.isNumber()) { if (!std::isfinite(value.asNumber())) throw std::runtime_error("non-finite number"); std::ostringstream out; out << std::setprecision(17) << value.asNumber(); return out.str(); }
    if (value.isString()) return quote(value.asString());
    if (value.isArray()) { std::string out = "["; bool first = true; for (const Value& item : value.asArray()) { if (!first) out += ','; first = false; out += stringify(item); } return out + "]"; }
    std::string out = "{"; bool first = true; for (const auto& item : value.asObject()) { if (!first) out += ','; first = false; out += quote(item.first) + ":" + stringify(item.second); } return out + "}";
}

} }  // namespace billiardgl::json
