#pragma once
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>
namespace julretsu {
// Minimal bounded JSON for AI provider requests and responses. Parsing throws std::runtime_error.
struct Json;
using JsonArray = std::vector<Json>;
using JsonObject = std::vector<std::pair<std::string, Json>>;
struct Json {
    std::variant<std::nullptr_t, bool, double, std::string, JsonArray, JsonObject> value;
    Json() : value(nullptr) {}
    Json(std::nullptr_t) : value(nullptr) {}
    Json(bool b) : value(b) {}
    Json(double n) : value(n) {}
    Json(int n) : value(double(n)) {}
    Json(const char* s) : value(std::string(s)) {}
    Json(std::string s) : value(std::move(s)) {}
    Json(JsonArray a) : value(std::move(a)) {}
    Json(JsonObject o) : value(std::move(o)) {}
    [[nodiscard]] const Json* find(std::string_view key) const;
    [[nodiscard]] const std::string* string() const { return std::get_if<std::string>(&value); }
    [[nodiscard]] const JsonArray* array() const { return std::get_if<JsonArray>(&value); }
    [[nodiscard]] const JsonObject* object() const { return std::get_if<JsonObject>(&value); }
};
[[nodiscard]] Json parse_json(std::string_view text, std::size_t max_depth = 64);
[[nodiscard]] std::string dump_json(const Json& value);
}
