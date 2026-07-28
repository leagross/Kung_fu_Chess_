#pragma once

#include <stdexcept>
#include <string>

namespace kfc::io {

/// Thrown by BoardParser::parse on invalid input. code() is a stable,
/// machine-readable identifier (e.g. "UNKNOWN_TOKEN") -- callers decide how
/// to present it (main.cpp prints "ERROR " + code).
class ParseError : public std::runtime_error {
public:
    explicit ParseError(std::string code);

    const std::string& code() const;

private:
    std::string code_;
};

}  // namespace kfc::io
