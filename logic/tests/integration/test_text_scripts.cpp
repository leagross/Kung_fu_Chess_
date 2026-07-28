// Text integration tests: the "visible spec" style mandated by the design
// document (section 13-16) -- each script drives the real command path
// (InputReader -> BoardParser -> Game -> CommandProcessor) exactly as a
// player or the eventual TextTestRunner would, and print board is the only
// verification mechanism. Never touches Board, RuleEngine, or
// RealTimeArbiter directly.
#include <gtest/gtest.h>
#include <sstream>

#include "kfc/io/board_parser.hpp"
#include "kfc/io/parse_error.hpp"
#include "kfc/texttests/command_processor.hpp"
#include "kfc/texttests/game.hpp"
#include "kfc/texttests/input_reader.hpp"

namespace {

std::string run_script(const std::string& fixture) {
    std::istringstream input(fixture);
    kfc::texttests::Sections sections = kfc::texttests::InputReader::read(input);

    kfc::io::BoardParser parser;
    kfc::texttests::Game game(parser.parse(sections.board_lines));

    std::ostringstream out;
    kfc::texttests::CommandProcessor::run(game, sections.command_lines, out);
    return out.str();
}

// Runs a single command line against a fresh trivial game, for the error
// cases that don't need a whole scripted board.
std::string run_command(const std::string& command) {
    kfc::io::BoardParser parser;
    kfc::texttests::Game game(parser.parse({"wR ."}));
    std::ostringstream out;
    kfc::texttests::CommandProcessor::run(game, {command}, out);
    return out.str();
}

}  // namespace

// Mirrors the design document's iteration-5 example exactly: a rook moving
// two cells takes 2000ms, split across two 1000ms waits so the board is
// observed both mid-flight and after arrival.
TEST(TextScriptsTest, PartialWaitLeavesTheBoardUnchangedThenArrivalUpdatesIt) {
    std::string fixture =
        "Board:\n"
        ". wR .\n"
        ". . .\n"
        ". . bK\n"
        "Commands:\n"
        "click 150 50\n"
        "click 150 250\n"
        "wait 1000\n"
        "print board\n"
        "wait 1000\n"
        "print board\n";

    std::string expected =
        ". wR .\n"
        ". . .\n"
        ". . bK\n"
        ". . .\n"
        ". . .\n"
        ". wR bK\n";

    EXPECT_EQ(run_script(fixture), expected);
}

// Mirrors the design document's iteration-6 example exactly: capturing the
// enemy king ends the game, and a further move that would otherwise be
// legal is rejected, leaving the board unchanged.
TEST(TextScriptsTest, CapturingTheKingEndsTheGameAndBlocksFurtherMoves) {
    std::string fixture =
        "Board:\n"
        "wR . bK\n"
        ". . wN\n"
        ". . .\n"
        "Commands:\n"
        "click 50 50\n"
        "click 250 50\n"
        "wait 2000\n"
        "print board\n"
        "click 250 150\n"
        "click 50 250\n"
        "wait 2000\n"
        "print board\n";

    std::string expected =
        ". . wR\n"
        ". . wN\n"
        ". . .\n"
        ". . wR\n"
        ". . wN\n"
        ". . .\n";

    EXPECT_EQ(run_script(fixture), expected);
}

// Mirrors the design document's iteration-8 example: a blocked sliding move
// leaves the board completely unchanged.
TEST(TextScriptsTest, ABlockedSlideLeavesTheBoardUnchanged) {
    std::string fixture =
        "Board:\n"
        "wR wP .\n"
        ". . .\n"
        ". . bK\n"
        "Commands:\n"
        "click 50 50\n"
        "click 250 50\n"
        "wait 3000\n"
        "print board\n";

    std::string expected =
        "wR wP .\n"
        ". . .\n"
        ". . bK\n";

    EXPECT_EQ(run_script(fixture), expected);
}

// Proves the "jump x y" DSL command reaches Controller::jump ->
// GameEngine::request_jump end to end: the piece lands back on its own
// cell once the jump's fixed duration has elapsed.
TEST(TextScriptsTest, AJumpLeavesThePieceOnItsOwnCellAfterLanding) {
    std::string fixture =
        "Board:\n"
        "wR . .\n"
        "Commands:\n"
        "jump 50 50\n"
        "wait 1000\n"
        "print board\n";

    std::string expected = "wR . .\n";

    EXPECT_EQ(run_script(fixture), expected);
}

// Proves promotion end to end: a White pawn arriving on row 0 becomes a
// queen, visible directly in print board's output -- no code outside
// RealTimeArbiter::resolve_arrival needed to change for this to work.
TEST(TextScriptsTest, AWhitePawnReachingTheLastRowIsPrintedAsAQueen) {
    std::string fixture =
        "Board:\n"
        ". . .\n"
        ". wP .\n"
        "Commands:\n"
        "click 150 150\n"
        "click 150 50\n"
        "wait 1000\n"
        "print board\n";

    std::string expected =
        ". wQ .\n"
        ". . .\n";

    EXPECT_EQ(run_script(fixture), expected);
}

// Proves the reselect-on-friendly-piece fix end to end: clicking a second
// piece of the same color replaces the selection, so the *second* click
// pair is what actually requests the move.
TEST(TextScriptsTest, ClickingAnotherFriendlyPieceReplacesSelectionInsteadOfMoving) {
    std::string fixture =
        "Board:\n"
        "wR . wK\n"
        ". . .\n"
        "Commands:\n"
        "click 50 50\n"
        "click 250 50\n"
        "click 250 150\n"
        "wait 1000\n"
        "print board\n";

    std::string expected =
        "wR . .\n"
        ". . wK\n";

    EXPECT_EQ(run_script(fixture), expected);
}

// Proves the has_moved-based double-move rule end to end: once a pawn has
// completed one ordinary move, a later two-cell request is rejected --
// even though nothing about its row or the board's size changed.
TEST(TextScriptsTest, APawnCannotDoubleMoveAfterItHasAlreadyMovedOnce) {
    std::string fixture =
        "Board:\n"
        ". . .\n"
        ". . .\n"
        ". . .\n"
        ". wP .\n"
        "Commands:\n"
        "click 150 350\n"
        "click 150 250\n"
        "wait 1000\n"
        "click 150 250\n"
        "click 150 50\n"
        "wait 1000\n"
        "print board\n";

    std::string expected =
        ". . .\n"
        ". . .\n"
        ". wP .\n"
        ". . .\n";

    EXPECT_EQ(run_script(fixture), expected);
}

// --- CommandProcessor error handling: a bad script fails loudly with a
// stable ParseError code instead of being silently dropped or throwing a raw
// std::stoi exception. ---

TEST(CommandProcessorErrorTest, AMisspelledCommandThrowsUnknownCommand) {
    try {
        run_command("clik 100 200");
        FAIL() << "expected ParseError";
    } catch (const kfc::io::ParseError& error) {
        EXPECT_EQ(error.code(), "UNKNOWN_COMMAND");
    }
}

TEST(CommandProcessorErrorTest, TheWrongArgumentCountThrows) {
    try {
        run_command("click 100");  // click needs two coordinates
        FAIL() << "expected ParseError";
    } catch (const kfc::io::ParseError& error) {
        EXPECT_EQ(error.code(), "INVALID_ARGUMENT_COUNT");
    }
}

TEST(CommandProcessorErrorTest, NonNumericCoordinatesThrowInvalidNumber) {
    try {
        run_command("click abc 20");
        FAIL() << "expected ParseError";
    } catch (const kfc::io::ParseError& error) {
        EXPECT_EQ(error.code(), "INVALID_NUMBER");
    }
}

TEST(CommandProcessorErrorTest, TrailingJunkAfterANumberThrowsInvalidNumber) {
    try {
        run_command("wait 100abc");  // std::stoi would silently accept "100"
        FAIL() << "expected ParseError";
    } catch (const kfc::io::ParseError& error) {
        EXPECT_EQ(error.code(), "INVALID_NUMBER");
    }
}

TEST(CommandProcessorErrorTest, ANegativeWaitThrowsInvalidNumber) {
    try {
        run_command("wait -50");
        FAIL() << "expected ParseError";
    } catch (const kfc::io::ParseError& error) {
        EXPECT_EQ(error.code(), "INVALID_NUMBER");
    }
}
