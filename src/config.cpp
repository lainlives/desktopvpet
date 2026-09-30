// config.cpp: recursive-descent JSON parser and typed accessors.
#include "config.hpp"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace dvp {

const Json *Json::find(std::string_view key) const {
    if (type != Type::Object) {
        return nullptr;
    }
    auto it = object.find(std::string(key));
    if (it == object.end()) {
        return nullptr;
    }
    return &it->second;
}

std::string Json::get_string(std::string_view key, const std::string &fallback) const {
    const Json *v = find(key);
    if (v && v->type == Type::String) {
        return v->string;
    }
    return fallback;
}

double Json::get_number(std::string_view key, double fallback) const {
    const Json *v = find(key);
    if (v && v->type == Type::Number) {
        return v->number;
    }
    return fallback;
}

int Json::get_int(std::string_view key, int fallback) const {
    const Json *v = find(key);
    if (v && v->type == Type::Number) {
        return static_cast<int>(v->number);
    }
    return fallback;
}

bool Json::get_bool(std::string_view key, bool fallback) const {
    const Json *v = find(key);
    if (v && v->type == Type::Bool) {
        return v->boolean;
    }
    return fallback;
}

namespace {

// Recursive-descent JSON parser. Any failure sets the error string and
// leaves the result as a Null value.
class Parser {
public:
    Parser(std::string_view text, std::string *error) : src_(text), error_(error) {}

    Json parse() {
        skip_ws();
        Json value = parse_value();
        skip_ws();
        if (!failed_ && pos_ != src_.size()) {
            fail("unexpected trailing characters");
        }
        return value;
    }

    bool failed() const { return failed_; }

private:
    void fail(const std::string &message) {
        if (!failed_) {
            failed_ = true;
            if (error_) {
                *error_ = message + " at byte " + std::to_string(pos_);
            }
        }
    }

    bool eof() const { return pos_ >= src_.size(); }
    char peek() const { return eof() ? '\0' : src_[pos_]; }

    char advance() { return src_[pos_++]; }

    void skip_ws() {
        while (!eof()) {
            char c = peek();
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                ++pos_;
            } else {
                break;
            }
        }
    }

    Json parse_value() {
        skip_ws();
        if (eof()) {
            fail("unexpected end of input");
            return {};
        }
        char c = peek();
        switch (c) {
            case '{': return parse_object();
            case '[': return parse_array();
            case '"': return parse_string_value();
            case 't':
            case 'f': return parse_bool();
            case 'n': return parse_null();
            default:  return parse_number();
        }
    }

    Json parse_object() {
        Json out;
        out.type = Json::Type::Object;
        advance();  // '{'
        skip_ws();
        if (peek() == '}') {
            advance();
            return out;
        }
        while (true) {
            skip_ws();
            if (peek() != '"') {
                fail("expected string key");
                return out;
            }
            std::string key = parse_string();
            skip_ws();
            if (peek() != ':') {
                fail("expected ':'");
                return out;
            }
            advance();
            out.object[key] = parse_value();
            skip_ws();
            if (peek() == ',') {
                advance();
                continue;
            }
            if (peek() == '}') {
                advance();
                return out;
            }
            fail("expected ',' or '}'");
            return out;
        }
    }

    Json parse_array() {
        Json out;
        out.type = Json::Type::Array;
        advance();  // '['
        skip_ws();
        if (peek() == ']') {
            advance();
            return out;
        }
        while (true) {
            out.array.push_back(parse_value());
            skip_ws();
            if (peek() == ',') {
                advance();
                continue;
            }
            if (peek() == ']') {
                advance();
                return out;
            }
            fail("expected ',' or ']'");
            return out;
        }
    }

    Json parse_string_value() {
        Json out;
        out.type = Json::Type::String;
        out.string = parse_string();
        return out;
    }

    // Decodes JSON escapes (including \uXXXX) into UTF-8.
    std::string parse_string() {
        std::string out;
        advance();  // opening quote
        while (!eof()) {
            char c = advance();
            if (c == '"') {
                return out;
            }
            if (c != '\\') {
                out.push_back(c);
                continue;
            }
            if (eof()) {
                fail("unterminated escape");
                return out;
            }
            char esc = advance();
            switch (esc) {
                case '"':  out.push_back('"');  break;
                case '\\': out.push_back('\\'); break;
                case '/':  out.push_back('/');  break;
                case 'b':  out.push_back('\b'); break;
                case 'f':  out.push_back('\f'); break;
                case 'n':  out.push_back('\n'); break;
                case 'r':  out.push_back('\r'); break;
                case 't':  out.push_back('\t'); break;
                case 'u': {
                    unsigned code = parse_hex4();
                    // Encode as UTF-8 (BMP only; no surrogate pairs).
                    if (code < 0x80) {
                        out.push_back(static_cast<char>(code));
                    } else if (code < 0x800) {
                        out.push_back(static_cast<char>(0xC0 | (code >> 6)));
                        out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
                    } else {
                        out.push_back(static_cast<char>(0xE0 | (code >> 12)));
                        out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
                        out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
                    }
                    break;
                }
                default:
                    fail("invalid escape");
                    return out;
            }
        }
        fail("unterminated string");
        return out;
    }

    unsigned parse_hex4() {
        unsigned value = 0;
        for (int i = 0; i < 4; ++i) {
            if (eof()) {
                fail("short \\u escape");
                return value;
            }
            char c = advance();
            value <<= 4;
            if (c >= '0' && c <= '9') {
                value |= static_cast<unsigned>(c - '0');
            } else if (c >= 'a' && c <= 'f') {
                value |= static_cast<unsigned>(c - 'a' + 10);
            } else if (c >= 'A' && c <= 'F') {
                value |= static_cast<unsigned>(c - 'A' + 10);
            } else {
                fail("invalid hex digit");
                return value;
            }
        }
        return value;
    }

    Json parse_bool() {
        Json out;
        out.type = Json::Type::Bool;
        if (src_.compare(pos_, 4, "true") == 0) {
            out.boolean = true;
            pos_ += 4;
        } else if (src_.compare(pos_, 5, "false") == 0) {
            out.boolean = false;
            pos_ += 5;
        } else {
            fail("invalid literal");
        }
        return out;
    }

    Json parse_null() {
        Json out;
        if (src_.compare(pos_, 4, "null") == 0) {
            pos_ += 4;
        } else {
            fail("invalid literal");
        }
        return out;
    }

    Json parse_number() {
        Json out;
        out.type = Json::Type::Number;
        std::size_t start = pos_;
        if (peek() == '-') {
            ++pos_;
        }
        while (!eof() && (std::isdigit(static_cast<unsigned char>(peek())) ||
                          peek() == '.' || peek() == 'e' || peek() == 'E' ||
                          peek() == '+' || peek() == '-')) {
            ++pos_;
        }
        if (start == pos_) {
            fail("invalid number");
            return out;
        }
        std::string token(src_.substr(start, pos_ - start));
        out.number = std::strtod(token.c_str(), nullptr);
        return out;
    }

    std::string_view src_;
    std::string *error_;
    std::size_t pos_ = 0;
    bool failed_ = false;
};

}  // namespace

Json parse_json(std::string_view text, std::string *error) {
    if (error) {
        error->clear();
    }
    Parser parser(text, error);
    Json value = parser.parse();
    if (parser.failed()) {
        return {};
    }
    return value;
}

Json load_json_file(const std::string &path, std::string *error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        if (error) {
            *error = "cannot open file: " + path;
        }
        return {};
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return parse_json(buffer.str(), error);
}

}  // namespace dvp
