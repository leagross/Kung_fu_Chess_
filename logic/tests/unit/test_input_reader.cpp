#include <gtest/gtest.h>
#include <sstream>

#include "kfc/io/parse_error.hpp"
#include "kfc/texttests/input_reader.hpp"

using kfc::io::ParseError;
using kfc::texttests::InputReader;
using kfc::texttests::Sections;

TEST(InputReaderTest, SplitsBoardAndCommandLinesAndDropsBlankLines) {
    std::istringstream input(
        "Board:\n"
        "wK . .\n"
        "\n"
        ". . bK\n"
        "Commands:\n"
        "\n"
        "print board\n");

    Sections sections = InputReader::read(input);

    ASSERT_EQ(sections.board_lines.size(), 2u);
    EXPECT_EQ(sections.board_lines[0], "wK . .");
    EXPECT_EQ(sections.board_lines[1], ". . bK");
    ASSERT_EQ(sections.command_lines.size(), 1u);
    EXPECT_EQ(sections.command_lines[0], "print board");
}

TEST(InputReaderTest, SectionMarkersAreMatchedIgnoringSurroundingWhitespace) {
    std::istringstream input(
        "  Board:  \n"
        "wK . .\n"
        "\tCommands:\n"
        "print board\n");

    Sections sections = InputReader::read(input);

    ASSERT_EQ(sections.board_lines.size(), 1u);
    EXPECT_EQ(sections.board_lines[0], "wK . .");
    ASSERT_EQ(sections.command_lines.size(), 1u);
    EXPECT_EQ(sections.command_lines[0], "print board");
}

TEST(InputReaderTest, RejectsInputMissingTheBoardMarker) {
    std::istringstream input("wK . .\nCommands:\nprint board\n");

    try {
        InputReader::read(input);
        FAIL() << "expected ParseError";
    } catch (const ParseError& error) {
        EXPECT_EQ(error.code(), "MISSING_BOARD_SECTION");
    }
}

TEST(InputReaderTest, RejectsInputMissingTheCommandsMarker) {
    std::istringstream input("Board:\nwK . .\nprint board\n");

    try {
        InputReader::read(input);
        FAIL() << "expected ParseError";
    } catch (const ParseError& error) {
        EXPECT_EQ(error.code(), "MISSING_COMMANDS_SECTION");
    }
}
