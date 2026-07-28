#include <gtest/gtest.h>

#include "kfc/input/board_mapper.hpp"

using kfc::input::BoardMapper;
using kfc::model::Position;

TEST(BoardMapperTest, XZeroToNinetyNineMapsToColumnZero) {
    BoardMapper mapper(8, 8);

    std::optional<Position> cell = mapper.pixel_to_cell(50, 0);

    ASSERT_TRUE(cell.has_value());
    EXPECT_EQ(cell->col, 0);
}

TEST(BoardMapperTest, XOneHundredToOneNinetyNineMapsToColumnOne) {
    BoardMapper mapper(8, 8);

    std::optional<Position> cell = mapper.pixel_to_cell(150, 0);

    ASSERT_TRUE(cell.has_value());
    EXPECT_EQ(cell->col, 1);
}

TEST(BoardMapperTest, YOneHundredToOneNinetyNineMapsToRowOne) {
    BoardMapper mapper(8, 8);

    std::optional<Position> cell = mapper.pixel_to_cell(0, 150);

    ASSERT_TRUE(cell.has_value());
    EXPECT_EQ(cell->row, 1);
}

TEST(BoardMapperTest, ClickOutsideTheBoardIsRejected) {
    BoardMapper mapper(3, 3);

    EXPECT_FALSE(mapper.pixel_to_cell(300, 0).has_value());
    EXPECT_FALSE(mapper.pixel_to_cell(0, 300).has_value());
    EXPECT_FALSE(mapper.pixel_to_cell(-1, 0).has_value());
    EXPECT_FALSE(mapper.pixel_to_cell(0, -1).has_value());
}
