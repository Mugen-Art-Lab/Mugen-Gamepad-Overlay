// SPDX-FileCopyrightText: 2026 Mugen Art Lab
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace mugen {

// Decode a local Windows image file into *straight* 32-bit RGBA pixels,
// independently from OBS's version-specific graphics/image-file ABI.
// Outputs are modified only on successful decode.
bool decode_image_file_rgba_wic(const std::string &utf8_path, uint32_t max_dimension,
                                uint64_t max_pixel_bytes, uint32_t &width,
                                uint32_t &height, std::vector<uint8_t> &rgba,
                                std::string &error);

} // namespace mugen
