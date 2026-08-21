#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace kfc::graphics {

/// Reads a board-layout file into the line-per-rank form
/// kfc::io::BoardParser::parse expects. Throws std::runtime_error if path
/// can't be opened.
std::vector<std::string> read_board_lines(const std::filesystem::path& path);

}  // namespace kfc::graphics
