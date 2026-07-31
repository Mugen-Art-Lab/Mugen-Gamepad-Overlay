// SPDX-FileCopyrightText: 2026 Mugen Art Lab
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <vector>

namespace mugen {

struct PresetTextureVariant {
    std::string label;
    std::string path;
    bool exact_name_match = false;
};

std::vector<PresetTextureVariant> discover_input_overlay_texture_variants(const std::string &config_path);

} // namespace mugen
