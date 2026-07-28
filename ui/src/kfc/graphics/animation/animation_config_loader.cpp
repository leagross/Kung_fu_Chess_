#include "../../../../include/kfc/graphics/animation/animation_config_loader.hpp"

#include <fstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include "kfc/graphics/constants.hpp"

namespace kfc::graphics {

namespace {

int count_sprite_frames(const std::filesystem::path& sprites_dir) {
    if (!std::filesystem::exists(sprites_dir)) {
        throw std::runtime_error("Missing sprites folder: " + sprites_dir.string());
    }
    return static_cast<int>(
        std::distance(std::filesystem::directory_iterator(sprites_dir), std::filesystem::directory_iterator{}));
}

AnimationClip load_clip(const std::filesystem::path& state_dir) {
    std::filesystem::path config_path = state_dir / kStateConfigFilename;
    std::ifstream file(config_path);
    if (!file) {
        throw std::runtime_error("Cannot open state config: " + config_path.string());
    }

    nlohmann::json config;
    file >> config;

    std::string next_state_text = config.at("physics").at("next_state_when_finished").get<std::string>();
    std::optional<PieceStateName> next_state = parse_piece_state_name(next_state_text);
    if (!next_state.has_value()) {
        throw std::runtime_error("Unknown next_state_when_finished '" + next_state_text + "' in " +
                                  config_path.string());
    }

    std::filesystem::path sprites_dir = state_dir / kSpritesFolderName;
    return AnimationClip{
        sprites_dir,
        count_sprite_frames(sprites_dir),
        config.at("graphics").at("frames_per_sec").get<int>(),
        config.at("graphics").at("is_loop").get<bool>(),
        *next_state,
    };
}

}  // namespace

PieceAnimationSet AnimationConfigLoader::load(const std::filesystem::path& piece_folder) const {
    std::unordered_map<PieceStateName, AnimationClip> clips;
    for (PieceStateName state : kAllPieceStateNames) {
        std::filesystem::path state_dir = piece_folder / kStatesFolderName / piece_state_name_folder(state);
        clips.emplace(state, load_clip(state_dir));
    }
    return PieceAnimationSet(std::move(clips));
}

}  // namespace kfc::graphics
