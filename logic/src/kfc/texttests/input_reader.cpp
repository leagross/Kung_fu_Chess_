#include "../../../include/kfc/texttests/input_reader.hpp"

#include <algorithm>
#include <istream>
#include <string>

#include "../../../include/kfc/io/parse_error.hpp"

using kfc::io::ParseError;

namespace kfc::texttests {

namespace {

bool is_blank(const std::string& line) {
    return line.find_first_not_of(" \t\r\n") == std::string::npos;
}

/// Leading/trailing whitespace removed -- so a section marker written as
/// " Board: " still matches "Board:" instead of being read as board content.
std::string trim(const std::string& line) {
    std::size_t first = line.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    std::size_t last = line.find_last_not_of(" \t\r\n");
    return line.substr(first, last - first + 1);
}

std::vector<std::string> split_non_blank_lines(std::istream& input) {
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (!is_blank(line)) {
            lines.push_back(line);
        }
    }
    return lines;
}

}  // namespace

Sections InputReader::read(std::istream& input) {
    std::vector<std::string> lines = split_non_blank_lines(input);

    auto is_marker = [](const std::string& target) {
        return [target](const std::string& line) { return trim(line) == target; };
    };
    auto board_it = std::find_if(lines.begin(), lines.end(), is_marker("Board:"));
    if (board_it == lines.end()) {
        throw ParseError("MISSING_BOARD_SECTION");
    }
    auto commands_it = std::find_if(board_it, lines.end(), is_marker("Commands:"));
    if (commands_it == lines.end()) {
        throw ParseError("MISSING_COMMANDS_SECTION");
    }

    return Sections{std::vector<std::string>(board_it + 1, commands_it),
                     std::vector<std::string>(commands_it + 1, lines.end())};
}

}  // namespace kfc::texttests
