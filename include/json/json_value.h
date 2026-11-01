#pragma once
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

class JsonValue {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    static constexpr std::size_t kDefaultMaxDepth = 128;

    JsonValue();

    static JsonValue null();
    static JsonValue boolean(bool b);
    static JsonValue number(double d);
    static JsonValue string(std::string s);
    static JsonValue array();
    static JsonValue object();

    JsonValue(const JsonValue&);
    JsonValue& operator=(const JsonValue&);
    JsonValue(JsonValue&&) noexcept;
    JsonValue& operator=(JsonValue&&) noexcept;
    ~JsonValue();

    Type type() const noexcept;

    bool               as_bool()   const;
    double             as_number() const;
    const std::string& as_string() const;

    std::size_t size() const;

    const JsonValue& operator[](std::size_t i) const;

    const std::string& key_at(std::size_t i) const;

    const JsonValue* find(std::string_view key) const;

    void push_back(JsonValue v);

    void set(std::string key, JsonValue v);

    static JsonValue parse(std::string_view text, std::string* error_out,
                           std::size_t max_depth = kDefaultMaxDepth);

    std::string dump() const;

    bool operator==(const JsonValue& other) const;
    bool operator!=(const JsonValue& other) const { return !(*this == other); }

private:
    Type                     type_ = Type::Null;
    bool                     bool_ = false;
    double                   number_ = 0.0;
    std::string              string_;
    std::vector<std::string> keys_;
    std::vector<JsonValue>   items_;
};
