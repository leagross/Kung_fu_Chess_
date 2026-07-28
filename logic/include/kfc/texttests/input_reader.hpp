#pragma once

#include <istream>
#include <string>
#include <vector>

namespace kfc::texttests {

/// The two halves of a fixture: the board-grid lines BoardParser consumes,
/// and the DSL command lines (click/wait/print board) CommandProcessor
/// consumes. Blank lines are dropped wherever they appear.
struct Sections {
    std::vector<std::string> board_lines;
    std::vector<std::string> command_lines;
};

/// Splits a raw fixture stream into its "Board:" and "Commands:" sections.
class InputReader {
public:
    /// Throws kfc::io::ParseError("MISSING_BOARD_SECTION") or
    /// ParseError("MISSING_COMMANDS_SECTION") if either marker is absent.
    static Sections read(std::istream& input);
};

}  // namespace kfc::texttests
