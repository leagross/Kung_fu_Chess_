#include <gtest/gtest.h>

#include "kfc/model/board.hpp"
#include "kfc/io/board_parser.hpp"
#include "kfc/io/board_printer.hpp"

using namespace kfc::model;
using kfc::io::BoardParser;
using kfc::io::BoardPrinter;

TEST(BoardPrinterTest, RoundTripsASimpleBoard) {
    BoardParser parser;
    BoardPrinter printer;
    std::vector<std::string> lines = {"wK . bK", ". . .", "wR . bR"};
    Board board = parser.parse(lines);

    std::string printed = printer.print(board);

    EXPECT_EQ(printed, "wK . bK\n. . .\nwR . bR\n");
}

TEST(BoardPrinterTest, PrintsEmptyCellsAsDots) {
    Board board(2, 1);

    std::string printed = BoardPrinter{}.print(board);

    EXPECT_EQ(printed, ". .\n");
}

TEST(BoardPrinterTest, PrintsTheDroneToken) {
    Board board(1, 1);
    board.add_piece(Piece{PieceId{1}, PieceColor::Black, PieceKind::Drone, Position{0, 0}, PieceState::Idle});

    std::string printed = BoardPrinter{}.print(board);

    EXPECT_EQ(printed, "bD\n");
}
