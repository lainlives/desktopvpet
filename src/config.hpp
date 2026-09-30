// config.hpp: minimal dependency-free JSON reader used for character manifests.
#pragma once

#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace dvp {

// Minimal, dependency-free JSON value used for character manifests.
// Supports objects, arrays, strings, numbers, booleans and null.
class Json {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    Type type = Type::Null;
    bool boolean = false;
    double number = 0.0;
    std::string string;
    std::vector<Json> array;
    std::map<std::string, Json> object;

    bool is_null() const { return type == Type::Null; }
    bool is_object() const { return type == Type::Object; }
    bool is_array() const { return type == Type::Array; }
    bool is_string() const { return type == Type::String; }
    bool is_number() const { return type == Type::Number; }
    bool is_bool() const { return type == Type::Bool; }

    // Object access. Returns nullptr when missing or not an object.
    const Json *find(std::string_view key) const;
    bool has(std::string_view key) const { return find(key) != nullptr; }

    // Typed getters with defaults; never throw.
    std::string get_string(std::string_view key, const std::string &fallback = {}) const;
    double get_number(std::string_view key, double fallback = 0.0) const;
    int get_int(std::string_view key, int fallback = 0) const;
    bool get_bool(std::string_view key, bool fallback = false) const;
};

// Parses JSON text. On failure returns a Null value and fills `error`.
Json parse_json(std::string_view text, std::string *error = nullptr);

// Reads and parses a JSON file. On failure returns a Null value and fills `error`.
Json load_json_file(const std::string &path, std::string *error = nullptr);

}  // namespace dvp
