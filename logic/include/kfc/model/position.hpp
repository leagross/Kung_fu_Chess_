#pragma once

#include <ostream>
#include <string>

namespace kfc::model {

/// Board cell by row/column; bounds checking is Board's responsibility, not Position's.
struct Position {
    int row;
    int col;
};

inline bool operator==(const Position& lhs, const Position& rhs) {
    return lhs.row == rhs.row && lhs.col == rhs.col;
}

inline bool operator!=(const Position& lhs, const Position& rhs) {
    return !(lhs == rhs);
}

/// e.g. "(2,3)".
inline std::string to_string(const Position& pos) {
    return "(" + std::to_string(pos.row) + "," + std::to_string(pos.col) + ")";
}

inline std::ostream& operator<<(std::ostream& os, const Position& pos) {
    return os << to_string(pos);
}

}  // namespace kfc::model
