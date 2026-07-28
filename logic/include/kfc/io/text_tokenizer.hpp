#pragma once

#include <string>
#include <vector>

namespace kfc::io {

/// Splits line into whitespace-separated tokens. Shared by BoardParser and
/// CommandProcessor so the splitting logic exists in exactly one place.
std::vector<std::string> tokenize(const std::string& line);

}  // namespace kfc::io
