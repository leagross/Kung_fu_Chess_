#include "../../../include/kfc/io/parse_error.hpp"

#include <utility>

namespace kfc::io {

ParseError::ParseError(std::string code) : std::runtime_error(code), code_(std::move(code)) {}

const std::string& ParseError::code() const {
    return code_;
}

}  // namespace kfc::io
