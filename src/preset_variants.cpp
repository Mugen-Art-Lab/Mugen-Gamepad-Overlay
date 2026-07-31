// SPDX-FileCopyrightText: 2026 Mugen Art Lab
// SPDX-License-Identifier: GPL-2.0-or-later

#include "preset_variants.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <system_error>
#include <tuple>

namespace mugen {
namespace {

std::filesystem::path utf8_to_path(const std::string &value)
{
#if defined(__cpp_char8_t)
    return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t *>(value.data()), value.size()));
#else
    return std::filesystem::u8path(value);
#endif
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

std::string ascii_lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool starts_with_case_insensitive(const std::string &value, const std::string &prefix)
{
    if (prefix.size() > value.size())
        return false;
    for (size_t i = 0; i < prefix.size(); ++i) {
        const auto a = static_cast<unsigned char>(value[i]);
        const auto b = static_cast<unsigned char>(prefix[i]);
        if (std::tolower(a) != std::tolower(b))
            return false;
    }
    return true;
}

} // namespace

std::vector<PresetTextureVariant> discover_input_overlay_texture_variants(const std::string &config_path)
{
    namespace fs = std::filesystem;
    std::vector<PresetTextureVariant> result;
    if (config_path.empty())
        return result;

    const fs::path config = utf8_to_path(config_path);
    const fs::path directory = config.parent_path();
    const std::string config_stem = path_to_utf8(config.stem());
    if (directory.empty())
        return result;

    std::error_code ec;
    if (!fs::is_directory(directory, ec))
        return result;

    for (fs::directory_iterator it(directory, fs::directory_options::skip_permission_denied, ec), end;
         !ec && it != end; it.increment(ec)) {
        const fs::directory_entry &entry = *it;
        if (!entry.is_regular_file(ec)) {
            ec.clear();
            continue;
        }

        const fs::path path = entry.path();
        if (ascii_lower(path_to_utf8(path.extension())) != ".png")
            continue;

        const std::string stem = path_to_utf8(path.stem());
        result.push_back({path_to_utf8(path.filename()), path_to_utf8(path), stem == config_stem});
    }

    std::stable_sort(result.begin(), result.end(), [&](const PresetTextureVariant &a,
                                                        const PresetTextureVariant &b) {
        const std::string a_stem = path_to_utf8(utf8_to_path(a.path).stem());
        const std::string b_stem = path_to_utf8(utf8_to_path(b.path).stem());
        const int a_rank = a.exact_name_match ? 0 : starts_with_case_insensitive(a_stem, config_stem) ? 1 : 2;
        const int b_rank = b.exact_name_match ? 0 : starts_with_case_insensitive(b_stem, config_stem) ? 1 : 2;
        return std::tie(a_rank, a.label) < std::tie(b_rank, b.label);
    });

    return result;
}

} // namespace mugen
