// MiniJsonParser.h
// A lightweight, header-only JSON parser for C++17/20
// Designed for 0-bug performance on known config structures.

#ifndef MINI_JSON_PARSER_H
#define MINI_JSON_PARSER_H

#include <string>
#include <map>
#include <vector>
#include <variant>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace BrutalDDoS {

class JsonValue;

using JsonObject = std::map<std::string, JsonValue>;
using JsonArray = std::vector<JsonValue>;

enum class JsonType { Null, Bool, Int, Double, String, Array, Object };

class JsonValue {
public:
    JsonType type;
    bool bool_val = false;
    long long int_val = 0;
    double double_val = 0.0;
    std::string string_val;
    JsonArray array_val;
    JsonObject object_val;

    JsonValue() : type(JsonType::Null) {}
    JsonValue(bool v) : type(JsonType::Bool), bool_val(v) {}
    JsonValue(long long v) : type(JsonType::Int), int_val(v) {}
    JsonValue(double v) : type(JsonType::Double), double_val(v) {}
    JsonValue(const std::string& v) : type(JsonType::String), string_val(v) {}
    JsonValue(std::initializer_list<std::pair<const std::string, JsonValue>> init) 
        : type(JsonType::Object) {
        for (auto& p : init) object_val[p.first] = p.second;
    }

    // Accessors
    bool get_bool() const { if (type != JsonType::Bool) throw std::runtime_error("Type mismatch"); return bool_val; }
    long long get_int() const { if (type != JsonType::Int) throw std::runtime_error("Type mismatch"); return int_val; }
    double get_double() const { if (type != JsonType::Double) throw std::runtime_error("Type mismatch"); return double_val; }
    std::string get_string() const { if (type != JsonType::String) throw std::runtime_error("Type mismatch"); return string_val; }
    
    const JsonValue& operator[](const std::string& key) const {
        if (type != JsonType::Object) throw std::runtime_error("Not an object");
        auto it = object_val.find(key);
        if (it == object_val.end()) throw std::runtime_error("Key not found: " + key);
        return it->second;
    }

    const JsonValue& operator[](size_t index) const {
        if (type != JsonType::Array) throw std::runtime_error("Not an array");
        if (index >= array_val.size()) throw std::out_of_range("Index out of bounds");
        return array_val[index];
    }

    size_t size() const {
        if (type == JsonType::Array) return array_val.size();
        if (type == JsonType::Object) return object_val.size();
        return 0;
    }
};

class JsonParser {
public:
    static JsonValue parse(const std::string& input) {
        JsonParser parser(input);
        return parser.parse_value();
    }

private:
    const std::string& input;
    size_t pos;

    JsonParser(const std::string& input) : input(input), pos(0) {}

    void skip_whitespace() {
        while (pos < input.size() && std::isspace(input[pos])) pos++;
    }

    char peek() { skip_whitespace(); return input[pos]; }
    char advance() { return input[pos++]; }

    std::string parse_string() {
        if (advance() != '"') throw std::runtime_error("Expected '\"'");
        std::string result;
        while (pos < input.size()) {
            char c = input[pos++];
            if (c == '"') break;
            if (c == '\\') {
                char next = input[pos++];
                switch (next) {
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case '/': result += '/'; break;
                    case 'n': result += '\n'; break;
                    case 't': result += '\t'; break;
                    case 'r': result += '\r'; break;
                    default: result += next; break;
                }
            } else {
                result += c;
            }
        }
        return result;
    }

    JsonValue parse_number() {
        size_t start = pos;
        bool is_float = false;
        if (input[pos] == '-') pos++;
        while (std::isdigit(input[pos])) pos++;
        if (input[pos] == '.') { is_float = true; pos++; while (std::isdigit(input[pos])) pos++; }
        if (input[pos] == 'e' || input[pos] == 'E') { is_float = true; pos++; if (input[pos] == '+' || input[pos] == '-') pos++; while (std::isdigit(input[pos])) pos++; }
        
        std::string num_str = input.substr(start, pos - start);
        if (is_float) return JsonValue(std::stod(num_str));
        return JsonValue(std::stoll(num_str));
    }

    JsonValue parse_value() {
        skip_whitespace();
        char c = peek();
        if (c == '{') return parse_object();
        if (c == '[') return parse_array();
        if (c == '"') return JsonValue(parse_string());
        if (c == 't' || c == 'f') return JsonValue(parse_bool());
        if (c == 'n') return parse_null();
        if (std::isdigit(c) || c == '-') return parse_number();
        throw std::runtime_error("Unexpected character: " + std::string(1, c));
    }

    JsonValue parse_object() {
        advance(); // {
        JsonObject obj;
        skip_whitespace();
        if (peek() == '}') { advance(); return JsonValue(obj); }
        
        while (true) {
            skip_whitespace();
            std::string key = parse_string();
            skip_whitespace();
            if (advance() != ':') throw std::runtime_error("Expected ':'");
            obj[key] = parse_value();
            skip_whitespace();
            char c = advance();
            if (c == ',') continue;
            if (c == '}') return JsonValue(obj);
            throw std::runtime_error("Expected ',' or '}'");
        }
    }

    JsonValue parse_array() {
        advance(); // [
        JsonArray arr;
        skip_whitespace();
        if (peek() == ']') { advance(); return JsonValue(arr); }
        
        while (true) {
            arr.push_back(parse_value());
            skip_whitespace();
            char c = advance();
            if (c == ',') continue;
            if (c == ']') return JsonValue(arr);
            throw std::runtime_error("Expected ',' or ']'");
        }
    }

    JsonValue parse_bool() {
        if (input.substr(pos, 4) == "true") { pos += 4; return JsonValue(true); }
        if (input.substr(pos, 5) == "false") { pos += 5; return JsonValue(false); }
        throw std::runtime_error("Invalid boolean");
    }

    JsonValue parse_null() {
        if (input.substr(pos, 4) == "null") { pos += 4; return JsonValue(); }
        throw std::runtime_error("Invalid null");
    }
};

} // namespace BrutalDDoS

#endif // MINI_JSON_PARSER_H
