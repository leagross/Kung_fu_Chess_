#include "../../../../include/kfc/graphics/animation/piece_state_name.hpp"

#include <unordered_map>

namespace kfc::graphics {

namespace {
const std::unordered_map<PieceStateName, std::string> kFolderNames = {
    {PieceStateName::Idle, "idle"},
    {PieceStateName::Move, "move"},
    {PieceStateName::Jump, "jump"},
    {PieceStateName::ShortRest, "short_rest"},
    {PieceStateName::LongRest, "long_rest"},
};
}  // namespace

std::string piece_state_name_folder(PieceStateName state) {
    return kFolderNames.at(state);
}

std::optional<PieceStateName> parse_piece_state_name(const std::string& name) {
    for (const auto& [state, folder] : kFolderNames) {
        if (folder == name) {
            return state;
        }
    }
    return std::nullopt;
}

}  // namespace kfc::graphics
