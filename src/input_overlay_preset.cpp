#include "input_overlay_preset.hpp"

#include <graphics/image-file.h>
#include <obs-module.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <string>
#include <string_view>
#include <utility>

namespace mugen {
namespace {

constexpr int kPressedTextureGap = 3;
constexpr uint32_t kMaxDimension = 8192;
constexpr uint64_t kMaxPixelBytes = 256ULL * 1024ULL * 1024ULL;
constexpr uint64_t kMaxCanvasBytes = 64ULL * 1024ULL * 1024ULL;
constexpr uint64_t kMaxJsonBytes = 8ULL * 1024ULL * 1024ULL;

struct JsonValue {
    enum class Type { Null, Boolean, Number, String, Array, Object };

    Type type = Type::Null;
    bool boolean = false;
    double number = 0.0;
    std::string string;
    std::vector<JsonValue> array;
    std::map<std::string, JsonValue, std::less<>> object;

    const JsonValue *find(std::string_view key) const
    {
        if (type != Type::Object)
            return nullptr;
        const auto it = object.find(key);
        return it == object.end() ? nullptr : &it->second;
    }

    int integer(int fallback = 0) const
    {
        if (type != Type::Number || !std::isfinite(number))
            return fallback;
        const double clamped = std::clamp(number,
                                          static_cast<double>(std::numeric_limits<int>::min()),
                                          static_cast<double>(std::numeric_limits<int>::max()));
        return static_cast<int>(std::lround(clamped));
    }

    bool bool_value(bool fallback = false) const
    {
        if (type == Type::Boolean)
            return boolean;
        if (type == Type::Number && std::isfinite(number))
            return number != 0.0;
        return fallback;
    }
};

class JsonParser {
public:
    explicit JsonParser(std::string_view input) : input_(input) {}

    bool parse(JsonValue &out, std::string &error)
    {
        skip_space();
        if (!parse_value(out)) {
            error = error_.empty() ? "Invalid JSON" : error_;
            return false;
        }
        skip_space();
        if (position_ != input_.size()) {
            set_error("Unexpected data after the JSON document");
            error = error_;
            return false;
        }
        return true;
    }

private:
    void skip_space()
    {
        while (position_ < input_.size()) {
            const char c = input_[position_];
            if (c != ' ' && c != '\t' && c != '\r' && c != '\n')
                break;
            ++position_;
        }
    }

    void set_error(std::string_view message)
    {
        if (!error_.empty())
            return;
        error_ = std::string(message) + " at byte " + std::to_string(position_);
    }

    bool consume(char expected)
    {
        if (position_ >= input_.size() || input_[position_] != expected)
            return false;
        ++position_;
        return true;
    }

    bool consume_literal(std::string_view literal)
    {
        if (input_.substr(position_, literal.size()) != literal)
            return false;
        position_ += literal.size();
        return true;
    }

    bool parse_value(JsonValue &out)
    {
        skip_space();
        if (position_ >= input_.size()) {
            set_error("Unexpected end of JSON");
            return false;
        }

        switch (input_[position_]) {
        case '{': return parse_object(out);
        case '[': return parse_array(out);
        case '"':
            out.type = JsonValue::Type::String;
            return parse_string(out.string);
        case 't':
            if (consume_literal("true")) {
                out.type = JsonValue::Type::Boolean;
                out.boolean = true;
                return true;
            }
            break;
        case 'f':
            if (consume_literal("false")) {
                out.type = JsonValue::Type::Boolean;
                out.boolean = false;
                return true;
            }
            break;
        case 'n':
            if (consume_literal("null")) {
                out.type = JsonValue::Type::Null;
                return true;
            }
            break;
        default:
            if (input_[position_] == '-' || (input_[position_] >= '0' && input_[position_] <= '9'))
                return parse_number(out);
            break;
        }

        set_error("Unexpected JSON token");
        return false;
    }

    bool parse_object(JsonValue &out)
    {
        if (!consume('{'))
            return false;
        out.type = JsonValue::Type::Object;
        skip_space();
        if (consume('}'))
            return true;

        for (;;) {
            skip_space();
            if (position_ >= input_.size() || input_[position_] != '"') {
                set_error("Expected an object key");
                return false;
            }

            std::string key;
            if (!parse_string(key))
                return false;
            skip_space();
            if (!consume(':')) {
                set_error("Expected ':' after an object key");
                return false;
            }

            JsonValue value;
            if (!parse_value(value))
                return false;
            out.object.insert_or_assign(std::move(key), std::move(value));

            skip_space();
            if (consume('}'))
                return true;
            if (!consume(',')) {
                set_error("Expected ',' or '}' in an object");
                return false;
            }
        }
    }

    bool parse_array(JsonValue &out)
    {
        if (!consume('['))
            return false;
        out.type = JsonValue::Type::Array;
        skip_space();
        if (consume(']'))
            return true;

        for (;;) {
            JsonValue value;
            if (!parse_value(value))
                return false;
            out.array.push_back(std::move(value));

            skip_space();
            if (consume(']'))
                return true;
            if (!consume(',')) {
                set_error("Expected ',' or ']' in an array");
                return false;
            }
        }
    }

    static bool hex_digit(char c, uint32_t &value)
    {
        if (c >= '0' && c <= '9') {
            value = static_cast<uint32_t>(c - '0');
            return true;
        }
        if (c >= 'a' && c <= 'f') {
            value = static_cast<uint32_t>(c - 'a' + 10);
            return true;
        }
        if (c >= 'A' && c <= 'F') {
            value = static_cast<uint32_t>(c - 'A' + 10);
            return true;
        }
        return false;
    }

    static void append_utf8(std::string &out, uint32_t codepoint)
    {
        if (codepoint <= 0x7FU) {
            out.push_back(static_cast<char>(codepoint));
        } else if (codepoint <= 0x7FFU) {
            out.push_back(static_cast<char>(0xC0U | (codepoint >> 6U)));
            out.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
        } else {
            out.push_back(static_cast<char>(0xE0U | (codepoint >> 12U)));
            out.push_back(static_cast<char>(0x80U | ((codepoint >> 6U) & 0x3FU)));
            out.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
        }
    }

    bool parse_string(std::string &out)
    {
        if (!consume('"'))
            return false;
        out.clear();

        while (position_ < input_.size()) {
            const char c = input_[position_++];
            if (c == '"')
                return true;
            if (static_cast<unsigned char>(c) < 0x20U) {
                set_error("Control character in a JSON string");
                return false;
            }
            if (c != '\\') {
                out.push_back(c);
                continue;
            }

            if (position_ >= input_.size()) {
                set_error("Incomplete escape sequence");
                return false;
            }
            const char escaped = input_[position_++];
            switch (escaped) {
            case '"': out.push_back('"'); break;
            case '\\': out.push_back('\\'); break;
            case '/': out.push_back('/'); break;
            case 'b': out.push_back('\b'); break;
            case 'f': out.push_back('\f'); break;
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            case 't': out.push_back('\t'); break;
            case 'u': {
                if (position_ + 4U > input_.size()) {
                    set_error("Incomplete Unicode escape");
                    return false;
                }
                uint32_t codepoint = 0;
                for (int i = 0; i < 4; ++i) {
                    uint32_t digit = 0;
                    if (!hex_digit(input_[position_++], digit)) {
                        set_error("Invalid Unicode escape");
                        return false;
                    }
                    codepoint = (codepoint << 4U) | digit;
                }
                // Preset keys are ASCII. Keep non-surrogate BMP text valid for metadata.
                if (codepoint >= 0xD800U && codepoint <= 0xDFFFU)
                    append_utf8(out, 0xFFFDU);
                else
                    append_utf8(out, codepoint);
                break;
            }
            default:
                set_error("Invalid escape sequence");
                return false;
            }
        }

        set_error("Unterminated JSON string");
        return false;
    }

    bool parse_number(JsonValue &out)
    {
        const size_t start = position_;
        if (input_[position_] == '-')
            ++position_;

        if (position_ >= input_.size()) {
            set_error("Incomplete JSON number");
            return false;
        }
        if (input_[position_] == '0') {
            ++position_;
        } else {
            if (input_[position_] < '1' || input_[position_] > '9') {
                set_error("Invalid JSON number");
                return false;
            }
            while (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9')
                ++position_;
        }

        if (position_ < input_.size() && input_[position_] == '.') {
            ++position_;
            const size_t fraction_start = position_;
            while (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9')
                ++position_;
            if (position_ == fraction_start) {
                set_error("Invalid fractional JSON number");
                return false;
            }
        }

        if (position_ < input_.size() && (input_[position_] == 'e' || input_[position_] == 'E')) {
            ++position_;
            if (position_ < input_.size() && (input_[position_] == '+' || input_[position_] == '-'))
                ++position_;
            const size_t exponent_start = position_;
            while (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9')
                ++position_;
            if (position_ == exponent_start) {
                set_error("Invalid JSON exponent");
                return false;
            }
        }

        const std::string token(input_.substr(start, position_ - start));
        char *end = nullptr;
        const double value = std::strtod(token.c_str(), &end);
        if (!end || *end != '\0' || !std::isfinite(value)) {
            set_error("Invalid JSON number");
            return false;
        }
        out.type = JsonValue::Type::Number;
        out.number = value;
        return true;
    }

    std::string_view input_;
    size_t position_ = 0;
    std::string error_;
};

std::filesystem::path utf8_to_path(const std::string &value)
{
#if defined(__cpp_char8_t)
    const auto *begin = reinterpret_cast<const char8_t *>(value.data());
    return std::filesystem::path(std::u8string(begin, begin + value.size()));
#else
    return std::filesystem::path(value);
#endif
}

bool load_json_file(const std::string &path, JsonValue &root, std::string &error)
{
    std::ifstream stream(utf8_to_path(path), std::ios::binary);
    if (!stream) {
        error = "Input Overlay JSON could not be opened";
        return false;
    }
    stream.seekg(0, std::ios::end);
    const std::streamoff size = stream.tellg();
    if (size < 0 || static_cast<uint64_t>(size) > kMaxJsonBytes) {
        error = "Input Overlay JSON is too large";
        return false;
    }
    stream.seekg(0, std::ios::beg);
    std::string text(static_cast<size_t>(size), '\0');
    if (size > 0)
        stream.read(text.data(), static_cast<std::streamsize>(size));
    if (!stream && size > 0) {
        error = "Input Overlay JSON could not be read";
        return false;
    }

    JsonParser parser(text);
    if (!parser.parse(root, error))
        return false;
    if (root.type != JsonValue::Type::Object) {
        error = "Input Overlay JSON root is not an object";
        return false;
    }
    return true;
}

int object_int(const JsonValue &object, std::string_view key, int fallback = 0)
{
    const JsonValue *value = object.find(key);
    return value ? value->integer(fallback) : fallback;
}

bool object_bool(const JsonValue &object, std::string_view key, bool fallback = false)
{
    const JsonValue *value = object.find(key);
    return value ? value->bool_value(fallback) : fallback;
}

bool numeric_array(const JsonValue &object, std::string_view key,
                   std::array<int, 4> &values, size_t required)
{
    const JsonValue *array = object.find(key);
    if (!array || array->type != JsonValue::Type::Array || array->array.size() < required)
        return false;

    values.fill(0);
    const size_t limit = std::min(values.size(), array->array.size());
    for (size_t i = 0; i < limit; ++i) {
        if (array->array[i].type != JsonValue::Type::Number)
            return false;
        values[i] = array->array[i].integer();
    }
    return true;
}

InputOverlayElementType parse_type(int value)
{
    switch (value) {
    case 0: return InputOverlayElementType::StaticTexture;
    case 2: return InputOverlayElementType::Button;
    case 5: return InputOverlayElementType::AnalogStick;
    case 6: return InputOverlayElementType::Trigger;
    case 7: return InputOverlayElementType::GamepadId;
    case 8: return InputOverlayElementType::DpadStick;
    default: return InputOverlayElementType::Unsupported;
    }
}

Action action_from_input_overlay_code(int code)
{
    switch (code) {
    case 0: return Action::South;
    case 1: return Action::East;
    case 2: return Action::West;
    case 3: return Action::North;
    case 4: return Action::Back;
    case 5: return Action::Guide;
    case 6: return Action::Start;
    case 7: return Action::LeftStick;
    case 8: return Action::RightStick;
    case 9: return Action::LeftShoulder;
    case 10: return Action::RightShoulder;
    case 11: return Action::DpadUp;
    case 12: return Action::DpadDown;
    case 13: return Action::DpadLeft;
    case 14: return Action::DpadRight;
    case 15: return Action::Misc1;
    case 20: return Action::Touchpad;
    default: return Action::Count;
    }
}

bool action_pressed(const GamepadState &state, Action action)
{
    return action != Action::Count && state.pressed[static_cast<size_t>(action)];
}

std::string path_to_utf8(const std::filesystem::path &path)
{
#if defined(__cpp_char8_t)
    const std::u8string value = path.u8string();
    return std::string(reinterpret_cast<const char *>(value.data()), value.size());
#else
    return path.u8string();
#endif
}

std::string resolve_image_path(const std::string &config_path, const std::string &image_path)
{
    namespace fs = std::filesystem;
    std::error_code ec;

    if (!image_path.empty() && fs::is_regular_file(utf8_to_path(image_path), ec))
        return image_path;

    if (config_path.empty())
        return {};

    const fs::path config = utf8_to_path(config_path);
    fs::path candidate = config;
    candidate.replace_extension(".png");
    if (fs::is_regular_file(candidate, ec))
        return path_to_utf8(candidate);

    candidate = config;
    candidate.replace_extension(".PNG");
    if (fs::is_regular_file(candidate, ec))
        return path_to_utf8(candidate);

    return {};
}

bool load_png_rgba(const std::string &path, InputOverlayPreset &preset, std::string &error)
{
    gs_image_file_t image{};
    gs_image_file_init(&image, path.c_str());
    if (!image.loaded || !image.texture_data || image.cx == 0 || image.cy == 0) {
        obs_enter_graphics();
        gs_image_file_free(&image);
        obs_leave_graphics();
        error = "The PNG atlas could not be decoded";
        return false;
    }

    if (image.cx > kMaxDimension || image.cy > kMaxDimension ||
        static_cast<uint64_t>(image.cx) * image.cy * 4ULL > kMaxPixelBytes) {
        obs_enter_graphics();
        gs_image_file_free(&image);
        obs_leave_graphics();
        error = "The PNG atlas is too large";
        return false;
    }

    preset.atlas_width = image.cx;
    preset.atlas_height = image.cy;
    preset.atlas_rgba.resize(static_cast<size_t>(image.cx) * image.cy * 4U);

    const size_t pixel_count = static_cast<size_t>(image.cx) * image.cy;
    if (image.format == GS_RGBA) {
        std::copy_n(image.texture_data, pixel_count * 4U, preset.atlas_rgba.data());
    } else if (image.format == GS_BGRA) {
        for (size_t i = 0; i < pixel_count; ++i) {
            preset.atlas_rgba[i * 4U + 0] = image.texture_data[i * 4U + 2];
            preset.atlas_rgba[i * 4U + 1] = image.texture_data[i * 4U + 1];
            preset.atlas_rgba[i * 4U + 2] = image.texture_data[i * 4U + 0];
            preset.atlas_rgba[i * 4U + 3] = image.texture_data[i * 4U + 3];
        }
    } else {
        obs_enter_graphics();
        gs_image_file_free(&image);
        obs_leave_graphics();
        error = "The PNG atlas decoded to an unsupported pixel format";
        preset.atlas_rgba.clear();
        return false;
    }

    obs_enter_graphics();
    gs_image_file_free(&image);
    obs_leave_graphics();
    return true;
}

bool rect_intersects_atlas(const InputOverlayPreset &preset, const InputOverlayElement &element)
{
    if (element.width <= 0 || element.height <= 0)
        return false;
    const int64_t left = element.source_x;
    const int64_t top = element.source_y;
    const int64_t right = left + element.width;
    const int64_t bottom = top + element.height;
    return right > 0 && bottom > 0 && left < static_cast<int64_t>(preset.atlas_width) &&
           top < static_cast<int64_t>(preset.atlas_height);
}

bool rect_fully_inside_atlas(const InputOverlayPreset &preset, const InputOverlayElement &element)
{
    if (element.source_x < 0 || element.source_y < 0 || element.width <= 0 || element.height <= 0)
        return false;
    const int64_t right = static_cast<int64_t>(element.source_x) + element.width;
    const int64_t bottom = static_cast<int64_t>(element.source_y) + element.height;
    return right <= static_cast<int64_t>(preset.atlas_width) &&
           bottom <= static_cast<int64_t>(preset.atlas_height);
}

void blit_element(PixelCanvas &canvas, const InputOverlayPreset &preset,
                  const InputOverlayElement &element, int source_y, int dest_x, int dest_y,
                  int source_x_offset = 0, int source_y_offset = 0,
                  int width_override = -1, int height_override = -1)
{
    const int width = width_override >= 0 ? width_override : element.width;
    const int height = height_override >= 0 ? height_override : element.height;
    canvas.blit_rgba(preset.atlas_rgba.data(), preset.atlas_width, preset.atlas_height,
                     element.source_x + source_x_offset, source_y + source_y_offset,
                     width, height, dest_x, dest_y);
}

void draw_trigger(PixelCanvas &canvas, const InputOverlayPreset &preset,
                  const InputOverlayElement &element, float value)
{
    value = std::clamp(value, 0.0F, 1.0F);
    blit_element(canvas, preset, element, element.source_y, element.dest_x, element.dest_y);

    const int active_y = element.source_y + element.height + kPressedTextureGap;
    if (value <= 0.0F || active_y + element.height > static_cast<int>(preset.atlas_height))
        return;

    if (element.trigger_mode) {
        if (value >= 0.5F)
            blit_element(canvas, preset, element, active_y, element.dest_x, element.dest_y);
        return;
    }

    // Input Overlay directions: 1 up, 2 down, 3 left, 4 right.
    switch (element.direction) {
    case 1: {
        const int amount = static_cast<int>(std::round(element.height * value));
        if (amount > 0) {
            const int offset = element.height - amount;
            blit_element(canvas, preset, element, active_y, element.dest_x, element.dest_y + offset,
                         0, offset, element.width, amount);
        }
        break;
    }
    case 2: {
        const int amount = static_cast<int>(std::round(element.height * value));
        if (amount > 0)
            blit_element(canvas, preset, element, active_y, element.dest_x, element.dest_y,
                         0, 0, element.width, amount);
        break;
    }
    case 3: {
        const int amount = static_cast<int>(std::round(element.width * value));
        if (amount > 0) {
            const int offset = element.width - amount;
            blit_element(canvas, preset, element, active_y, element.dest_x + offset, element.dest_y,
                         offset, 0, amount, element.height);
        }
        break;
    }
    case 4:
    default: {
        const int amount = static_cast<int>(std::round(element.width * value));
        if (amount > 0)
            blit_element(canvas, preset, element, active_y, element.dest_x, element.dest_y,
                         0, 0, amount, element.height);
        break;
    }
    }
}

int dpad_stick_offset(const GamepadState &state)
{
    const bool up = action_pressed(state, Action::DpadUp);
    const bool down = action_pressed(state, Action::DpadDown);
    const bool left = action_pressed(state, Action::DpadLeft);
    const bool right = action_pressed(state, Action::DpadRight);

    if (up && left)
        return 5;
    if (up && right)
        return 6;
    if (down && left)
        return 7;
    if (down && right)
        return 8;
    if (left)
        return 1;
    if (right)
        return 2;
    if (up)
        return 3;
    if (down)
        return 4;
    return 0;
}

bool source_rect_inside(const InputOverlayPreset &preset, int x, int y, int width, int height)
{
    if (x < 0 || y < 0 || width <= 0 || height <= 0)
        return false;
    return static_cast<uint64_t>(x + width) <= preset.atlas_width &&
           static_cast<uint64_t>(y + height) <= preset.atlas_height;
}


} // namespace

bool load_input_overlay_preset(const std::string &config_path, const std::string &image_path,
                               InputOverlayPreset &out, std::string &error)
{
    if (config_path.empty()) {
        error = "Input Overlay config path is empty";
        return false;
    }

    JsonValue root;
    if (!load_json_file(config_path, root, error))
        return false;

    const int raw_width = object_int(root, "overlay_width", 0);
    const int raw_height = object_int(root, "overlay_height", 0);
    const JsonValue *elements = root.find("elements");
    if (raw_width <= 0 || raw_height <= 0 || !elements || elements->type != JsonValue::Type::Array) {
        error = "This is not a supported Input Overlay 5.x JSON preset";
        return false;
    }
    if (raw_width > static_cast<int>(kMaxDimension) || raw_height > static_cast<int>(kMaxDimension) ||
        static_cast<uint64_t>(raw_width) * static_cast<uint64_t>(raw_height) * 4ULL > kMaxCanvasBytes) {
        error = "The Input Overlay canvas is too large";
        return false;
    }

    InputOverlayPreset parsed;
    parsed.config_path = config_path;
    parsed.width = static_cast<uint32_t>(raw_width);
    parsed.height = static_cast<uint32_t>(raw_height);
    parsed.elements.reserve(elements->array.size());

    for (const JsonValue &item : elements->array) {
        if (item.type != JsonValue::Type::Object) {
            ++parsed.unsupported_elements;
            continue;
        }

        std::array<int, 4> mapping{};
        std::array<int, 4> position{};
        InputOverlayElement element;
        element.type = parse_type(object_int(item, "type", -1));
        element.z_level = object_int(item, "z_level", 0);
        element.code = object_int(item, "code", -1);
        element.side = object_int(item, "side", 0);
        element.direction = object_int(item, "direction", 0);
        element.stick_radius = std::max(0, object_int(item, "stick_radius", 0));
        element.trigger_mode = object_bool(item, "trigger_mode", false);

        const bool mapping_ok = numeric_array(item, "mapping", mapping, 4);
        const bool position_ok = numeric_array(item, "pos", position, 2);
        if (mapping_ok && position_ok) {
            element.source_x = mapping[0];
            element.source_y = mapping[1];
            element.width = mapping[2];
            element.height = mapping[3];
            element.dest_x = position[0];
            element.dest_y = position[1];
        }

        if (mapping_ok && position_ok && element.type != InputOverlayElementType::Unsupported) {
            parsed.elements.push_back(element);
            ++parsed.supported_elements;
        } else {
            ++parsed.unsupported_elements;
        }
    }

    if (parsed.elements.empty()) {
        error = "The preset contains no supported gamepad elements";
        return false;
    }

    parsed.image_path = resolve_image_path(config_path, image_path);
    if (parsed.image_path.empty()) {
        error = "Select the PNG texture atlas used by this Input Overlay preset";
        return false;
    }

    if (!load_png_rgba(parsed.image_path, parsed, error))
        return false;

    parsed.elements.erase(std::remove_if(parsed.elements.begin(), parsed.elements.end(),
                                         [&](const InputOverlayElement &element) {
                                             if (!rect_intersects_atlas(parsed, element)) {
                                                 ++parsed.unsupported_elements;
                                                 if (parsed.supported_elements > 0)
                                                     --parsed.supported_elements;
                                                 return true;
                                             }
                                             if (!rect_fully_inside_atlas(parsed, element))
                                                 ++parsed.clipped_elements;
                                             return false;
                                         }),
                          parsed.elements.end());

    std::stable_sort(parsed.elements.begin(), parsed.elements.end(),
                     [](const InputOverlayElement &a, const InputOverlayElement &b) {
                         return a.z_level < b.z_level;
                     });

    if (parsed.elements.empty()) {
        error = "All preset elements point outside the PNG texture atlas";
        return false;
    }

    out = std::move(parsed);
    error.clear();
    return true;
}

void draw_input_overlay_preset(PixelCanvas &canvas, const InputOverlayPreset &preset,
                               const GamepadState &state)
{
    canvas.clear();
    if (preset.atlas_rgba.empty())
        return;

    for (const InputOverlayElement &element : preset.elements) {
        switch (element.type) {
        case InputOverlayElementType::StaticTexture:
            blit_element(canvas, preset, element, element.source_y, element.dest_x, element.dest_y);
            break;
        case InputOverlayElementType::Button: {
            const Action action = action_from_input_overlay_code(element.code);
            const bool pressed = action_pressed(state, action);
            const int active_y = element.source_y + element.height + kPressedTextureGap;
            const int source_y = pressed && active_y + element.height <= static_cast<int>(preset.atlas_height)
                                     ? active_y
                                     : element.source_y;
            blit_element(canvas, preset, element, source_y, element.dest_x, element.dest_y);
            break;
        }
        case InputOverlayElementType::AnalogStick: {
            const bool left_side = element.side == 0;
            const float axis_x = left_side ? state.left_x : state.right_x;
            const float axis_y = left_side ? state.left_y : state.right_y;
            const int x = element.dest_x + static_cast<int>(std::round(axis_x * element.stick_radius));
            const int y = element.dest_y + static_cast<int>(std::round(axis_y * element.stick_radius));
            const Action click = left_side ? Action::LeftStick : Action::RightStick;
            const int active_y = element.source_y + element.height + kPressedTextureGap;
            const bool use_active = action_pressed(state, click) &&
                                    source_rect_inside(preset, element.source_x, active_y,
                                                       element.width, element.height);
            blit_element(canvas, preset, element, use_active ? active_y : element.source_y, x, y);
            break;
        }
        case InputOverlayElementType::Trigger:
            draw_trigger(canvas, preset, element,
                         element.side == 0 ? state.left_trigger : state.right_trigger);
            break;
        case InputOverlayElementType::GamepadId:
            // This source represents one selected controller, so player-one/neutral is offset 0.
            blit_element(canvas, preset, element, element.source_y, element.dest_x, element.dest_y);
            break;
        case InputOverlayElementType::DpadStick: {
            const int offset = dpad_stick_offset(state);
            const int source_x = element.source_x + (element.width + kPressedTextureGap) * offset;
            const int safe_offset = source_rect_inside(preset, source_x, element.source_y,
                                                       element.width, element.height)
                                        ? offset
                                        : 0;
            blit_element(canvas, preset, element, element.source_y, element.dest_x, element.dest_y,
                         (element.width + kPressedTextureGap) * safe_offset);
            break;
        }
        case InputOverlayElementType::Unsupported:
            break;
        }
    }
}

} // namespace mugen
