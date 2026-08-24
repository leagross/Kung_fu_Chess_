#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace kfc::graphics {

/// Line-per-rank form kfc::io::BoardParser::parse expects.
std::vector<std::string> read_board_lines(const std::filesystem::path& path);

}  // namespace kfc::graphics
