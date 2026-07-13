#pragma once

#include <map>
#include <string>
#include <vector>

namespace billiardgl { namespace json {

class Value {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };
    using Array = std::vector<Value>;
    using Object = std::map<std::string, Value>;

    Value();
    explicit Value(bool value);
    explicit Value(double value);
    explicit Value(int value);
    explicit Value(const std::string& value);
    explicit Value(const char* value);
    explicit Value(const Array& value);
    explicit Value(const Object& value);

    static Value array();
    static Value object();
    Type type() const { return type_; }
    bool isNull() const { return type_ == Type::Null; }
    bool isBool() const { return type_ == Type::Bool; }
    bool isNumber() const { return type_ == Type::Number; }
    bool isString() const { return type_ == Type::String; }
    bool isArray() const { return type_ == Type::Array; }
    bool isObject() const { return type_ == Type::Object; }
    bool asBool() const;
    double asNumber() const;
    int asInt() const;
    const std::string& asString() const;
    const Array& asArray() const;
    Array& asArray();
    const Object& asObject() const;
    Object& asObject();
    const Value& at(const std::string& key) const;
    Value& operator[](const std::string& key);
    bool has(const std::string& key) const;

private:
    Type type_;
    bool boolean_;
    double number_;
    std::string string_;
    Array array_;
    Object object_;
};

struct ParseResult {
    bool ok = false;
    Value value;
    std::string error;
};

ParseResult parse(const std::string& text);
std::string stringify(const Value& value);

} }  // namespace billiardgl::json
