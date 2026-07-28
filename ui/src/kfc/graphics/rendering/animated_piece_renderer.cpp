#include "../../../../include/kfc/graphics/rendering/animated_piece_renderer.hpp"

#include <algorithm>
#include <iostream>
#include <optional>

#include "kfc/input/board_mapper.hpp"

namespace kfc::graphics {

namespace {
const cv::Scalar kHourglassColor(180, 220, 255, 150);  // BGR sandy orange, translucent
// Pieces are drawn a bit smaller than one cell (centered on it), leaving a
// visible gap to neighboring pieces instead of crowding/overlapping them --
// a purely visual choice, tune freely.
constexpr double kPieceScaleFactor = 0.95;
}  // namespace

AnimatedPieceRenderer::AnimatedPieceRenderer(bool show_rest_ring) : show_rest_ring_(show_rest_ring) {}

void AnimatedPieceRenderer::draw(const PieceAnimatorRegistry& registry, Img& board_image) {
    int board_width = board_image.get_mat().cols;
    int board_height = board_image.get_mat().rows;
    int piece_size = static_cast<int>(kfc::input::kCellSizePixels * kPieceScaleFactor);
    int centering_offset = (piece_size - kfc::input::kCellSizePixels) / 2;

    for (const auto& [id, animator] : registry.animators()) {
        try {
            std::string sprite_path = animator.current_sprite_path().string();

            auto cached = sprite_cache_.find(sprite_path);
            if (cached == sprite_cache_.end()) {
                Img sprite;
                sprite.read(sprite_path, {piece_size, piece_size});
                cached = sprite_cache_.emplace(sprite_path, std::move(sprite)).first;
            }

            PixelPoint position = animator.pixel_position();
            // Centered on the cell, a bit larger than it -- clamped so an
            // edge-row/column piece's enlarged sprite never spills past the
            // board's own bounds, which draw_on would otherwise reject.
            int draw_x = std::clamp(position.x - centering_offset, 0, board_width - piece_size);
            int draw_y = std::clamp(position.y - centering_offset, 0, board_height - piece_size);
            cached->second.draw_on(board_image, draw_x, draw_y);

            if (show_rest_ring_) {
                // The "hourglass": a translucent overlay draining from the
                // top of the piece's cell while it rests after a move,
                // drawn last so it sits on top of the sprite.
                std::optional<double> remaining = animator.rest_remaining_fraction();
                if (remaining.has_value()) {
                    board_image.draw_hourglass_overlay(position.x, position.y, kfc::input::kCellSizePixels,
                                                         *remaining, kHourglassColor);
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Skipping draw for piece id " << id.value << ": " << e.what() << "\n";
        }
    }
}

}  // namespace kfc::graphics
