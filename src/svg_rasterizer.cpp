#include "svg_rasterizer.hpp"

#define NANOSVG_IMPLEMENTATION
#define NANOSVGRAST_IMPLEMENTATION
#include <nanosvg.h>
#include <nanosvgrast.h>

#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace mugen {
namespace {

constexpr uint64_t kMaxSvgBytes = 8ULL * 1024ULL * 1024ULL;

std::filesystem::path utf8_to_path(const std::string &value)
{
#if defined(__cpp_char8_t)
    const auto *begin = reinterpret_cast<const char8_t *>(value.data());
    return std::filesystem::path(std::u8string(begin, begin + value.size()));
#else
    return std::filesystem::path(value);
#endif
}

std::string ascii_lower(std::string value)
{
    for (char &ch : value) {
        if (ch >= 'A' && ch <= 'Z')
            ch = static_cast<char>(ch - 'A' + 'a');
    }
    return value;
}


std::string trim_copy(std::string value)
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

bool is_name_char(char c)
{
    const unsigned char value = static_cast<unsigned char>(c);
    return std::isalnum(value) != 0 || c == '_' || c == '-';
}

bool remove_svg_doctypes(std::string &text, std::string &error)
{
    std::string lower = ascii_lower(text);
    if (lower.find("<!entity") != std::string::npos) {
        error = "SVG contains an entity declaration";
        return false;
    }

    size_t position = 0;
    while ((position = lower.find("<!doctype", position)) != std::string::npos) {
        const size_t end = lower.find('>', position + 9);
        if (end == std::string::npos) {
            error = "SVG contains a malformed DOCTYPE declaration";
            return false;
        }

        const std::string declaration = lower.substr(position, end - position + 1);
        size_t cursor = 9;
        while (cursor < declaration.size() &&
               std::isspace(static_cast<unsigned char>(declaration[cursor])) != 0)
            ++cursor;
        if (declaration.compare(cursor, 3, "svg") != 0 ||
            (cursor + 3 < declaration.size() && is_name_char(declaration[cursor + 3]))) {
            error = "SVG contains an unsupported DOCTYPE declaration";
            return false;
        }
        if (declaration.find('[') != std::string::npos) {
            error = "SVG DOCTYPE internal subsets are not supported";
            return false;
        }

        // GamepadViewer skin assets exported by common vector editors often
        // contain the standard SVG 1.1 PUBLIC doctype. NanoSVG does not need
        // it, and removing it prevents any external DTD resolution while
        // preserving the actual local drawing data.
        text.erase(position, end - position + 1);
        lower.erase(position, end - position + 1);
    }
    return true;
}

std::map<std::string, std::string> collect_simple_svg_class_styles(std::string &text)
{
    std::map<std::string, std::string> styles;
    std::string lower = ascii_lower(text);
    size_t position = 0;
    while ((position = lower.find("<style", position)) != std::string::npos) {
        const size_t open_end = lower.find('>', position + 6);
        if (open_end == std::string::npos)
            break;
        const size_t close = lower.find("</style>", open_end + 1);
        if (close == std::string::npos)
            break;

        std::string body = text.substr(open_end + 1, close - open_end - 1);
        size_t rule_pos = 0;
        while (rule_pos < body.size()) {
            const size_t brace = body.find('{', rule_pos);
            if (brace == std::string::npos)
                break;
            const size_t end_brace = body.find('}', brace + 1);
            if (end_brace == std::string::npos)
                break;
            const std::string selector_text = trim_copy(body.substr(rule_pos, brace - rule_pos));
            const std::string declarations = trim_copy(body.substr(brace + 1, end_brace - brace - 1));

            size_t selector_pos = 0;
            while (selector_pos <= selector_text.size()) {
                const size_t comma = selector_text.find(',', selector_pos);
                const size_t length = comma == std::string::npos
                                          ? selector_text.size() - selector_pos
                                          : comma - selector_pos;
                const std::string selector = trim_copy(selector_text.substr(selector_pos, length));
                if (selector.size() > 1 && selector.front() == '.') {
                    bool simple = true;
                    for (size_t i = 1; i < selector.size(); ++i) {
                        if (!is_name_char(selector[i])) {
                            simple = false;
                            break;
                        }
                    }
                    if (simple && !declarations.empty()) {
                        std::string &stored = styles[ascii_lower(selector.substr(1))];
                        if (!stored.empty() && stored.back() != ';')
                            stored.push_back(';');
                        stored += declarations;
                    }
                }
                if (comma == std::string::npos)
                    break;
                selector_pos = comma + 1;
            }
            rule_pos = end_brace + 1;
        }

        const size_t erase_length = close + 8 - position;
        text.erase(position, erase_length);
        lower.erase(position, erase_length);
    }
    return styles;
}

void inline_simple_svg_class_styles(std::string &text,
                                    const std::map<std::string, std::string> &styles)
{
    if (styles.empty())
        return;

    std::string lower = ascii_lower(text);
    size_t position = 0;
    while ((position = lower.find("class", position)) != std::string::npos) {
        const bool left_boundary = position == 0 || !is_name_char(lower[position - 1]);
        const size_t after_name = position + 5;
        const bool right_boundary = after_name >= lower.size() || !is_name_char(lower[after_name]);
        if (!left_boundary || !right_boundary) {
            position = after_name;
            continue;
        }

        size_t cursor = after_name;
        while (cursor < lower.size() &&
               std::isspace(static_cast<unsigned char>(lower[cursor])) != 0)
            ++cursor;
        if (cursor >= lower.size() || lower[cursor] != '=') {
            position = after_name;
            continue;
        }
        ++cursor;
        while (cursor < lower.size() &&
               std::isspace(static_cast<unsigned char>(lower[cursor])) != 0)
            ++cursor;
        if (cursor >= lower.size() || (lower[cursor] != '\'' && lower[cursor] != '"')) {
            position = after_name;
            continue;
        }

        const char quote = lower[cursor++];
        const size_t value_begin = cursor;
        const size_t value_end = lower.find(quote, value_begin);
        if (value_end == std::string::npos)
            break;

        std::string merged;
        size_t class_pos = value_begin;
        while (class_pos < value_end) {
            while (class_pos < value_end &&
                   std::isspace(static_cast<unsigned char>(lower[class_pos])) != 0)
                ++class_pos;
            size_t class_end = class_pos;
            while (class_end < value_end &&
                   std::isspace(static_cast<unsigned char>(lower[class_end])) == 0)
                ++class_end;
            if (class_end > class_pos) {
                const std::string name = lower.substr(class_pos, class_end - class_pos);
                const auto found = styles.find(name);
                if (found != styles.end()) {
                    if (!merged.empty() && merged.back() != ';')
                        merged.push_back(';');
                    merged += found->second;
                }
            }
            class_pos = class_end + 1;
        }

        if (!merged.empty()) {
            if (merged.back() != ';')
                merged.push_back(';');
            const std::string injection = " style=\"" + merged + "\"";
            const size_t insert_at = value_end + 1;
            text.insert(insert_at, injection);
            lower.insert(insert_at, ascii_lower(injection));
            position = insert_at + injection.size();
        } else {
            position = value_end + 1;
        }
    }
}

} // namespace

bool rasterize_svg_file(const std::string &path, uint32_t max_dimension,
                        uint32_t &width, uint32_t &height,
                        std::vector<uint8_t> &rgba, std::string &error)
{
    std::ifstream stream(utf8_to_path(path), std::ios::binary);
    if (!stream) {
        error = "SVG file could not be opened";
        return false;
    }
    stream.seekg(0, std::ios::end);
    const std::streamoff raw_size = stream.tellg();
    if (raw_size <= 0 || static_cast<uint64_t>(raw_size) > kMaxSvgBytes) {
        error = "SVG file is empty or too large";
        return false;
    }
    stream.seekg(0, std::ios::beg);
    std::string text(static_cast<size_t>(raw_size), '\0');
    stream.read(text.data(), static_cast<std::streamsize>(raw_size));
    if (!stream) {
        error = "SVG file could not be read";
        return false;
    }

    if (!remove_svg_doctypes(text, error))
        return false;

    const auto class_styles = collect_simple_svg_class_styles(text);
    inline_simple_svg_class_styles(text, class_styles);

    std::string security_scan = ascii_lower(text);

    // Normal SVG files declare XML namespaces with URI-looking values such as
    // xmlns="http://www.w3.org/2000/svg". Those strings are identifiers, not
    // network requests. Blank namespace attribute values before checking for
    // actual external resource references, otherwise virtually every normal
    // SVG is rejected.
    size_t namespace_pos = 0;
    while ((namespace_pos = security_scan.find("xmlns", namespace_pos)) != std::string::npos) {
        const bool valid_prefix = namespace_pos == 0 ||
            std::isspace(static_cast<unsigned char>(security_scan[namespace_pos - 1])) != 0 ||
            security_scan[namespace_pos - 1] == '<';
        size_t cursor = namespace_pos + 5;
        while (cursor < security_scan.size() &&
               (std::isalnum(static_cast<unsigned char>(security_scan[cursor])) != 0 ||
                security_scan[cursor] == ':' || security_scan[cursor] == '-' ||
                security_scan[cursor] == '_'))
            ++cursor;
        while (cursor < security_scan.size() &&
               std::isspace(static_cast<unsigned char>(security_scan[cursor])) != 0)
            ++cursor;
        if (!valid_prefix || cursor >= security_scan.size() || security_scan[cursor] != '=') {
            namespace_pos += 5;
            continue;
        }
        ++cursor;
        while (cursor < security_scan.size() &&
               std::isspace(static_cast<unsigned char>(security_scan[cursor])) != 0)
            ++cursor;
        if (cursor >= security_scan.size() ||
            (security_scan[cursor] != '\'' && security_scan[cursor] != '"')) {
            namespace_pos += 5;
            continue;
        }
        const char quote = security_scan[cursor++];
        const size_t value_begin = cursor;
        const size_t value_end = security_scan.find(quote, value_begin);
        if (value_end == std::string::npos)
            break;
        std::fill(security_scan.begin() + static_cast<std::ptrdiff_t>(value_begin),
                  security_scan.begin() + static_cast<std::ptrdiff_t>(value_end), ' ');
        namespace_pos = value_end + 1;
    }

    if (security_scan.find("<script") != std::string::npos ||
        security_scan.find("javascript:") != std::string::npos ||
        security_scan.find("http://") != std::string::npos ||
        security_scan.find("https://") != std::string::npos ||
        security_scan.find("file:") != std::string::npos ||
        security_scan.find("data:") != std::string::npos ||
        security_scan.find("<!entity") != std::string::npos ||
        security_scan.find("<!doctype") != std::string::npos) {
        error = "SVG contains scripts, external data, or unsafe declarations";
        return false;
    }

    text.push_back('\0');
    NSVGimage *image = nsvgParse(text.data(), "px", 96.0F);
    if (!image || !std::isfinite(image->width) || !std::isfinite(image->height) ||
        image->width <= 0.0F || image->height <= 0.0F) {
        if (image)
            nsvgDelete(image);
        error = "SVG could not be parsed";
        return false;
    }

    const float longest = std::max(image->width, image->height);
    const float scale = longest > static_cast<float>(max_dimension)
                            ? static_cast<float>(max_dimension) / longest
                            : 1.0F;
    width = std::max<uint32_t>(1U, static_cast<uint32_t>(std::lround(image->width * scale)));
    height = std::max<uint32_t>(1U, static_cast<uint32_t>(std::lround(image->height * scale)));
    if (static_cast<uint64_t>(width) * height * 4ULL >
        static_cast<uint64_t>(max_dimension) * max_dimension * 4ULL) {
        nsvgDelete(image);
        error = "SVG raster is too large";
        return false;
    }

    NSVGrasterizer *rasterizer = nsvgCreateRasterizer();
    if (!rasterizer) {
        nsvgDelete(image);
        error = "SVG rasterizer could not be created";
        return false;
    }

    rgba.assign(static_cast<size_t>(width) * height * 4U, 0U);
    nsvgRasterize(rasterizer, image, 0.0F, 0.0F, scale, rgba.data(),
                  static_cast<int>(width), static_cast<int>(height),
                  static_cast<int>(width * 4U));
    nsvgDeleteRasterizer(rasterizer);
    nsvgDelete(image);
    error.clear();
    return true;
}

} // namespace mugen
