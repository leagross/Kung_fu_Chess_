#include <gtest/gtest.h>

#include "kfc/model/board.hpp"
#include "kfc/io/board_parser.hpp"
#include "kfc/io/parse_error.hpp"

using namespace kfc::model;
using kfc::io::BoardParser;
using kfc::io::ParseError;

TEST(BoardParserTest, AcceptsARectangularBoard) {
    BoardParser parser;
    std::vector<std::string> lines = {"wK . .", ". . .", "wR . bK"};

    Board board = parser.parse(lines);

    EXPECT_EQ(board.width(), 3);
    EXPECT_EQ(board.height(), 3);
}

TEST(BoardParserTest, RejectsInconsistentRowWidth) {
    BoardParser parser;
    std::vector<std::string> lines = {"wK . .", ". bK"};

    try {
        parser.parse(lines);
        FAIL() << "expected ParseError";
    } catch (const ParseError& error) {
        EXPECT_EQ(error.code(), "ROW_WIDTH_MISMATCH");
    }
}

TEST(BoardParserTest, RejectsAnUnknownPieceToken) {
    BoardParser parser;
    std::vector<std::string> lines = {"wK xZ"};

    try {
        parser.parse(lines);
        FAIL() << "expected ParseError";
    } catch (const ParseError& error) {
        EXPECT_EQ(error.code(), "UNKNOWN_TOKEN");
    }
}

TEST(BoardParserTest, RejectsAnEmptyBoard) {
    BoardParser parser;
    std::vector<std::string> lines = {};

    try {
        parser.parse(lines);
        FAIL() << "expected ParseError";
    } catch (const ParseError& error) {
        EXPECT_EQ(error.code(), "EMPTY_BOARD");
    }
}

TEST(BoardParserTest, RejectsABlankLineAsAZeroWidthBoard) {
    BoardParser parser;
    std::vector<std::string> lines = {"   "};  // whitespace only -> zero tokens

    try {
        parser.parse(lines);
        FAIL() << "expected ParseError";
    } catch (const ParseError& error) {
        EXPECT_EQ(error.code(), "EMPTY_BOARD");
    }
}

TEST(BoardParserTest, AssignsSequentialPieceIdsInReadingOrder) {
    BoardParser parser;
    std::vector<std::string> lines = {"wK bR"};

    Board board = parser.parse(lines);

    std::optional<Piece> first = board.piece_at(Position{0, 0});
    std::optional<Piece> second = board.piece_at(Position{0, 1});
    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(first->id, PieceId{1});
    EXPECT_EQ(second->id, PieceId{2});
}

TEST(BoardParserTest, ParsesTheDroneToken) {
    BoardParser parser;
    std::vector<std::string> lines = {"wD ."};

    Board board = parser.parse(lines);

    std::optional<Piece> drone = board.piece_at(Position{0, 0});
    ASSERT_TRUE(drone.has_value());
    EXPECT_EQ(drone->kind, PieceKind::Drone);
    EXPECT_EQ(drone->color, PieceColor::White);
}
