#include <gtest/gtest.h>
#include "kfc/model/position.hpp"

using kfc::model::Position;

TEST(PositionTest, EqualPositionsWithSameRowAndCol) {
    Position a{2, 3};
    Position b{2, 3};
    EXPECT_EQ(a, b);
}

TEST(PositionTest, DifferentRowMakesPositionsUnequal) {
    Position a{2, 3};
    Position b{5, 3};
    EXPECT_NE(a, b);
}

TEST(PositionTest, DifferentColMakesPositionsUnequal) {
    Position a{2, 3};
    Position b{2, 7};
    EXPECT_NE(a, b);
}

TEST(PositionTest, ReadableRepresentationIncludesRowAndCol) {
    Position pos{2, 3};
    EXPECT_EQ(to_string(pos), "(2,3)");
}
