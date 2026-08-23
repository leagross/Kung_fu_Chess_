#pragma once

#include <istream>
#include <string>
#include <vector>

namespace kfc::texttests {

/// Blank lines are dropped wherever they appear.
struct Sections {
    std::vector<std::string> board_lines;
    std::vector<std::string> command_lines;
};

/// Splits a raw fixture stream into its "Board:" and "Commands:" sections.
class InputReader {
public:
    /// Throws ParseError("MISSING_BOARD_SECTION"/"MISSING_COMMANDS_SECTION") if a marker is absent.
    static Sections read(std::istream& input);
};

}  // namespace kfc::texttests
