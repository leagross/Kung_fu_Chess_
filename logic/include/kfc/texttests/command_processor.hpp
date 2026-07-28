#pragma once

#include <ostream>
#include <string>
#include <vector>

#include "../../kfc/texttests/game.hpp"

namespace kfc::texttests {

/// Executes DSL command lines against a Game: "click x y" -> Game::click,
/// "jump x y" -> Game::jump, "wait ms" -> Game::wait, "print board" ->
/// Game::print_board written to out. Never touches Board, RuleEngine, or
/// RealTimeArbiter directly -- it only calls Game's public entry points.
/// Any line that isn't one of the four recognized commands is skipped.
class CommandProcessor {
public:
    static void run(Game& game, const std::vector<std::string>& command_lines, std::ostream& out);
};

}  // namespace kfc::texttests
