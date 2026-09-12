#pragma once

#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace dap {

class json;
using json_array  = std::vector<json>;
using json_object = std::map<std::string, json>;

class json {
public:
    enum class Kind { Null, Bool, Number, String, Array, Object };

    json()                         : kind_(Kind::Null) {}
    json(std::nullptr_t)           : kind_(Kind::Null) {}
    json(bool v)                   : kind_(Kind::Bool),   b_(v) {}
    json(int v)                    : kind_(Kind::Number), n_(v) {}
    json(double v)                 : kind_(Kind::Number), n_(v) {}
    json(const char* v)            : kind_(Kind::String), s_(v) {}
    json(const std::string& v)     : kind_(Kind::String), s_(v) {}
    json(std::string&& v)          : kind_(Kind::String), s_(std::move(v)) {}
    json(const json_array& v)      : kind_(Kind::Array),  a_(std::make_shared<json_array>(v)) {}
    json(json_array&& v)           : kind_(Kind::Array),  a_(std::make_shared<json_array>(std::move(v))) {}
    json(const json_object& v)     : kind_(Kind::Object), o_(std::make_shared<json_object>(v)) {}
    json(json_object&& v)          : kind_(Kind::Object), o_(std::make_shared<json_object>(std::move(v))) {}

    json(std::initializer_list<std::pair<const std::string, json>> fields)
        : kind_(Kind::Object), o_(std::make_shared<json_object>(fields.begin(), fields.end())) {}

    static json array(std::initializer_list<json> items) {
        json j;
        j.kind_ = Kind::Array;
        j.a_ = std::make_shared<json_array>(items.begin(), items.end());
        return j;
    }

    void push_back(json val) {
        if (!is_array()) { kind_ = Kind::Array; a_ = std::make_shared<json_array>(); }
        a_->push_back(std::move(val));
    }

    static json object() { return json(json_object{}); }
    static json array()  { return json(json_array{}); }

    bool is_null()   const { return kind_ == Kind::Null; }
    bool is_bool()   const { return kind_ == Kind::Bool; }
    bool is_number() const { return kind_ == Kind::Number; }
    bool is_string() const { return kind_ == Kind::String; }
    bool is_array()  const { return kind_ == Kind::Array; }
    bool is_object() const { return kind_ == Kind::Object; }

    bool contains(const std::string& key) const {
        return is_object() && o_->count(key) > 0;
    }

    bool        get_bool()   const { return b_; }
    double      get_number() const { return n_; }
    int         get_int()    const { return static_cast<int>(n_); }
    const std::string& get_string() const { return s_; }

    template<typename T>
    T value(const std::string& key, T def) const {
        if (!is_object()) return def;
        auto it = o_->find(key);
        if (it == o_->end()) return def;
        return _coerce<T>(it->second, def);
    }

    json& operator[](const std::string& key) {
        if (!is_object()) { kind_ = Kind::Object; o_ = std::make_shared<json_object>(); }
        return (*o_)[key];
    }
    const json& operator[](const std::string& key) const {
        static json null_json;
        if (!is_object()) return null_json;
        auto it = o_->find(key);
        return it == o_->end() ? null_json : it->second;
    }
    json& operator[](size_t i) {
        return (*a_)[i];
    }
    const json& operator[](size_t i) const {
        static json null_json;
        if (!is_array() || i >= a_->size()) return null_json;
        return (*a_)[i];
    }

    auto begin() const { return a_ ? a_->begin() : json_array{}.begin(); }
    auto end()   const { return a_ ? a_->end()   : json_array{}.end();   }
    size_t size() const {
        if (is_array())  return a_ ? a_->size() : 0;
        if (is_object()) return o_ ? o_->size() : 0;
        return 0;
    }

    std::string dump() const {
        std::ostringstream oss;
        _dump(oss);
        return oss.str();
    }

    static json parse(const std::string& s) {
        size_t pos = 0;
        return _parse(s, pos);
    }

private:
    Kind kind_ = Kind::Null;
    bool b_ = false;
    double n_ = 0;
    std::string s_;
    std::shared_ptr<json_array>  a_;
    std::shared_ptr<json_object> o_;

    template<typename T>
    static T _coerce(const json& j, T def) { (void)j; return def; }

    void _dump(std::ostringstream& oss) const {
        switch (kind_) {
        case Kind::Null:   oss << "null"; break;
        case Kind::Bool:   oss << (b_ ? "true" : "false"); break;
        case Kind::Number: {
            if (n_ == static_cast<long long>(n_))
                oss << static_cast<long long>(n_);
            else
                oss << n_;
            break;
        }
        case Kind::String: oss << '"' << _escape(s_) << '"'; break;
        case Kind::Array: {
            oss << '[';
            if (a_) {
                for (size_t i = 0; i < a_->size(); ++i) {
                    if (i) oss << ',';
                    (*a_)[i]._dump(oss);
                }
            }
            oss << ']';
            break;
        }
        case Kind::Object: {
            oss << '{';
            if (o_) {
                bool first = true;
                for (const auto& [k, v] : *o_) {
                    if (!first) oss << ',';
                    first = false;
                    oss << '"' << _escape(k) << '"' << ':';
                    v._dump(oss);
                }
            }
            oss << '}';
            break;
        }
        }
    }

    static std::string _escape(const std::string& s) {
        std::string out;
        out.reserve(s.size());
        for (char c : s) {
            switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;
            }
        }
        return out;
    }

    static void _skip(const std::string& s, size_t& pos) {
        while (pos < s.size() && std::isspace((unsigned char)s[pos])) ++pos;
    }

    static json _parse(const std::string& s, size_t& pos) {
        _skip(s, pos);
        if (pos >= s.size()) return {};
        char c = s[pos];
        if (c == '"') return _parse_string(s, pos);
        if (c == '{') return _parse_object(s, pos);
        if (c == '[') return _parse_array(s, pos);
        if (c == 't') { pos += 4; return json(true); }
        if (c == 'f') { pos += 5; return json(false); }
        if (c == 'n') { pos += 4; return {}; }
        if (c == '-' || std::isdigit((unsigned char)c)) return _parse_number(s, pos);
        return {};
    }

    static std::string _parse_string(const std::string& s, size_t& pos) {
        ++pos;
        std::string out;
        while (pos < s.size() && s[pos] != '"') {
            if (s[pos] == '\\' && pos + 1 < s.size()) {
                ++pos;
                switch (s[pos]) {
                case '"':  out += '"';  break;
                case '\\': out += '\\'; break;
                case 'n':  out += '\n'; break;
                case 'r':  out += '\r'; break;
                case 't':  out += '\t'; break;
                default:   out += s[pos];
                }
            } else {
                out += s[pos];
            }
            ++pos;
        }
        if (pos < s.size()) ++pos;
        return out;
    }

    static json _parse_number(const std::string& s, size_t& pos) {
        size_t start = pos;
        if (s[pos] == '-') ++pos;
        while (pos < s.size() && (std::isdigit((unsigned char)s[pos]) || s[pos] == '.' || s[pos] == 'e' || s[pos] == 'E' || s[pos] == '+' || s[pos] == '-')) ++pos;
        return json(std::stod(s.substr(start, pos - start)));
    }

    static json _parse_object(const std::string& s, size_t& pos) {
        ++pos;
        json_object obj;
        _skip(s, pos);
        while (pos < s.size() && s[pos] != '}') {
            _skip(s, pos);
            if (s[pos] != '"') break;
            std::string key = _parse_string(s, pos);
            _skip(s, pos);
            if (pos < s.size() && s[pos] == ':') ++pos;
            _skip(s, pos);
            obj[key] = _parse(s, pos);
            _skip(s, pos);
            if (pos < s.size() && s[pos] == ',') ++pos;
            _skip(s, pos);
        }
        if (pos < s.size()) ++pos;
        return json(std::move(obj));
    }

    static json _parse_array(const std::string& s, size_t& pos) {
        ++pos;
        json_array arr;
        _skip(s, pos);
        while (pos < s.size() && s[pos] != ']') {
            arr.push_back(_parse(s, pos));
            _skip(s, pos);
            if (pos < s.size() && s[pos] == ',') ++pos;
            _skip(s, pos);
        }
        if (pos < s.size()) ++pos;
        return json(std::move(arr));
    }
};

template<> inline std::string json::_coerce(const json& j, std::string def) {
    return j.is_string() ? j.get_string() : def;
}
template<> inline int json::_coerce(const json& j, int def) {
    return j.is_number() ? j.get_int() : def;
}
template<> inline bool json::_coerce(const json& j, bool def) {
    return j.is_bool() ? j.get_bool() : def;
}

}

using json = dap::json;

