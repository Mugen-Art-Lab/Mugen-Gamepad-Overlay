#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace mugen {

bool rasterize_svg_file(const std::string &path, uint32_t max_dimension,
                        uint32_t &width, uint32_t &height,
                        std::vector<uint8_t> &rgba, std::string &error);

} // namespace mugen
