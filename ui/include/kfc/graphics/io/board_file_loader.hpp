#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace kfc::graphics {

/// Reads a board-layout file into the line-per-rank form
/// kfc::io::BoardParser::parse expects. Throws std::runtime_error if path
/// can't be opened. A free function, not a class: it holds no state between
/// calls, so there is nothing a class would give it beyond a longer name --
/// the same reasoning that kept cell_top_left a free function.
std::vector<std::string> read_board_lines(const std::filesystem::path& path);

}  // namespace kfc::graphics
