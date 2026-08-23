#pragma once

#include <stdexcept>
#include <string>

namespace kfc::io {

/// Thrown by BoardParser::parse; code() is a stable machine-readable identifier.
class ParseError : public std::runtime_error {
public:
    explicit ParseError(std::string code);

    const std::string& code() const;

private:
    std::string code_;
};

}  // namespace kfc::io
