// The real playable entry point: reads a "Board:"/"Commands:" fixture from
// stdin and drives it through the same command path a click-driven UI would
// use (InputReader -> BoardParser -> Game -> CommandProcessor), writing only
// print-board output to stdout.
#include <iostream>

#include "../../include/kfc/io/board_parser.hpp"
#include "../../include/kfc/io/parse_error.hpp"
#include "../../include/kfc/texttests/command_processor.hpp"
#include "../../include/kfc/texttests/game.hpp"
#include "../../include/kfc/texttests/input_reader.hpp"

int main() {
    std::ios::sync_with_stdio(false);

    try {
        kfc::texttests::Sections sections = kfc::texttests::InputReader::read(std::cin);
        kfc::io::BoardParser parser;
        kfc::model::Board board = parser.parse(sections.board_lines);
        kfc::texttests::Game game(std::move(board));
        kfc::texttests::CommandProcessor::run(game, sections.command_lines, std::cout);
    } catch (const kfc::io::ParseError& error) {
        std::cout << "ERROR " << error.code() << "\n";
        return 0;
    }

    return 0;
}
