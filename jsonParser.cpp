#include <iostream>
#include <vector>
#include <variant>
#include <map>
#include <fstream>
#include <sstream>

namespace json {
    struct Node;
    using Int = int64_t;
    using Null = std::monostate;
    using String = std::string;
    using Float = double;
    using Bool = bool;
    using Array = std::vector<Node>;
    using Object = std::map<std::string, Node>;
    using Value = std::variant<Int, Null, String, Bool, Float, Array, Object>;

    struct Node {
        Value value;

        Node() : value(Null{}) {}

        Node(Value _value) : value(_value) {}

        auto &operator[](size_t i) {
            if (auto array = std::get_if<Array>(&value)) {
                return (*array)[i];
            }
            throw std::runtime_error("index for not array!");
        }

        auto &operator[](const std::string &key) {
            if (auto obj = std::get_if<Object>(&value)) {
                return obj->at(key);
            }
            throw std::runtime_error("key for not object!");
        }

        void push(const Node &node) {
            if (auto k = std::get_if<Array>(&value)) {
                k->push_back(node);
            }
        }

    };

    struct JsonParser {
        std::string_view str;
        size_t pos = 0;

        auto parse_null() -> std::optional<Value> {
            if (str.substr(pos, 4) == "null") {
                pos += 4;
                return Null{};
            }
            return {};
        }

        auto parse_true() -> std::optional<Value> {
            if (str.substr(pos, 4) == "true") {
                pos += 4;
                return true;
            }
            return {};
        }

        auto parse_false() -> std::optional<Value> {
            if (str.substr(pos, 5) == "false") {
                pos += 5;
                return false;
            }
            return {};
        }

        void parse_whitespace() {
            while (std::isspace(str[pos])) {
                pos++;
            }
        }

        auto parse_numbers() -> std::optional<Value> {
            parse_whitespace();
            size_t endpos = pos;
            while (endpos < str.size() && (std::isdigit(str[endpos]) || str[endpos] == '.' || str[endpos] == 'e')) {
                endpos++;
            }
            std::string num = std::string(str.substr(pos, endpos - pos));
            pos = endpos;
            if (num.find('.') != num.npos || num.find('e') != num.npos) {
                try {
                    Float ret = std::stod(num);
                    return ret;
                }
                catch (...) {
                    return {};
                }
            } else {
                try {
                    Int ret = std::stoi(num);
                    return ret;
                }
                catch (...) {
                    return {};
                }
            }
        }

        auto parse_array() -> std::optional<Value> {
            if (str[pos] == '[') {
                pos++;
            }
            Array ret;
            while (pos < str.size() && str[pos] != ']') {
                parse_whitespace();
                auto tmp = parse_value();
                ret.push_back(tmp.value());
                while (pos < str.size() && str[pos] == ',') {
                    pos++;
                }
                parse_whitespace();
            }
            pos++;
            return ret;
        }

        auto parse_object() -> std::optional<Value> {
            if (str[pos] == '{') {
                pos++;
            }
            parse_whitespace();
            Object obj;

            while (pos < str.size() && str[pos] != '}') {
                auto key = parse_string();
                if (!std::holds_alternative<String>(key.value())) {
                    return {};
                }
                parse_whitespace();
                if (str[pos] == ':') {
                    pos++;
                }
                parse_whitespace();
                auto val = parse_value();

                obj[std::get<String>(key.value())] = val.value();

                parse_whitespace();
                while (pos < str.size() && str[pos] == ',') {
                    pos++;
                }
                parse_whitespace();

            }
            pos++;
            return obj;
        }

        auto parse_string() -> std::optional<Value> {
            if (str[pos] == '"') {
                pos++;
            }
            size_t endpos = pos;
            while (endpos < str.size() && str[endpos] != '"') {
                endpos++;
            }
            std::string ss = std::string(str.substr(pos, endpos - pos));
            pos = endpos + 1;
            return ss;
        }

        auto parse_value() -> std::optional<Value> {
            parse_whitespace();
            switch (str[pos]) {
                case 'n':
                    return parse_null();
                case 't':
                    return parse_true();
                case 'f':
                    return parse_false();
                case '[':
                    return parse_array();
                case '{':
                    return parse_object();
                case '"':
                    return parse_string();
                default:
                    return parse_numbers();
            }
        }

        auto parse() -> std::optional<Node> {
            parse_whitespace();
            auto val = parse_value();
            if (val) {
                return Node{*val};
            }
            return {};
        }
    };

    auto parser(std::string_view json_str) -> std::optional<Node> {
        JsonParser p = {json_str};
        return p.parse();
    }

    class JsonGenerator {
    public:
        static auto generate(const Node &node) -> std::string {
            return std::visit(
                    [](auto &&args) -> std::string {
                        using T = std::decay_t<decltype(args)>;
                        if constexpr (std::is_same_v<T, Null>) {
                            return "null";
                        } else if constexpr (std::is_same_v<T, Bool>) {
                            if (args) {
                                return "true";
                            }
                            return "false";
                        } else if constexpr (std::is_same_v<T, Int>) {
                            return std::to_string(args);
                        } else if constexpr (std::is_same_v<T, Float>) {
                            return std::to_string(args);
                        } else if constexpr (std::is_same_v<T, Array>) {
                            return generate_array(args);
                        } else if constexpr (std::is_same_v<T, Object>) {
                            return generate_object(args);
                        } else if constexpr (std::is_same_v<T, String>) {
                            return generate_string(args);
                        } else { return ""; }

                    }, node.value);
        }

        static auto generate_string(const String &str) -> std::string {
            return "\"" + str + "\"";
        }

        static auto generate_array(const Array &arr) -> std::string {
            std::string json_ret = "[";
            for (const auto &node: arr) {
                std::string ar = generate(node.value);
                json_ret += ar;
                json_ret += ",";
            }
            if (json_ret.size() > 1) json_ret.pop_back();
            json_ret += "]";
            return json_ret;

        }

        static auto generate_object(const Object &obj) -> std::string {
            std::string json_ret = "{";
            for (const auto &[key, value]: obj) {
                json_ret += generate_string(key);
                json_ret += ":";
                json_ret += generate(value);
                json_ret += ",";
            }
            if (json_ret.size() > 1) json_ret.pop_back();
            json_ret += "}";
            return json_ret;
        }

    };

    std::ostream &operator<<(std::ostream &ost, const Node &value) {
        ost << JsonGenerator::generate(value);
        return ost;
    }

}

using namespace json;

int main() {
    std::ifstream fin("..\\test_json.txt");
    std::stringstream ss;
    ss<<fin.rdbuf();
    std::string json_str=std::string(ss.str());

    auto json_node=parser(json_str).value();

    std::cout<<json_node["person"]<<'\n'<<'\n';
    std::get<Int>(json_node["person"]["age"].value)=99;
    std::cout<<json_node["person"]<<'\n'<<'\n';

    std::cout<<json_node<<'\n'<<'\n';
    std::string newitem="{\"type\":\"home2\",\"number\": \"555-5348\"}";

    auto nitem= parser(newitem).value();
    std::cout<<nitem<<'\n'<<'\n';

    std::get<Array>(json_node["person"]["contact_numbers"].value).push_back(nitem);
    std::cout<<json_node<<'\n'<<'\n';

    return 0;
}
