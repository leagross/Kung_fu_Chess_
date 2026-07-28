#include "../../../include/kfc/io/text_tokenizer.hpp"

#include <sstream>

namespace kfc::io {

std::vector<std::string> tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    std::istringstream stream(line);
    std::string token;
    while (stream >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

}  // namespace kfc::io
