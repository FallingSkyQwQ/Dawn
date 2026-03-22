#include "dawn/infra/json/simple_json.h"

#include <cctype>
#include <cmath>
#include <sstream>
#include <stdexcept>

namespace dawn::infra::json {

namespace {

class Parser {
public:
    explicit Parser(std::string_view text) : text_(text) {}

    ParseResult parse() {
        skip_whitespace();
        auto value = parse_value();
        if (!result_.error.message.empty()) {
            return result_;
        }
        skip_whitespace();
        if (!at_end()) {
            fail("unexpected trailing characters");
            return result_;
        }
        result_.ok = true;
        result_.value = std::move(value);
        return result_;
    }

private:
    Value parse_value() {
        skip_whitespace();
        if (at_end()) {
            return fail("unexpected end of input");
        }

        const char ch = peek();
        if (ch == '"') {
            return Value(parse_string());
        }
        if (ch == '{') {
            return parse_object();
        }
        if (ch == '[') {
            return parse_array();
        }
        if (ch == 't') {
            return parse_literal("true", Value(true));
        }
        if (ch == 'f') {
            return parse_literal("false", Value(false));
        }
        if (ch == 'n') {
            return parse_literal("null", Value(nullptr));
        }
        return parse_number();
    }

    Value parse_literal(std::string_view literal, Value value) {
        if (text_.substr(pos_, literal.size()) != literal) {
            return fail("invalid literal");
        }
        pos_ += literal.size();
        return value;
    }

    Value parse_number() {
        const std::size_t start = pos_;
        if (peek() == '-') {
            advance();
        }
        while (!at_end() && std::isdigit(static_cast<unsigned char>(peek()))) {
            advance();
        }
        if (!at_end() && peek() == '.') {
            advance();
            while (!at_end() && std::isdigit(static_cast<unsigned char>(peek()))) {
                advance();
            }
        }
        if (!at_end() && (peek() == 'e' || peek() == 'E')) {
            advance();
            if (!at_end() && (peek() == '+' || peek() == '-')) {
                advance();
            }
            while (!at_end() && std::isdigit(static_cast<unsigned char>(peek()))) {
                advance();
            }
        }

        const auto number_text = std::string(text_.substr(start, pos_ - start));
        try {
            return Value(std::stod(number_text));
        } catch (const std::exception&) {
            return fail("invalid number");
        }
    }

    Value parse_array() {
        advance(); // [
        Value::Array array;
        skip_whitespace();
        if (!at_end() && peek() == ']') {
            advance();
            return Value(std::move(array));
        }

        while (true) {
            array.push_back(parse_value());
            if (!result_.error.message.empty()) {
                return result_.value;
            }
            skip_whitespace();
            if (at_end()) {
                return fail("unterminated array");
            }
            if (peek() == ']') {
                advance();
                break;
            }
            if (peek() != ',') {
                return fail("expected ',' in array");
            }
            advance();
        }
        return Value(std::move(array));
    }

    Value parse_object() {
        advance(); // {
        Value::Object object;
        skip_whitespace();
        if (!at_end() && peek() == '}') {
            advance();
            return Value(std::move(object));
        }

        while (true) {
            skip_whitespace();
            if (at_end() || peek() != '"') {
                return fail("expected string key");
            }
            const auto key = parse_string();
            skip_whitespace();
            if (at_end() || peek() != ':') {
                return fail("expected ':' after object key");
            }
            advance();
            object.emplace(std::move(key), parse_value());
            if (!result_.error.message.empty()) {
                return result_.value;
            }
            skip_whitespace();
            if (at_end()) {
                return fail("unterminated object");
            }
            if (peek() == '}') {
                advance();
                break;
            }
            if (peek() != ',') {
                return fail("expected ',' in object");
            }
            advance();
        }
        return Value(std::move(object));
    }

    std::string parse_string() {
        if (peek() != '"') {
            fail("expected string");
            return {};
        }
        advance();
        std::string result;
        while (!at_end()) {
            const char ch = advance();
            if (ch == '"') {
                return result;
            }
            if (ch == '\\') {
                if (at_end()) {
                    fail("unterminated escape sequence");
                    return {};
                }
                const char escaped = advance();
                switch (escaped) {
                case '"': result.push_back('"'); break;
                case '\\': result.push_back('\\'); break;
                case '/': result.push_back('/'); break;
                case 'b': result.push_back('\b'); break;
                case 'f': result.push_back('\f'); break;
                case 'n': result.push_back('\n'); break;
                case 'r': result.push_back('\r'); break;
                case 't': result.push_back('\t'); break;
                default:
                    fail("unsupported escape sequence");
                    return {};
                }
                continue;
            }
            result.push_back(ch);
        }
        fail("unterminated string");
        return {};
    }

    Value fail(std::string message) {
        result_.ok = false;
        result_.error = ParseError{pos_, std::move(message)};
        return Value();
    }

    char peek() const {
        return text_[pos_];
    }

    char advance() {
        return text_[pos_++];
    }

    void skip_whitespace() {
        while (!at_end() && std::isspace(static_cast<unsigned char>(peek()))) {
            ++pos_;
        }
    }

    bool at_end() const {
        return pos_ >= text_.size();
    }

    std::string_view text_;
    std::size_t pos_ = 0;
    ParseResult result_;
};

std::string indent(int count) {
    return std::string(static_cast<std::size_t>(count), ' ');
}

std::string escape(const std::string& input) {
    std::string output;
    output.reserve(input.size() + 8);
    for (const char ch : input) {
        switch (ch) {
        case '"': output += "\\\""; break;
        case '\\': output += "\\\\"; break;
        case '\b': output += "\\b"; break;
        case '\f': output += "\\f"; break;
        case '\n': output += "\\n"; break;
        case '\r': output += "\\r"; break;
        case '\t': output += "\\t"; break;
        default: output.push_back(ch); break;
        }
    }
    return output;
}

std::string stringify_impl(const Value& value, int indent_size, int depth) {
    std::ostringstream out;
    if (value.is_null()) {
        out << "null";
    } else if (value.is_bool()) {
        out << (value.as_bool() ? "true" : "false");
    } else if (value.is_number()) {
        out << value.as_number();
    } else if (value.is_string()) {
        out << '"' << escape(value.as_string()) << '"';
    } else if (value.is_array()) {
        const auto& array = value.as_array();
        out << '[';
        if (!array.empty()) {
            const bool pretty = indent_size > 0;
            for (std::size_t i = 0; i < array.size(); ++i) {
                if (pretty) {
                    out << '\n' << indent((depth + 1) * indent_size);
                }
                out << stringify_impl(array[i], indent_size, depth + 1);
                if (i + 1 < array.size()) {
                    out << ',';
                }
            }
            if (pretty) {
                out << '\n' << indent(depth * indent_size);
            }
        }
        out << ']';
    } else if (value.is_object()) {
        const auto& object = value.as_object();
        out << '{';
        if (!object.empty()) {
            const bool pretty = indent_size > 0;
            std::size_t index = 0;
            for (const auto& [key, child] : object) {
                if (pretty) {
                    out << '\n' << indent((depth + 1) * indent_size);
                }
                out << '"' << escape(key) << "\":";
                if (pretty) {
                    out << ' ';
                }
                out << stringify_impl(child, indent_size, depth + 1);
                if (++index < object.size()) {
                    out << ',';
                }
            }
            if (pretty) {
                out << '\n' << indent(depth * indent_size);
            }
        }
        out << '}';
    }
    return out.str();
}

} // namespace

Value::Value() : storage_(nullptr) {}
Value::Value(std::nullptr_t) : storage_(nullptr) {}
Value::Value(bool value) : storage_(value) {}
Value::Value(int value) : storage_(static_cast<double>(value)) {}
Value::Value(double value) : storage_(value) {}
Value::Value(const char* value) : storage_(std::string(value)) {}
Value::Value(std::string value) : storage_(std::move(value)) {}
Value::Value(Array value) : storage_(std::move(value)) {}
Value::Value(Object value) : storage_(std::move(value)) {}

bool Value::is_null() const { return std::holds_alternative<std::nullptr_t>(storage_); }
bool Value::is_bool() const { return std::holds_alternative<bool>(storage_); }
bool Value::is_number() const { return std::holds_alternative<double>(storage_); }
bool Value::is_string() const { return std::holds_alternative<std::string>(storage_); }
bool Value::is_array() const { return std::holds_alternative<Array>(storage_); }
bool Value::is_object() const { return std::holds_alternative<Object>(storage_); }

const Value::Storage& Value::storage() const { return storage_; }
Value::Storage& Value::storage() { return storage_; }

const Value::Array& Value::as_array() const { return std::get<Array>(storage_); }
Value::Array& Value::as_array() { return std::get<Array>(storage_); }
const Value::Object& Value::as_object() const { return std::get<Object>(storage_); }
Value::Object& Value::as_object() { return std::get<Object>(storage_); }
const std::string& Value::as_string() const { return std::get<std::string>(storage_); }
std::string& Value::as_string() { return std::get<std::string>(storage_); }
double Value::as_number() const { return std::get<double>(storage_); }
bool Value::as_bool() const { return std::get<bool>(storage_); }

ParseResult parse(std::string_view text) {
    return Parser(text).parse();
}

std::string stringify(const Value& value, int indent) {
    return stringify_impl(value, indent, 0);
}

const Value* find(const Value::Object& object, const std::string& key) {
    const auto it = object.find(key);
    if (it == object.end()) {
        return nullptr;
    }
    return &it->second;
}

Value* find(Value::Object& object, const std::string& key) {
    const auto it = object.find(key);
    if (it == object.end()) {
        return nullptr;
    }
    return &it->second;
}

} // namespace dawn::infra::json
