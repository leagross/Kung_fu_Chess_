#pragma once

#include <ostream>
#include <string>

namespace kfc::model {

/// A single board cell, identified by row and column. Carries no knowledge
/// of board size -- bounds checking is Board's responsibility, not Position's.

struct Position{
    int row;
    int col;        
};

/// True when both positions refer to the same row and column.
inline bool operator==(const Position& lhs, const Position& rhs) {
    return lhs.row == rhs.row && lhs.col == rhs.col;
}

/// True when the positions differ in row or column.
inline bool operator!=(const Position& lhs, const Position& rhs) {
    return !(lhs == rhs);
}

/// Human-readable form, e.g. "(2,3)". Used in test failure messages and logs.
inline std::string to_string(const Position& pos) {
    return "(" + std::to_string(pos.row) + "," + std::to_string(pos.col) + ")";
}

/// Lets Position be streamed directly, e.g. into GoogleTest failure output.
inline std::ostream& operator<<(std::ostream& os, const Position& pos) {
    return os << to_string(pos);
}

}  // namespace kfc::model
