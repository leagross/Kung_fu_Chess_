#pragma once

#include <ostream>
#include <string>
#include <vector>

#include "../../kfc/texttests/game.hpp"

namespace kfc::texttests {

/// Executes DSL command lines against a Game: click/jump/wait/print board.
class CommandProcessor {
public:
    static void run(Game& game, const std::vector<std::string>& command_lines, std::ostream& out);
};

}  // namespace kfc::texttests
