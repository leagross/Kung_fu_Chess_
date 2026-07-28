#include "../../../../include/kfc/graphics/io/board_file_loader.hpp"

#include <fstream>
#include <stdexcept>

namespace kfc::graphics {

std::vector<std::string> read_board_lines(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("Cannot open board file: " + path.string());
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty()) {
            lines.push_back(line);
        }
    }
    return lines;
}

}  // namespace kfc::graphics
