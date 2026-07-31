#include "gamepadviewer_skin.hpp"
#include "svg_rasterizer.hpp"

#include <graphics/image-file.h>
#include <obs-module.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace mugen {
namespace {

constexpr uint64_t kMaxCssBytes = 2ULL * 1024ULL * 1024ULL;
constexpr uint32_t kMaxDimension = 4096;
constexpr uint64_t kMaxPixelBytes = 64ULL * 1024ULL * 1024ULL;

struct CssRule {
    std::vector<std::string> selectors;
    std::map<std::string, std::string> properties;
};

struct ComputedStyle {
    std::string image;
    std::string width;
    std::string height;
    std::string left;
    std::string top;
    std::string right;
    std::string bottom;
    std::string margin_left;
    std::string margin_top;
    std::string margin_right;
    std::string margin_bottom;
    std::string translate_x;
    std::string translate_y;
    std::string background_x;
    std::string background_y;
    std::string background_width;
    std::string background_height;
    std::string float_side;
    bool background_cover = false;
    bool background_contain = false;
    bool scale_to_box = false;
    bool flip_x = false;
    bool hidden = false;
    float opacity = 1.0F;
    float saturation = 1.0F;
    float hue_rotation = 0.0F;
};

struct Target {
    std::set<std::string> classes;
    std::string parent_class;
    bool controller = false;
    bool disconnected = false;
    bool pseudo_before = false;
    bool pseudo_after = false;
};

struct Box {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
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

std::string path_to_utf8(const std::filesystem::path &path)
{
#if defined(__cpp_char8_t)
    const auto value = path.u8string();
    return std::string(reinterpret_cast<const char *>(value.data()), value.size());
#else
    return path.u8string();
#endif
}

std::string trim(std::string value)
{
    const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char c) {
        return std::isspace(c) != 0;
    });
    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char c) {
        return std::isspace(c) != 0;
    }).base();
    if (first >= last)
        return {};
    return std::string(first, last);
}

std::string ascii_lower(std::string value)
{
    for (char &ch : value) {
        if (ch >= 'A' && ch <= 'Z')
            ch = static_cast<char>(ch - 'A' + 'a');
    }
    return value;
}

bool starts_with_ci(std::string_view value, std::string_view prefix)
{
    if (value.size() < prefix.size())
        return false;
    for (size_t i = 0; i < prefix.size(); ++i) {
        const unsigned char a = static_cast<unsigned char>(value[i]);
        const unsigned char b = static_cast<unsigned char>(prefix[i]);
        if (std::tolower(a) != std::tolower(b))
            return false;
    }
    return true;
}

bool read_text_file(const std::string &path, std::string &out, std::string &error)
{
    std::ifstream stream(utf8_to_path(path), std::ios::binary);
    if (!stream) {
        error = "GamepadViewer CSS could not be opened";
        return false;
    }
    stream.seekg(0, std::ios::end);
    const std::streamoff raw_size = stream.tellg();
    if (raw_size <= 0 || static_cast<uint64_t>(raw_size) > kMaxCssBytes) {
        error = "GamepadViewer CSS is empty or too large";
        return false;
    }
    stream.seekg(0, std::ios::beg);
    out.assign(static_cast<size_t>(raw_size), '\0');
    stream.read(out.data(), static_cast<std::streamsize>(raw_size));
    if (!stream) {
        error = "GamepadViewer CSS could not be read";
        return false;
    }
    return true;
}

std::string strip_comments(const std::string &input)
{
    std::string out;
    out.reserve(input.size());
    bool in_comment = false;
    bool in_quote = false;
    char quote = 0;
    for (size_t i = 0; i < input.size(); ++i) {
        const char c = input[i];
        if (in_comment) {
            if (c == '*' && i + 1 < input.size() && input[i + 1] == '/') {
                in_comment = false;
                ++i;
            }
            continue;
        }
        if (in_quote) {
            out.push_back(c);
            if (c == quote && (i == 0 || input[i - 1] != '\\'))
                in_quote = false;
            continue;
        }
        if (c == '\'' || c == '"') {
            in_quote = true;
            quote = c;
            out.push_back(c);
            continue;
        }
        if (c == '/' && i + 1 < input.size() && input[i + 1] == '*') {
            in_comment = true;
            ++i;
            continue;
        }
        out.push_back(c);
    }
    return out;
}

std::vector<std::string> split_top_level(const std::string &value, char delimiter)
{
    std::vector<std::string> out;
    size_t start = 0;
    int parentheses = 0;
    bool in_quote = false;
    char quote = 0;
    for (size_t i = 0; i <= value.size(); ++i) {
        const char c = i < value.size() ? value[i] : delimiter;
        if (in_quote) {
            if (c == quote && (i == 0 || value[i - 1] != '\\'))
                in_quote = false;
            continue;
        }
        if (c == '\'' || c == '"') {
            in_quote = true;
            quote = c;
            continue;
        }
        if (c == '(')
            ++parentheses;
        else if (c == ')' && parentheses > 0)
            --parentheses;
        else if (c == delimiter && parentheses == 0) {
            out.push_back(trim(value.substr(start, i - start)));
            start = i + 1;
        }
    }
    return out;
}

std::vector<std::string> split_whitespace_top_level(const std::string &value)
{
    std::vector<std::string> out;
    size_t start = std::string::npos;
    int parentheses = 0;
    bool in_quote = false;
    char quote = 0;
    for (size_t i = 0; i <= value.size(); ++i) {
        const char c = i < value.size() ? value[i] : ' ';
        if (in_quote) {
            if (c == quote && (i == 0 || value[i - 1] != '\\'))
                in_quote = false;
            if (start == std::string::npos)
                start = i;
            continue;
        }
        if (c == '\'' || c == '"') {
            in_quote = true;
            quote = c;
            if (start == std::string::npos)
                start = i;
            continue;
        }
        if (c == '(')
            ++parentheses;
        else if (c == ')' && parentheses > 0)
            --parentheses;

        const bool separator = i == value.size() ||
            (parentheses == 0 && std::isspace(static_cast<unsigned char>(c)) != 0);
        if (separator) {
            if (start != std::string::npos) {
                out.push_back(trim(value.substr(start, i - start)));
                start = std::string::npos;
            }
        } else if (start == std::string::npos) {
            start = i;
        }
    }
    return out;
}

std::map<std::string, std::string> parse_declarations(const std::string &body)
{
    std::map<std::string, std::string> result;
    for (const std::string &declaration : split_top_level(body, ';')) {
        const size_t colon = declaration.find(':');
        if (colon == std::string::npos)
            continue;
        const std::string name = ascii_lower(trim(declaration.substr(0, colon)));
        std::string value = trim(declaration.substr(colon + 1));
        const size_t important = ascii_lower(value).find("!important");
        if (important != std::string::npos)
            value = trim(value.substr(0, important));
        if (!name.empty() && !value.empty())
            result[name] = value;
    }
    return result;
}

std::vector<CssRule> parse_css(const std::string &input, size_t &skipped)
{
    const std::string css = strip_comments(input);
    std::vector<CssRule> rules;
    size_t position = 0;
    while (position < css.size()) {
        const size_t open = css.find('{', position);
        if (open == std::string::npos)
            break;
        const size_t close = css.find('}', open + 1);
        if (close == std::string::npos) {
            ++skipped;
            break;
        }
        const std::string header = trim(css.substr(position, open - position));
        const std::string body = css.substr(open + 1, close - open - 1);
        position = close + 1;
        if (header.empty() || header[0] == '@') {
            ++skipped;
            continue;
        }
        CssRule rule;
        rule.selectors = split_top_level(header, ',');
        rule.properties = parse_declarations(body);
        if (rule.selectors.empty() || rule.properties.empty()) {
            ++skipped;
            continue;
        }
        rules.push_back(std::move(rule));
    }
    return rules;
}

std::set<std::string> selector_classes(const std::string &selector)
{
    std::set<std::string> classes;
    for (size_t i = 0; i < selector.size(); ++i) {
        if (selector[i] != '.')
            continue;
        size_t end = i + 1;
        while (end < selector.size()) {
            const char c = selector[end];
            if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-'))
                break;
            ++end;
        }
        if (end > i + 1)
            classes.insert(ascii_lower(selector.substr(i + 1, end - i - 1)));
        i = end > 0 ? end - 1 : i;
    }
    return classes;
}

bool selector_matches(const std::string &raw_selector, const Target &target)
{
    const std::string selector = ascii_lower(raw_selector);
    const bool has_before = selector.find("::before") != std::string::npos ||
                            selector.find(":before") != std::string::npos;
    const bool has_after = selector.find("::after") != std::string::npos ||
                           selector.find(":after") != std::string::npos;
    if (has_before != target.pseudo_before || has_after != target.pseudo_after)
        return false;

    const std::set<std::string> classes = selector_classes(selector);
    const bool wants_controller = classes.contains("controller");
    if (!target.controller && wants_controller)
        return false;
    if (!classes.contains("custom"))
        return false;
    // Selectors such as `.custom.disconnected div` describe arbitrary child
    // elements in the browser DOM. They are not controller/layer selectors
    // that the local renderer can safely interpret.
    if (selector.find(" div") != std::string::npos ||
        selector.find(" span") != std::string::npos ||
        selector.find(" svg") != std::string::npos)
        return false;
    if (classes.contains("disconnected") != target.disconnected &&
        selector.find(":not(.disconnected)") == std::string::npos)
        return false;
    if (selector.find(":not(.disconnected)") != std::string::npos && target.disconnected)
        return false;

    bool matched_target_class = target.controller;
    for (const std::string &name : classes) {
        if (name == "custom" || name == "controller" || name == "disconnected")
            continue;
        if (!target.parent_class.empty() && name == target.parent_class)
            continue;
        if (target.controller || !target.classes.contains(name))
            return false;
        matched_target_class = true;
    }

    // A parent-only selector such as `.custom .sticks` must not be applied to
    // the child `.stick.left`. Parent geometry is computed separately by
    // parent_style_for(). Letting the parent rule also cascade into the child
    // doubles its offset when the child relies on the parent's origin.
    return matched_target_class;
}

std::string extract_url(const std::string &value)
{
    const std::string lower = ascii_lower(value);
    const size_t url = lower.find("url(");
    if (url == std::string::npos)
        return {};
    size_t begin = url + 4;
    size_t end = value.find(')', begin);
    if (end == std::string::npos)
        return {};
    std::string path = trim(value.substr(begin, end - begin));
    if (path.size() >= 2 && ((path.front() == '"' && path.back() == '"') ||
                             (path.front() == '\'' && path.back() == '\'')))
        path = path.substr(1, path.size() - 2);
    return trim(path);
}

std::optional<float> parse_number_prefix(const std::string &raw);

void apply_properties(ComputedStyle &style, const std::map<std::string, std::string> &properties)
{
    for (const auto &[name, value] : properties) {
        if (name == "background" || name == "background-image") {
            const std::string url = extract_url(value);
            if (!url.empty())
                style.image = url;
        } else if (name == "width") style.width = value;
        else if (name == "height") style.height = value;
        else if (name == "left") style.left = value;
        else if (name == "top") style.top = value;
        else if (name == "right") style.right = value;
        else if (name == "bottom") style.bottom = value;
        else if (name == "margin-left") style.margin_left = value;
        else if (name == "margin-top") style.margin_top = value;
        else if (name == "margin-right") style.margin_right = value;
        else if (name == "margin-bottom") style.margin_bottom = value;
        else if (name == "background-position-x") style.background_x = value;
        else if (name == "background-position-y") style.background_y = value;
        else if (name == "background-position") {
            const std::vector<std::string> parts = split_whitespace_top_level(value);
            if (!parts.empty()) style.background_x = parts[0];
            if (parts.size() > 1) style.background_y = parts[1];
        } else if (name == "background-size") {
            const std::string lower = ascii_lower(trim(value));
            style.background_cover = lower == "cover";
            style.background_contain = lower == "contain";
            style.scale_to_box = false;
            style.background_width.clear();
            style.background_height.clear();
            if (!style.background_cover && !style.background_contain) {
                const std::vector<std::string> parts = split_whitespace_top_level(value);
                if (!parts.empty())
                    style.background_width = parts[0];
                if (parts.size() > 1)
                    style.background_height = parts[1];
                else if (!parts.empty())
                    style.background_height = "auto";
                style.scale_to_box = style.background_width == "100%" &&
                                     style.background_height == "100%";
            }
        } else if (name == "filter") {
            const std::string lower = ascii_lower(trim(value));
            if (lower == "none" || lower == "unset" || lower == "initial") {
                style.saturation = 1.0F;
                style.hue_rotation = 0.0F;
            } else {
                const auto function_argument = [&](std::string_view function) -> std::string {
                    const size_t begin = lower.find(function);
                    if (begin == std::string::npos)
                        return {};
                    const size_t value_begin = begin + function.size();
                    const size_t end = lower.find(')', value_begin);
                    if (end == std::string::npos)
                        return {};
                    return trim(lower.substr(value_begin, end - value_begin));
                };
                const std::string saturation = function_argument("saturate(");
                if (!saturation.empty()) {
                    const auto number = parse_number_prefix(saturation);
                    if (number) {
                        const float factor = saturation.ends_with("%") ? *number / 100.0F : *number;
                        style.saturation = std::clamp(factor, 0.0F, 4.0F);
                    }
                }
                const std::string hue = function_argument("hue-rotate(");
                if (!hue.empty()) {
                    const auto number = parse_number_prefix(hue);
                    if (number)
                        style.hue_rotation = *number;
                }
            }
        } else if (name == "opacity") {
            try {
                style.opacity = std::clamp(std::stof(value), 0.0F, 1.0F);
            } catch (...) {
            }
        } else if (name == "display") {
            style.hidden = ascii_lower(trim(value)) == "none";
        } else if (name == "visibility") {
            style.hidden = ascii_lower(trim(value)) == "hidden";
        } else if (name == "float") {
            style.float_side = ascii_lower(trim(value));
        } else if (name == "transform" || name == "-webkit-transform") {
            const std::string lower = ascii_lower(value);
            if (lower.find("rotatey(180") != std::string::npos ||
                lower.find("scalex(-1") != std::string::npos)
                style.flip_x = true;

            const auto function_argument = [&](std::string_view function) -> std::string {
                const size_t begin = lower.find(function);
                if (begin == std::string::npos)
                    return {};
                const size_t value_begin = begin + function.size();
                const size_t end = value.find(')', value_begin);
                if (end == std::string::npos)
                    return {};
                return trim(value.substr(value_begin, end - value_begin));
            };
            const std::string translate_x = function_argument("translatex(");
            const std::string translate_y = function_argument("translatey(");
            if (!translate_x.empty())
                style.translate_x = translate_x;
            if (!translate_y.empty())
                style.translate_y = translate_y;
            const std::string translate = function_argument("translate(");
            if (!translate.empty()) {
                const std::vector<std::string> parts = split_top_level(translate, ',');
                if (!parts.empty())
                    style.translate_x = parts[0];
                if (parts.size() > 1)
                    style.translate_y = parts[1];
            }
        }
    }
}

int selector_specificity(const std::string &selector)
{
    const std::string lower = ascii_lower(selector);
    int score = static_cast<int>(selector_classes(lower).size()) * 10;
    for (char c : lower)
        if (c == '#')
            score += 100;
    if (lower.find("::before") != std::string::npos ||
        lower.find("::after") != std::string::npos)
        score += 1;
    return score;
}

ComputedStyle compute_style(const std::vector<CssRule> &rules, const Target &target)
{
    struct Winner {
        int specificity = -1;
        size_t order = 0;
        std::string value;
    };
    std::map<std::string, Winner> winners;
    for (size_t order = 0; order < rules.size(); ++order) {
        const CssRule &rule = rules[order];
        int best_specificity = -1;
        for (const std::string &selector : rule.selectors) {
            if (selector_matches(selector, target))
                best_specificity = std::max(best_specificity, selector_specificity(selector));
        }
        if (best_specificity < 0)
            continue;
        for (const auto &[name, value] : rule.properties) {
            Winner &winner = winners[name];
            if (best_specificity > winner.specificity ||
                (best_specificity == winner.specificity && order >= winner.order)) {
                winner.specificity = best_specificity;
                winner.order = order;
                winner.value = value;
            }
        }
    }

    std::map<std::string, std::string> properties;
    for (const auto &[name, winner] : winners)
        properties[name] = winner.value;
    ComputedStyle style;
    apply_properties(style, properties);
    return style;
}

std::optional<float> parse_number_prefix(const std::string &raw)
{
    std::string value = trim(raw);
    if (value.empty())
        return std::nullopt;
    char *end = nullptr;
    const float parsed = std::strtof(value.c_str(), &end);
    if (!end || end == value.c_str() || !std::isfinite(parsed))
        return std::nullopt;
    return parsed;
}

struct LengthContext {
    int percent_base = 0;
    int viewport_width = 0;
    int viewport_height = 0;
};

std::optional<float> resolve_length_atom(const std::string &raw, const LengthContext &context)
{
    const std::string value = ascii_lower(trim(raw));
    if (value.empty() || value == "auto")
        return std::nullopt;

    const auto number = parse_number_prefix(value);
    if (!number)
        return std::nullopt;
    if (value.ends_with("vw"))
        return static_cast<float>(context.viewport_width) * *number / 100.0F;
    if (value.ends_with("vh"))
        return static_cast<float>(context.viewport_height) * *number / 100.0F;
    if (value.ends_with("%"))
        return static_cast<float>(context.percent_base) * *number / 100.0F;
    // Unitless values and px are both interpreted as CSS pixels.
    return *number;
}

std::optional<int> resolve_length(const std::string &raw, const LengthContext &context)
{
    std::string value = ascii_lower(trim(raw));
    if (value.empty() || value == "auto")
        return std::nullopt;
    if (value == "0")
        return 0;

    if (!starts_with_ci(value, "calc(")) {
        const auto resolved = resolve_length_atom(value, context);
        return resolved ? std::optional<int>(static_cast<int>(std::lround(*resolved)))
                        : std::nullopt;
    }

    if (value.back() != ')')
        return std::nullopt;
    value = trim(value.substr(5, value.size() - 6));
    float total = 0.0F;
    int sign = 1;
    size_t position = 0;
    bool found = false;
    while (position < value.size()) {
        while (position < value.size() &&
               std::isspace(static_cast<unsigned char>(value[position])) != 0)
            ++position;
        if (position < value.size() && (value[position] == '+' || value[position] == '-')) {
            sign = value[position] == '-' ? -1 : 1;
            ++position;
            while (position < value.size() &&
                   std::isspace(static_cast<unsigned char>(value[position])) != 0)
                ++position;
        }
        const size_t begin = position;
        while (position < value.size() && value[position] != '+' && value[position] != '-')
            ++position;
        const std::string atom = trim(value.substr(begin, position - begin));
        const auto resolved = resolve_length_atom(atom, context);
        if (!resolved)
            return std::nullopt;
        total += static_cast<float>(sign) * *resolved;
        sign = 1;
        found = true;
    }
    return found ? std::optional<int>(static_cast<int>(std::lround(total))) : std::nullopt;
}

std::optional<int> resolve_length(const std::string &raw, int percent_base,
                                  int viewport_width, int viewport_height)
{
    return resolve_length(raw, LengthContext{percent_base, viewport_width, viewport_height});
}

bool inspect_svg_logical_size(const std::filesystem::path &path, int &width, int &height)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
        return false;
    std::string text(64U * 1024U, '\0');
    stream.read(text.data(), static_cast<std::streamsize>(text.size()));
    text.resize(static_cast<size_t>(std::max<std::streamsize>(0, stream.gcount())));
    const std::string lower = ascii_lower(text);
    const size_t svg = lower.find("<svg");
    if (svg == std::string::npos)
        return false;
    const size_t tag_end = lower.find('>', svg + 4);
    if (tag_end == std::string::npos)
        return false;
    const std::string tag = text.substr(svg, tag_end - svg + 1);
    const std::string tag_lower = ascii_lower(tag);

    const auto attribute = [&](const std::string &name) -> std::string {
        size_t pos = tag_lower.find(name);
        while (pos != std::string::npos) {
            const bool left_ok = pos == 0 || !(std::isalnum(static_cast<unsigned char>(tag_lower[pos - 1])) != 0 || tag_lower[pos - 1] == '_' || tag_lower[pos - 1] == '-');
            size_t cursor = pos + name.size();
            const bool right_ok = cursor >= tag_lower.size() || !(std::isalnum(static_cast<unsigned char>(tag_lower[cursor])) != 0 || tag_lower[cursor] == '_' || tag_lower[cursor] == '-');
            if (left_ok && right_ok) {
                while (cursor < tag.size() &&
                       std::isspace(static_cast<unsigned char>(tag[cursor])) != 0)
                    ++cursor;
                if (cursor < tag.size() && tag[cursor] == '=') {
                    ++cursor;
                    while (cursor < tag.size() &&
                           std::isspace(static_cast<unsigned char>(tag[cursor])) != 0)
                        ++cursor;
                    if (cursor < tag.size() && (tag[cursor] == '\'' || tag[cursor] == '"')) {
                        const char quote = tag[cursor++];
                        const size_t end = tag.find(quote, cursor);
                        if (end != std::string::npos)
                            return tag.substr(cursor, end - cursor);
                    }
                }
            }
            pos = tag_lower.find(name, pos + name.size());
        }
        return {};
    };

    const std::string view_box = attribute("viewbox");
    if (!view_box.empty()) {
        std::istringstream values(view_box);
        float min_x = 0.0F, min_y = 0.0F, w = 0.0F, h = 0.0F;
        if (values >> min_x >> min_y >> w >> h && std::isfinite(w) && std::isfinite(h) &&
            w > 0.0F && h > 0.0F) {
            width = std::max(1, static_cast<int>(std::lround(w)));
            height = std::max(1, static_cast<int>(std::lround(h)));
            return true;
        }
    }

    const auto raw_width = parse_number_prefix(attribute("width"));
    const auto raw_height = parse_number_prefix(attribute("height"));
    if (raw_width && raw_height && *raw_width > 0.0F && *raw_height > 0.0F) {
        width = std::max(1, static_cast<int>(std::lround(*raw_width)));
        height = std::max(1, static_cast<int>(std::lround(*raw_height)));
        return true;
    }
    return false;
}

bool local_asset_path(const std::filesystem::path &root, const std::string &reference,
                      std::filesystem::path &out)
{
    const std::string lower = ascii_lower(trim(reference));
    if (lower.empty() || starts_with_ci(lower, "http:") || starts_with_ci(lower, "https:") ||
        starts_with_ci(lower, "data:") || starts_with_ci(lower, "file:") ||
        starts_with_ci(lower, "//"))
        return false;

    std::filesystem::path ref = utf8_to_path(reference);
    if (ref.is_absolute())
        return false;

    std::error_code ec;
    const std::filesystem::path absolute_root =
        std::filesystem::weakly_canonical(std::filesystem::absolute(root, ec), ec);
    if (ec)
        return false;
    const std::filesystem::path candidate =
        std::filesystem::weakly_canonical(absolute_root / ref, ec);
    if (ec)
        return false;

    auto root_it = absolute_root.begin();
    auto candidate_it = candidate.begin();
    for (; root_it != absolute_root.end(); ++root_it, ++candidate_it) {
        if (candidate_it == candidate.end())
            return false;
#if defined(_WIN32)
        if (ascii_lower(path_to_utf8(*root_it)) != ascii_lower(path_to_utf8(*candidate_it)))
            return false;
#else
        if (*root_it != *candidate_it)
            return false;
#endif
    }
    if (!std::filesystem::is_regular_file(candidate, ec) || ec)
        return false;
    out = candidate;
    return true;
}

bool load_bitmap(const std::string &path, GpvRasterImage &image, std::string &error)
{
    gs_image_file_t decoded{};
    gs_image_file_init(&decoded, path.c_str());
    if (!decoded.loaded || !decoded.texture_data || decoded.cx == 0 || decoded.cy == 0) {
        obs_enter_graphics();
        gs_image_file_free(&decoded);
        obs_leave_graphics();
        error = "Image could not be decoded";
        return false;
    }
    if (decoded.cx > kMaxDimension || decoded.cy > kMaxDimension ||
        static_cast<uint64_t>(decoded.cx) * decoded.cy * 4ULL > kMaxPixelBytes) {
        obs_enter_graphics();
        gs_image_file_free(&decoded);
        obs_leave_graphics();
        error = "Image is too large";
        return false;
    }

    image.width = decoded.cx;
    image.height = decoded.cy;
    const size_t pixels = static_cast<size_t>(decoded.cx) * decoded.cy;
    image.rgba.resize(pixels * 4U);
    if (decoded.format == GS_RGBA) {
        std::copy_n(decoded.texture_data, pixels * 4U, image.rgba.data());
    } else if (decoded.format == GS_BGRA) {
        for (size_t i = 0; i < pixels; ++i) {
            image.rgba[i * 4U + 0] = decoded.texture_data[i * 4U + 2];
            image.rgba[i * 4U + 1] = decoded.texture_data[i * 4U + 1];
            image.rgba[i * 4U + 2] = decoded.texture_data[i * 4U + 0];
            image.rgba[i * 4U + 3] = decoded.texture_data[i * 4U + 3];
        }
    } else {
        obs_enter_graphics();
        gs_image_file_free(&decoded);
        obs_leave_graphics();
        error = "Image pixel format is unsupported";
        return false;
    }
    obs_enter_graphics();
    gs_image_file_free(&decoded);
    obs_leave_graphics();
    return true;
}

bool load_image(const std::filesystem::path &path, GpvRasterImage &image, std::string &error)
{
    const std::string extension = ascii_lower(path_to_utf8(path.extension()));
    const std::string utf8 = path_to_utf8(path);
    if (extension == ".svg") {
        return rasterize_svg_file(utf8, kMaxDimension, image.width, image.height,
                                  image.rgba, error);
    }
    if (extension == ".png" || extension == ".jpg" || extension == ".jpeg")
        return load_bitmap(utf8, image, error);
    error = "Only local SVG, PNG and JPEG assets are supported";
    return false;
}

Box style_box(const ComputedStyle &style, const Box &parent, const GpvRasterImage *image,
              const std::set<std::string> &classes, int viewport_width, int viewport_height)
{
    Box box;
    box.width = resolve_length(style.width, parent.width, viewport_width, viewport_height).value_or(
        image ? static_cast<int>(image->width) : parent.width);
    box.height = resolve_length(style.height, parent.height, viewport_width, viewport_height).value_or(
        image ? static_cast<int>(image->height) : parent.height);
    box.width = std::max(1, box.width);
    box.height = std::max(1, box.height);

    const std::optional<int> left =
        resolve_length(style.left, parent.width, viewport_width, viewport_height);
    const std::optional<int> right =
        resolve_length(style.right, parent.width, viewport_width, viewport_height);
    const std::optional<int> top =
        resolve_length(style.top, parent.height, viewport_width, viewport_height);
    const std::optional<int> bottom =
        resolve_length(style.bottom, parent.height, viewport_width, viewport_height);

    if (left)
        box.x = parent.x + *left;
    else if (right)
        box.x = parent.x + parent.width - box.width - *right;
    else if (style.float_side == "right" || classes.contains("right"))
        box.x = parent.x + parent.width - box.width;
    else
        box.x = parent.x;

    if (top)
        box.y = parent.y + *top;
    else if (bottom)
        box.y = parent.y + parent.height - box.height - *bottom;
    else
        box.y = parent.y;
    box.x += resolve_length(style.margin_left, parent.width, viewport_width, viewport_height).value_or(0);
    box.x -= resolve_length(style.margin_right, parent.width, viewport_width, viewport_height).value_or(0);
    box.y += resolve_length(style.margin_top, parent.height, viewport_width, viewport_height).value_or(0);
    box.y -= resolve_length(style.margin_bottom, parent.height, viewport_width, viewport_height).value_or(0);
    box.x += resolve_length(style.translate_x, parent.width, viewport_width, viewport_height).value_or(0);
    box.y += resolve_length(style.translate_y, parent.height, viewport_width, viewport_height).value_or(0);
    return box;
}

std::pair<int, int> resolve_background_size(const ComputedStyle &style, const Box &box,
                                            const GpvRasterImage &image,
                                            int viewport_width, int viewport_height)
{
    const float intrinsic_width = static_cast<float>(std::max<uint32_t>(1U, image.width));
    const float intrinsic_height = static_cast<float>(std::max<uint32_t>(1U, image.height));
    if (style.background_cover || style.background_contain) {
        const float sx = static_cast<float>(box.width) / intrinsic_width;
        const float sy = static_cast<float>(box.height) / intrinsic_height;
        const float scale = style.background_cover ? std::max(sx, sy) : std::min(sx, sy);
        return {std::max(1, static_cast<int>(std::lround(intrinsic_width * scale))),
                std::max(1, static_cast<int>(std::lround(intrinsic_height * scale)))};
    }

    const bool width_auto = style.background_width.empty() ||
                            ascii_lower(trim(style.background_width)) == "auto";
    const bool height_auto = style.background_height.empty() ||
                             ascii_lower(trim(style.background_height)) == "auto";
    std::optional<int> width;
    std::optional<int> height;
    if (!width_auto)
        width = resolve_length(style.background_width, box.width, viewport_width, viewport_height);
    if (!height_auto)
        height = resolve_length(style.background_height, box.height, viewport_width, viewport_height);

    if (!width && !height)
        return {static_cast<int>(image.width), static_cast<int>(image.height)};
    if (!width && height)
        width = static_cast<int>(std::lround(static_cast<float>(*height) * intrinsic_width /
                                             intrinsic_height));
    if (width && !height)
        height = static_cast<int>(std::lround(static_cast<float>(*width) * intrinsic_height /
                                              intrinsic_width));
    return {std::max(1, *width), std::max(1, *height)};
}

int resolve_background_position(const std::string &raw, int box_size, int background_size,
                                int viewport_width, int viewport_height, bool horizontal)
{
    const std::string value = ascii_lower(trim(raw));
    if (value.empty() || value == (horizontal ? "left" : "top"))
        return 0;
    if (value == "center")
        return (box_size - background_size) / 2;
    if (value == (horizontal ? "right" : "bottom"))
        return box_size - background_size;
    if (value.ends_with("%")) {
        const auto percent = parse_number_prefix(value);
        if (percent)
            return static_cast<int>(std::lround(
                static_cast<float>(box_size - background_size) * *percent / 100.0F));
    }
    return resolve_length(value, box_size, viewport_width, viewport_height).value_or(0);
}

bool styles_equivalent(const ComputedStyle &a, const ComputedStyle &b)
{
    return a.image == b.image && a.width == b.width && a.height == b.height &&
           a.left == b.left && a.top == b.top && a.right == b.right &&
           a.bottom == b.bottom && a.margin_left == b.margin_left &&
           a.margin_top == b.margin_top && a.margin_right == b.margin_right &&
           a.margin_bottom == b.margin_bottom && a.translate_x == b.translate_x &&
           a.translate_y == b.translate_y && a.background_x == b.background_x &&
           a.background_y == b.background_y &&
           a.background_width == b.background_width &&
           a.background_height == b.background_height &&
           a.float_side == b.float_side &&
           a.background_cover == b.background_cover &&
           a.background_contain == b.background_contain &&
           a.scale_to_box == b.scale_to_box && a.flip_x == b.flip_x &&
           a.hidden == b.hidden &&
           std::abs(a.opacity - b.opacity) < 0.001F &&
           std::abs(a.saturation - b.saturation) < 0.001F &&
           std::abs(a.hue_rotation - b.hue_rotation) < 0.001F;
}

std::optional<GpvLayer> make_layer(const ComputedStyle &style, const ComputedStyle &parent_style,
                                   const std::set<std::string> &classes,
                                   const std::filesystem::path &asset_root, const Box &canvas,
                                   std::unordered_map<std::string, std::shared_ptr<GpvRasterImage>> &cache,
                                   size_t &rejected_assets, std::string &warning,
                                   bool preserve_zero_opacity = false)
{
    if (style.hidden || style.image.empty() ||
        (!preserve_zero_opacity && style.opacity <= 0.0F))
        return std::nullopt;

    std::filesystem::path asset_path;
    if (!local_asset_path(asset_root, style.image, asset_path)) {
        ++rejected_assets;
        warning = "Some CSS assets were missing, remote, or outside the skin folder";
        return std::nullopt;
    }
    const std::string key = path_to_utf8(asset_path);
    auto found = cache.find(key);
    if (found == cache.end()) {
        auto loaded = std::make_shared<GpvRasterImage>();
        std::string image_error;
        if (!load_image(asset_path, *loaded, image_error)) {
            ++rejected_assets;
            warning = image_error;
            return std::nullopt;
        }
        found = cache.emplace(key, std::move(loaded)).first;
    }

    Box parent = canvas;
    if (!parent_style.width.empty() || !parent_style.height.empty() ||
        !parent_style.left.empty() || !parent_style.top.empty() ||
        !parent_style.right.empty() || !parent_style.bottom.empty() ||
        !parent_style.margin_left.empty() || !parent_style.margin_top.empty() ||
        !parent_style.margin_right.empty() || !parent_style.margin_bottom.empty() ||
        !parent_style.translate_x.empty() || !parent_style.translate_y.empty())
        parent = style_box(parent_style, canvas, nullptr, {}, canvas.width, canvas.height);
    const Box box = style_box(style, parent, found->second.get(), classes,
                              canvas.width, canvas.height);

    GpvLayer layer;
    layer.image = found->second;
    layer.x = box.x;
    layer.y = box.y;
    layer.width = box.width;
    layer.height = box.height;
    const auto [background_width, background_height] =
        resolve_background_size(style, box, *layer.image, canvas.width, canvas.height);
    layer.background_width = background_width;
    layer.background_height = background_height;
    layer.background_x = resolve_background_position(style.background_x, box.width,
                                                      background_width, canvas.width,
                                                      canvas.height, true);
    layer.background_y = resolve_background_position(style.background_y, box.height,
                                                      background_height, canvas.width,
                                                      canvas.height, false);
    layer.scale_to_box = style.scale_to_box ||
                         (background_width == box.width && background_height == box.height);
    layer.flip_x = style.flip_x;
    layer.saturation = style.saturation;
    layer.hue_rotation = style.hue_rotation;
    layer.opacity = preserve_zero_opacity
                        ? 255
                        : static_cast<uint8_t>(std::lround(style.opacity * 255.0F));
    return layer;
}

ComputedStyle parent_style_for(const std::vector<CssRule> &rules, const std::string &parent_class)
{
    if (parent_class.empty())
        return {};
    Target parent;
    parent.classes = {parent_class};
    return compute_style(rules, parent);
}

GpvLayerPair make_pair(const std::vector<CssRule> &rules, Target target,
                       const std::filesystem::path &asset_root, const Box &canvas,
                       std::unordered_map<std::string, std::shared_ptr<GpvRasterImage>> &cache,
                       size_t &loaded, size_t &rejected, std::string &warning)
{
    const ComputedStyle parent = parent_style_for(rules, target.parent_class);
    const ComputedStyle normal_style = compute_style(rules, target);
    GpvLayerPair pair;
    pair.normal = make_layer(normal_style, parent, target.classes, asset_root, canvas,
                             cache, rejected, warning);
    if (pair.normal)
        ++loaded;

    target.classes.insert("pressed");
    const ComputedStyle pressed_style = compute_style(rules, target);
    if (!styles_equivalent(normal_style, pressed_style)) {
        pair.pressed = make_layer(pressed_style, parent, target.classes, asset_root, canvas,
                                  cache, rejected, warning);
        if (pair.pressed)
            ++loaded;
    }
    return pair;
}

std::optional<GpvLayer> make_analog_layer(
    const std::vector<CssRule> &rules, Target target,
    const std::filesystem::path &asset_root, const Box &canvas,
    std::unordered_map<std::string, std::shared_ptr<GpvRasterImage>> &cache,
    size_t &loaded, size_t &rejected, std::string &warning)
{
    const ComputedStyle parent = parent_style_for(rules, target.parent_class);
    const ComputedStyle normal_style = compute_style(rules, target);
    target.classes.insert("pressed");
    const ComputedStyle pressed_style = compute_style(rules, target);

    // GamepadViewer commonly keeps the analog overlay at opacity: 0 in CSS
    // and changes opacity from JavaScript. Prefer the pressed style when it
    // carries the visible image/geometry, then let our renderer supply alpha.
    const ComputedStyle &chosen =
        (!pressed_style.image.empty() && !styles_equivalent(normal_style, pressed_style))
            ? pressed_style
            : normal_style;
    auto layer = make_layer(chosen, parent, target.classes, asset_root, canvas, cache,
                            rejected, warning, true);
    if (layer)
        ++loaded;
    return layer;
}

Target target(std::initializer_list<const char *> classes, const char *parent = "")
{
    Target out;
    for (const char *value : classes)
        out.classes.insert(value);
    out.parent_class = parent ? parent : "";
    return out;
}

void draw_layer(PixelCanvas &canvas, const std::optional<GpvLayer> &layer,
                int offset_x = 0, int offset_y = 0, float opacity_scale = 1.0F)
{
    if (!layer || !layer->image || layer->image->rgba.empty())
        return;
    const uint8_t opacity = static_cast<uint8_t>(std::lround(
        std::clamp(opacity_scale, 0.0F, 1.0F) * static_cast<float>(layer->opacity)));
    canvas.blit_rgba_scaled(layer->image->rgba.data(), layer->image->width, layer->image->height,
                            layer->x + offset_x, layer->y + offset_y, layer->width, layer->height,
                            layer->background_x, layer->background_y,
                            layer->background_width, layer->background_height,
                            layer->scale_to_box, layer->flip_x, layer->saturation,
                            layer->hue_rotation, opacity);
}

const std::optional<GpvLayer> &selected_layer(const GpvLayerPair &pair, bool pressed)
{
    return pressed && pair.pressed ? pair.pressed : pair.normal;
}

void draw_pair(PixelCanvas &canvas, const GpvLayerPair &pair, bool pressed)
{
    draw_layer(canvas, selected_layer(pair, pressed));
}

void draw_moving_stick(PixelCanvas &canvas, const GpvLayerPair &pair, bool pressed,
                       float axis_x, float axis_y, float travel)
{
    const std::optional<GpvLayer> &layer = selected_layer(pair, pressed);
    if (!layer)
        return;

    float x = axis_x;
    float y = axis_y;
    const float magnitude = std::sqrt(x * x + y * y);
    if (magnitude > 1.0F) {
        x /= magnitude;
        y /= magnitude;
    }
    const float clamped_travel = std::clamp(travel, 0.0F, 0.5F);
    const int radius_x = static_cast<int>(std::lround(
        static_cast<float>(std::max(1, layer->width)) * clamped_travel));
    const int radius_y = static_cast<int>(std::lround(
        static_cast<float>(std::max(1, layer->height)) * clamped_travel));
    draw_layer(canvas, layer, static_cast<int>(std::lround(x * radius_x)),
               static_cast<int>(std::lround(y * radius_y)));
}

void draw_trigger(PixelCanvas &canvas, const GpvTriggerVisual &trigger, float value,
                  float threshold)
{
    const float normalized = std::clamp(value, 0.0F, 1.0F);
    draw_layer(canvas, trigger.analog, 0, 0, normalized);
    draw_pair(canvas, trigger.button, normalized >= std::clamp(threshold, 0.0F, 1.0F));
}

int fight_stick_index(const GamepadState &state)
{
    const bool up = state.pressed[static_cast<size_t>(Action::DpadUp)];
    const bool down = state.pressed[static_cast<size_t>(Action::DpadDown)];
    const bool left = state.pressed[static_cast<size_t>(Action::DpadLeft)];
    const bool right = state.pressed[static_cast<size_t>(Action::DpadRight)];
    if (up && left) return 5;
    if (up && right) return 6;
    if (down && left) return 7;
    if (down && right) return 8;
    if (left) return 1;
    if (right) return 2;
    if (up) return 3;
    if (down) return 4;
    return 0;
}

} // namespace

bool load_gamepadviewer_skin(const std::string &css_path, GamepadViewerSkin &out, std::string &error)
{
    if (css_path.empty()) {
        error = "Select a local GamepadViewer CSS file";
        return false;
    }
    std::string text;
    if (!read_text_file(css_path, text, error))
        return false;

    GamepadViewerSkin parsed;
    parsed.css_path = css_path;
    const std::vector<CssRule> rules = parse_css(text, parsed.skipped_rules);
    if (rules.empty()) {
        error = "No usable CSS rules were found";
        return false;
    }

    const std::filesystem::path root = utf8_to_path(css_path).parent_path();
    std::unordered_map<std::string, std::shared_ptr<GpvRasterImage>> cache;

    Target base_target;
    base_target.controller = true;
    const ComputedStyle base_style = compute_style(rules, base_target);
    std::filesystem::path base_path;
    if (base_style.image.empty() || !local_asset_path(root, base_style.image, base_path)) {
        error = "GamepadViewer CSS has no usable local controller background";
        return false;
    }
    GpvRasterImage base_image;
    if (!load_image(base_path, base_image, error))
        return false;

    int logical_width = static_cast<int>(base_image.width);
    int logical_height = static_cast<int>(base_image.height);
    if (ascii_lower(path_to_utf8(base_path.extension())) == ".svg")
        inspect_svg_logical_size(base_path, logical_width, logical_height);
    parsed.width = static_cast<uint32_t>(std::clamp(
        resolve_length(base_style.width, logical_width, logical_width, logical_height).value_or(
            logical_width), 1, static_cast<int>(kMaxDimension)));
    parsed.height = static_cast<uint32_t>(std::clamp(
        resolve_length(base_style.height, logical_height, logical_width, logical_height).value_or(
            logical_height), 1, static_cast<int>(kMaxDimension)));
    const Box canvas{0, 0, static_cast<int>(parsed.width), static_cast<int>(parsed.height)};
    cache.emplace(path_to_utf8(base_path), std::make_shared<GpvRasterImage>(base_image));
    parsed.base = make_layer(base_style, {}, {}, root, canvas, cache,
                             parsed.rejected_assets, parsed.warning);
    if (!parsed.base) {
        error = "Controller background could not be prepared";
        return false;
    }
    parsed.base->x = 0;
    parsed.base->y = 0;
    parsed.base->width = static_cast<int>(parsed.width);
    parsed.base->height = static_cast<int>(parsed.height);
    parsed.base->background_x = 0;
    parsed.base->background_y = 0;
    parsed.base->background_width = static_cast<int>(parsed.width);
    parsed.base->background_height = static_cast<int>(parsed.height);
    parsed.base->scale_to_box = true;
    ++parsed.loaded_layers;

    Target disconnected_target = base_target;
    disconnected_target.disconnected = true;
    const ComputedStyle disconnected_style = compute_style(rules, disconnected_target);
    if (!disconnected_style.image.empty()) {
        parsed.disconnected = make_layer(disconnected_style, {}, {}, root, canvas, cache,
                                         parsed.rejected_assets, parsed.warning);
        if (parsed.disconnected) {
            parsed.disconnected->x = 0;
            parsed.disconnected->y = 0;
            parsed.disconnected->width = static_cast<int>(parsed.width);
            parsed.disconnected->height = static_cast<int>(parsed.height);
            parsed.disconnected->background_x = 0;
            parsed.disconnected->background_y = 0;
            parsed.disconnected->background_width = static_cast<int>(parsed.width);
            parsed.disconnected->background_height = static_cast<int>(parsed.height);
            parsed.disconnected->scale_to_box = true;
            ++parsed.loaded_layers;
        }
    }

    Target disconnected_overlay_target = disconnected_target;
    disconnected_overlay_target.pseudo_after = true;
    const ComputedStyle disconnected_overlay_style =
        compute_style(rules, disconnected_overlay_target);
    if (!disconnected_overlay_style.image.empty()) {
        parsed.disconnected_overlay =
            make_layer(disconnected_overlay_style, {}, {}, root, canvas, cache,
                       parsed.rejected_assets, parsed.warning);
        if (parsed.disconnected_overlay) {
            parsed.disconnected_overlay->x = 0;
            parsed.disconnected_overlay->y = 0;
            parsed.disconnected_overlay->width = static_cast<int>(parsed.width);
            parsed.disconnected_overlay->height = static_cast<int>(parsed.height);
            parsed.disconnected_overlay->background_x = 0;
            parsed.disconnected_overlay->background_y = 0;
            parsed.disconnected_overlay->background_width = static_cast<int>(parsed.width);
            parsed.disconnected_overlay->background_height = static_cast<int>(parsed.height);
            parsed.disconnected_overlay->scale_to_box = true;
            ++parsed.loaded_layers;
        }
    }

    Target dpad_base = target({"dpad"});
    dpad_base.pseudo_before = true;
    const ComputedStyle dpad_base_style = compute_style(rules, dpad_base);
    if (!dpad_base_style.image.empty()) {
        const ComputedStyle parent = parent_style_for(rules, "dpad");
        auto layer = make_layer(dpad_base_style, parent, dpad_base.classes, root, canvas, cache,
                                parsed.rejected_assets, parsed.warning);
        if (layer) {
            parsed.static_layers.push_back(std::move(*layer));
            ++parsed.loaded_layers;
        }
    }

    const auto add_pseudo_static = [&](const char *parent_class, bool before) {
        Target pseudo = target({parent_class});
        pseudo.pseudo_before = before;
        pseudo.pseudo_after = !before;
        const ComputedStyle style = compute_style(rules, pseudo);
        if (style.image.empty())
            return;
        const ComputedStyle parent = parent_style_for(rules, parent_class);
        auto layer = make_layer(style, parent, pseudo.classes, root, canvas, cache,
                                parsed.rejected_assets, parsed.warning);
        if (layer) {
            parsed.static_layers.push_back(std::move(*layer));
            ++parsed.loaded_layers;
        }
    };
    // Istador-style skins draw the neutral L2/R2 caps with pseudo-elements and
    // place the analog pressed overlays in `.trigger.left/right`.
    add_pseudo_static("triggers", true);
    add_pseudo_static("triggers", false);

    // Player light bars are regular static layers. p0 is the browser's default
    // first-player class; hue rotation for other player slots is intentionally
    // not executed by the local CSS subset.
    const Target quadrant_target = target({"quadrant", "p0"});
    const ComputedStyle quadrant_style = compute_style(rules, quadrant_target);
    if (!quadrant_style.image.empty()) {
        auto layer = make_layer(quadrant_style, {}, quadrant_target.classes, root, canvas, cache,
                                parsed.rejected_assets, parsed.warning);
        if (layer) {
            parsed.static_layers.push_back(std::move(*layer));
            ++parsed.loaded_layers;
        }
    }

    const std::array<std::pair<Action, Target>, 13> action_targets{{
        {Action::South, target({"button", "a"}, "abxy")},
        {Action::East, target({"button", "b"}, "abxy")},
        {Action::West, target({"button", "x"}, "abxy")},
        {Action::North, target({"button", "y"}, "abxy")},
        {Action::Back, target({"back"}, "arrows")},
        {Action::Guide, target({"meta"})},
        {Action::Start, target({"start"}, "arrows")},
        {Action::Misc1, target({"capture"})},
        {Action::Touchpad, target({"touchpad"})},
        {Action::LeftShoulder, target({"bumper", "left"}, "bumpers")},
        {Action::RightShoulder, target({"bumper", "right"}, "bumpers")},
        {Action::LeftStick, target({"stick", "left"}, "sticks")},
        {Action::RightStick, target({"stick", "right"}, "sticks")},
    }};
    for (const auto &[action, visual] : action_targets) {
        parsed.actions[static_cast<size_t>(action)] =
            make_pair(rules, visual, root, canvas, cache, parsed.loaded_layers,
                      parsed.rejected_assets, parsed.warning);
    }

    const auto pair_empty = [](const GpvLayerPair &pair) {
        return !pair.normal && !pair.pressed;
    };
    if (pair_empty(parsed.actions[static_cast<size_t>(Action::Guide)]))
        parsed.actions[static_cast<size_t>(Action::Guide)] =
            make_pair(rules, target({"guide"}), root, canvas, cache, parsed.loaded_layers,
                      parsed.rejected_assets, parsed.warning);
    if (pair_empty(parsed.actions[static_cast<size_t>(Action::Guide)]))
        parsed.actions[static_cast<size_t>(Action::Guide)] =
            make_pair(rules, target({"home"}), root, canvas, cache, parsed.loaded_layers,
                      parsed.rejected_assets, parsed.warning);
    if (pair_empty(parsed.actions[static_cast<size_t>(Action::Misc1)]))
        parsed.actions[static_cast<size_t>(Action::Misc1)] =
            make_pair(rules, target({"misc"}), root, canvas, cache, parsed.loaded_layers,
                      parsed.rejected_assets, parsed.warning);

    if (!pair_empty(parsed.actions[static_cast<size_t>(Action::LeftStick)]))
        ++parsed.analog_sticks;
    if (!pair_empty(parsed.actions[static_cast<size_t>(Action::RightStick)]))
        ++parsed.analog_sticks;

    parsed.left_trigger.button =
        make_pair(rules, target({"trigger-button", "left"}, "triggers"),
                  root, canvas, cache, parsed.loaded_layers,
                  parsed.rejected_assets, parsed.warning);
    parsed.left_trigger.analog =
        make_analog_layer(rules, target({"trigger", "left"}, "triggers"),
                          root, canvas, cache, parsed.loaded_layers,
                          parsed.rejected_assets, parsed.warning);
    if (parsed.left_trigger.analog)
        ++parsed.analog_triggers;

    parsed.right_trigger.button =
        make_pair(rules, target({"trigger-button", "right"}, "triggers"),
                  root, canvas, cache, parsed.loaded_layers,
                  parsed.rejected_assets, parsed.warning);
    parsed.right_trigger.analog =
        make_analog_layer(rules, target({"trigger", "right"}, "triggers"),
                          root, canvas, cache, parsed.loaded_layers,
                          parsed.rejected_assets, parsed.warning);
    if (parsed.right_trigger.analog)
        ++parsed.analog_triggers;

    const std::array<std::pair<Action, Target>, 4> dpad_targets{{
        {Action::DpadUp, target({"face", "up"}, "dpad")},
        {Action::DpadRight, target({"face", "right"}, "dpad")},
        {Action::DpadDown, target({"face", "down"}, "dpad")},
        {Action::DpadLeft, target({"face", "left"}, "dpad")},
    }};
    for (size_t i = 0; i < dpad_targets.size(); ++i) {
        parsed.dpad[i] = make_pair(rules, dpad_targets[i].second, root, canvas, cache,
                                   parsed.loaded_layers, parsed.rejected_assets,
                                   parsed.warning);
        parsed.actions[static_cast<size_t>(dpad_targets[i].first)] = {};
    }

    const std::array<Target, 9> fstick_targets{{
        target({"fstick"}), target({"fstick", "left"}), target({"fstick", "right"}),
        target({"fstick", "up"}), target({"fstick", "down"}),
        target({"fstick", "up", "left"}), target({"fstick", "up", "right"}),
        target({"fstick", "down", "left"}), target({"fstick", "down", "right"}),
    }};
    for (size_t i = 0; i < fstick_targets.size(); ++i) {
        const ComputedStyle style = compute_style(rules, fstick_targets[i]);
        parsed.fight_stick[i] = make_layer(style, {}, fstick_targets[i].classes, root, canvas,
                                            cache, parsed.rejected_assets, parsed.warning);
        if (parsed.fight_stick[i])
            ++parsed.loaded_layers;
    }

    if (parsed.loaded_layers <= 1) {
        error = "Controller background loaded, but no supported input layers were found";
        return false;
    }
    out = std::move(parsed);
    error.clear();
    return true;
}

void draw_gamepadviewer_skin(PixelCanvas &canvas, const GamepadViewerSkin &skin,
                             const GamepadState &state, float stick_travel,
                             float trigger_threshold)
{
    canvas.clear();
    if (!state.connected && skin.disconnected) {
        draw_layer(canvas, skin.disconnected);
        draw_layer(canvas, skin.disconnected_overlay);
    } else {
        draw_layer(canvas, skin.base);
    }

    if (!state.connected && skin.disconnected)
        return;

    for (const GpvLayer &layer : skin.static_layers)
        draw_layer(canvas, layer);

    draw_trigger(canvas, skin.left_trigger, state.left_trigger, trigger_threshold);
    draw_trigger(canvas, skin.right_trigger, state.right_trigger, trigger_threshold);

    for (size_t i = 0; i < static_cast<size_t>(Action::Count); ++i) {
        const Action action = static_cast<Action>(i);
        if (action == Action::DpadUp || action == Action::DpadRight ||
            action == Action::DpadDown || action == Action::DpadLeft ||
            action == Action::LeftStick || action == Action::RightStick)
            continue;
        draw_pair(canvas, skin.actions[i], state.pressed[i]);
    }

    const std::array<Action, 4> dpad_actions{Action::DpadUp, Action::DpadRight,
                                             Action::DpadDown, Action::DpadLeft};
    for (size_t i = 0; i < dpad_actions.size(); ++i)
        draw_pair(canvas, skin.dpad[i], state.pressed[static_cast<size_t>(dpad_actions[i])]);

    const int index = fight_stick_index(state);
    if (skin.fight_stick[static_cast<size_t>(index)])
        draw_layer(canvas, skin.fight_stick[static_cast<size_t>(index)]);
    else
        draw_layer(canvas, skin.fight_stick[0]);

    // Draw analog stick caps last. Several real GamepadViewer skins use
    // full-canvas D-pad SVG layers; drawing those after the sticks can paint
    // the neutral left-stick artwork over our moving/pressed left-stick layer.
    draw_moving_stick(canvas, skin.actions[static_cast<size_t>(Action::LeftStick)],
                      state.pressed[static_cast<size_t>(Action::LeftStick)],
                      state.left_x, state.left_y, stick_travel);
    draw_moving_stick(canvas, skin.actions[static_cast<size_t>(Action::RightStick)],
                      state.pressed[static_cast<size_t>(Action::RightStick)],
                      state.right_x, state.right_y, stick_travel);
}

} // namespace mugen
