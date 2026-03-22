#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace dawn::infra::json {

class Value {
public:
    using Array = std::vector<Value>;
    using Object = std::map<std::string, Value>;
    using Storage = std::variant<std::nullptr_t, bool, double, std::string, Array, Object>;

    Value();
    Value(std::nullptr_t);
    Value(bool value);
    Value(int value);
    Value(double value);
    Value(const char* value);
    Value(std::string value);
    Value(Array value);
    Value(Object value);

    [[nodiscard]] bool is_null() const;
    [[nodiscard]] bool is_bool() const;
    [[nodiscard]] bool is_number() const;
    [[nodiscard]] bool is_string() const;
    [[nodiscard]] bool is_array() const;
    [[nodiscard]] bool is_object() const;

    [[nodiscard]] const Storage& storage() const;
    [[nodiscard]] Storage& storage();

    [[nodiscard]] const Array& as_array() const;
    [[nodiscard]] Array& as_array();
    [[nodiscard]] const Object& as_object() const;
    [[nodiscard]] Object& as_object();
    [[nodiscard]] const std::string& as_string() const;
    [[nodiscard]] std::string& as_string();
    [[nodiscard]] double as_number() const;
    [[nodiscard]] bool as_bool() const;

private:
    Storage storage_;
};

struct ParseError {
    std::size_t position = 0;
    std::string message;
};

struct ParseResult {
    bool ok = false;
    Value value;
    ParseError error;
};

ParseResult parse(std::string_view text);
std::string stringify(const Value& value, int indent = 2);

const Value* find(const Value::Object& object, const std::string& key);
Value* find(Value::Object& object, const std::string& key);

} // namespace dawn::infra::json
