#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <variant>
#include <vector>

#include <opencv2/opencv.hpp>

#include "../../include/kfc/graphics/audio/sound_player_factory.hpp"
#include "../../include/kfc/graphics/dialogs/room_prompt.hpp"
#include "../../include/kfc/graphics/platform/screen_metrics.hpp"

#include "../../include/kfc/graphics/animation/piece_animator_registry.hpp"
#include "../../include/kfc/graphics/animation/piece_asset_library.hpp"
#include "../../include/kfc/graphics/app/game_session.hpp"
#include "../../include/kfc/graphics/app/home_screen.hpp"
#include "../../include/kfc/graphics/app/match_overlay.hpp"
#include "../../include/kfc/graphics/assets/piece_code_scheme.hpp"
#include "../../include/kfc/graphics/constants.hpp"
#include "../../include/kfc/graphics/input/mouse_input_adapter.hpp"
#include "../../include/kfc/graphics/input/screen_mapper.hpp"
#include "../../include/kfc/graphics/primitives/img.hpp"
#include "../../include/kfc/graphics/rendering/animated_piece_renderer.hpp"
#include "../../include/kfc/graphics/rendering/board_layout.hpp"
#include "../../include/kfc/graphics/rendering/game_over_banner.hpp"
#include "../../include/kfc/graphics/rendering/hud_renderer.hpp"
#include "kfc/audio/sound_board.hpp"
#include "kfc/events/game_events.hpp"
#include "kfc/input/board_mapper.hpp"
#include "kfc/protocol/messages.hpp"
#include "kfc/realtime/move_log_observer.hpp"
#include "kfc/realtime/score_observer.hpp"
#include "kfc/texttests/game_view.hpp"

int main(int argc, char** argv) {
    try {
        kfc::graphics::PiecesMineCodeScheme code_scheme;
        kfc::graphics::app::GameSession session(argc, argv);
        std::unique_ptr<kfc::graphics::dialogs::IRoomPrompt> prompt = kfc::graphics::dialogs::make_room_prompt();
        constexpr int kIntroDurationMs = kfc::graphics::app::kDefaultIntroDurationMs;

        int board_pixel_width = session.board_width() * kfc::input::kCellSizePixels;
        int board_pixel_height = session.board_height() * kfc::input::kCellSizePixels;

        kfc::graphics::BoardLayout layout = kfc::graphics::compute_board_layout(board_pixel_width, board_pixel_height);
        int framed_board_width = layout.framed_board_width;
        int framed_board_height = layout.framed_board_height;
        int framed_board_inset_x = layout.framed_board_inset_x;
        int framed_board_inset_y = layout.framed_board_inset_y;
        int board_column_width = layout.board_column_width;
        int board_column_height = layout.board_column_height;
        int canvas_width = layout.canvas_width;
        int canvas_height = layout.canvas_height;
        int framed_board_x = layout.framed_board_x;
        int framed_board_y = layout.framed_board_y;
        int grid_offset_x = layout.grid_offset_x;
        int grid_offset_y = layout.grid_offset_y;

        std::filesystem::path board_path =
            kfc::graphics::assets_root() / kfc::graphics::kDefaultAssetPackName / kfc::graphics::kBoardImageFilename;
        kfc::graphics::Img framed_board_texture;
        framed_board_texture.read(board_path.string(), {framed_board_width, framed_board_height});
        // The playable grid, cropped back out of the scaled framed board, so
        // everything downstream draws unaware the source image has a frame.
        kfc::graphics::Img board_texture =
            framed_board_texture.cropped(framed_board_inset_x, framed_board_inset_y, board_pixel_width,
                                          board_pixel_height);

        // background.png read once at native resolution; every "cover"-scale
        // use (canvas below, and per-frame window) scales fresh from this
        // source, since scaling an already-cropped result would compound
        // the crop and show less of the scene than the target allows.
        std::filesystem::path background_path =
            kfc::graphics::assets_root() / kfc::graphics::kDefaultAssetPackName / "background.png";
        kfc::graphics::Img background_source;
        background_source.read(background_path.string());

        kfc::graphics::Img background_texture = background_source.cover_scaled(canvas_width, canvas_height);

        // board.png carries residual near-255 alpha noise even where meant
        // fully opaque, which would force draw_on's expensive per-pixel
        // blend path for no benefit; force_opaque avoids that. board_texture
        // itself is left alone since pieces need its real alpha to blend.
        // background_texture stays 4-channel so HudRenderer's panels can
        // genuinely alpha-blend onto it (see canvas.force_opaque() below).
        framed_board_texture.force_opaque();

        // Background + frame never change frame-to-frame; compose once
        // instead of redrawing the large framed board every frame.
        kfc::graphics::Img static_backdrop = background_texture.clone();
        framed_board_texture.draw_on(static_backdrop, framed_board_x, framed_board_y);

        kfc::graphics::PieceAssetLibrary asset_library(code_scheme);
        kfc::graphics::PieceAnimatorRegistry animator_registry(asset_library);
        kfc::graphics::AnimatedPieceRenderer piece_renderer(/*show_rest_ring=*/true);
        kfc::graphics::HudRenderer hud_renderer;

        // The canvas's logical resolution can exceed the actual screen; this
        // is only the window's starting size, since it's resizable below.
        kfc::graphics::platform::ScreenSize screen = kfc::graphics::platform::prepare_display_and_measure_screen();
        int screen_width = screen.width;
        int screen_height = screen.height;
        constexpr double kScreenFitFraction = 0.93;  // leave a little room for the title bar/taskbar
        double display_scale = std::min({1.0, kScreenFitFraction * screen_width / canvas_width,
                                          kScreenFitFraction * screen_height / canvas_height});
        int display_width = static_cast<int>(std::lround(canvas_width * display_scale));
        int display_height = static_cast<int>(std::lround(canvas_height * display_scale));

        const std::string window_name = "Image";
        // WINDOW_NORMAL: user can drag-resize; content is rescaled per-frame
        // and clicks mapped back via ScreenMapper (see render loop below).
        cv::namedWindow(window_name, cv::WINDOW_NORMAL);
        cv::resizeWindow(window_name, display_width, display_height);

        kfc::graphics::ScreenMapper screen_mapper(window_name, canvas_width, canvas_height);

        std::string room_name;  // shown on-screen during a named-room game
        if (session.is_networked()) {
            // Loops rather than exits on a refused join, so a mistyped room
            // name doesn't force relaunching the whole app.
            while (true) {
                std::optional<kfc::protocol::ClientMessage> action =
                    kfc::graphics::app::run_home_screen(window_name, background_source, *prompt);
                if (!action.has_value()) {
                    cv::destroyAllWindows();
                    return 0;  // window closed on the home screen
                }
                if (session.connect(*action)) {
                    room_name = session.room_name();
                    break;
                }
                std::string message = session.join_failure_message();
                prompt->show_message("Kung Fu Chess", message);
            }
        }

        kfc::texttests::IGameView* game_view = &session.view();

        kfc::model::MoveLogObserver move_log(game_view->board().height());
        kfc::model::ScoreObserver score(session.value_provider());
        // Wired here, before the render loop, on this one thread (the bus is
        // not internally synchronized).
        game_view->events().subscribe<kfc::model::ArrivalEvent>(
            [&move_log](const kfc::model::ArrivalEvent& event) { move_log.on_arrival(event); });
        game_view->events().subscribe<kfc::model::ArrivalEvent>(
            [&score](const kfc::model::ArrivalEvent& event) { score.on_arrival(event); });

        // Fed straight into the observers rather than published on the bus,
        // since these arrivals already happened and shouldn't replay sounds
        // or animations.
        for (const kfc::model::ArrivalEvent& past : session.history()) {
            move_log.on_arrival(past);
            score.on_arrival(past);
        }

        kfc::graphics::app::MatchOverlay overlay(game_view->events(), kIntroDurationMs);

        std::unique_ptr<kfc::audio::ISoundPlayer> sound_player =
            kfc::graphics::audio::make_sound_player(kfc::graphics::assets_root() / "sounds");
        kfc::audio::SoundBoard sound_board(game_view->events(), *sound_player);

        // A spectator watches without playing, so no mouse input is wired up.
        bool spectating = session.is_spectator();
        std::optional<kfc::graphics::MouseInputAdapter> mouse_adapter;
        if (!spectating) {
            mouse_adapter.emplace(window_name, *game_view, screen_mapper, grid_offset_x, grid_offset_y);
        }

        constexpr int kSearchTimeoutMs = 60000;
        auto search_started_at = std::chrono::steady_clock::now();
        // Message box is shown after the loop, once the seat is given back.
        bool search_timed_out = false;

        std::cout << "Click a piece, then click its destination. Press any key to quit.\n";

        // Cached by window size, since it only changes on an actual resize.
        kfc::graphics::Img cached_window_background;
        int cached_window_background_width = -1;
        int cached_window_background_height = -1;

        auto last_frame_at = std::chrono::steady_clock::now();
        while (true) {
            auto now = std::chrono::steady_clock::now();
            int elapsed_ms = static_cast<int>(
                std::chrono::duration_cast<std::chrono::milliseconds>(now - last_frame_at).count());
            last_frame_at = now;
            // Real delta time keeps the simulated clock tracking real time
            // regardless of render cost; clamped so a paused/dragged window
            // never dumps a multi-second jump into the simulation.
            elapsed_ms = std::clamp(elapsed_ms, 1, 100);

            game_view->wait(elapsed_ms);
            animator_registry.advance(elapsed_ms, *game_view);

            bool searching = session.is_networked() && !session.is_match_started();
            if (searching &&
                std::chrono::duration_cast<std::chrono::milliseconds>(now - search_started_at).count() >
                    kSearchTimeoutMs) {
                search_timed_out = true;
                break;
            }

            kfc::graphics::Img board_frame = board_texture.clone();
            piece_renderer.draw(animator_registry, board_frame);
            switch (overlay.current(searching, now)) {
                case kfc::graphics::app::Overlay::Searching:
                    kfc::graphics::draw_searching_banner(board_pixel_width, board_pixel_height, board_frame);
                    break;
                case kfc::graphics::app::Overlay::GameOver:
                    kfc::graphics::draw_game_over_banner(overlay.winner(), board_pixel_width, board_pixel_height,
                                                          board_frame);
                    break;
                case kfc::graphics::app::Overlay::Countdown:
                    kfc::graphics::draw_countdown_banner(overlay.countdown_seconds(), board_pixel_width,
                                                          board_pixel_height, board_frame);
                    break;
                case kfc::graphics::app::Overlay::Intro:
                    kfc::graphics::draw_intro_banner(board_pixel_width, board_pixel_height,
                                                      overlay.intro_opacity(now), board_frame);
                    break;
                case kfc::graphics::app::Overlay::None:
                    break;
            }

            kfc::graphics::Img canvas = static_backdrop.clone();
            board_frame.draw_on(canvas, grid_offset_x, grid_offset_y);
            hud_renderer.draw(move_log, score, board_column_width, board_column_height, canvas);
            if (!room_name.empty()) {
                std::string label = (spectating ? "WATCHING ROOM: " : "ROOM: ") + room_name;
                int baseline = 0;
                cv::Size ts = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 1.0, 2, &baseline);
                canvas.put_text(label, (canvas_width - ts.width) / 2, 36, 1.0, cv::Scalar(255, 255, 255, 255), 2);
            }
            // Flattens the HUD panels' translucency so the larger per-frame
            // resize/composite below takes draw_on's cheap copyTo path.
            canvas.force_opaque();

            // Picked up fresh every frame so resizing the window takes
            // effect immediately. Background covers the window completely
            // (cropped if needed); game content is scaled uniformly and
            // centered on top. ScreenMapper's click mapping mirrors the
            // content placement, not the background's.
            cv::Rect current_window_rect = cv::getWindowImageRect(window_name);
            int current_window_width = current_window_rect.width > 0 ? current_window_rect.width : display_width;
            int current_window_height =
                current_window_rect.height > 0 ? current_window_rect.height : display_height;

            // Scaled fresh from background_source (not background_texture,
            // already cropped to the canvas's aspect); only redone when the
            // window size actually changed.
            if (current_window_width != cached_window_background_width ||
                current_window_height != cached_window_background_height) {
                cached_window_background = background_source.cover_scaled(current_window_width, current_window_height);
                cached_window_background_width = current_window_width;
                cached_window_background_height = current_window_height;
            }
            kfc::graphics::Img window_frame = cached_window_background.clone();

            double content_fit_scale = std::min(static_cast<double>(current_window_width) / canvas_width,
                                                 static_cast<double>(current_window_height) / canvas_height);
            int content_width =
                std::min(current_window_width, static_cast<int>(std::lround(canvas_width * content_fit_scale)));
            int content_height =
                std::min(current_window_height, static_cast<int>(std::lround(canvas_height * content_fit_scale)));
            int content_x = std::max(0, (current_window_width - content_width) / 2);
            int content_y = std::max(0, (current_window_height - content_height) / 2);
            canvas.resized(content_width, content_height).draw_on(window_frame, content_x, content_y);

            cv::imshow(window_name, window_frame.get_mat());
            if (cv::waitKey(1) >= 0) {
                break;
            }
            // Closing via the X button sends no key; without this the loop
            // would spin on a destroyed window and never close the socket.
            if (cv::getWindowProperty(window_name, cv::WND_PROP_VISIBLE) < 1.0) {
                break;
            }

            // Caps this process's own frame rate so it doesn't burn a full
            // CPU core; harmless to simulation timing since elapsed_ms is
            // measured from real wall-clock time regardless.
            constexpr std::chrono::milliseconds kTargetFrameDuration{16};  // ~60 FPS
            auto frame_duration = std::chrono::steady_clock::now() - now;
            if (frame_duration < kTargetFrameDuration) {
                std::this_thread::sleep_for(kTargetFrameDuration - frame_duration);
            }
        }

        cv::destroyAllWindows();

        // Hand the seat back before showing the (blocking) message box, or
        // the next player to press Play gets matched into an empty room.
        if (search_timed_out) {
            session.disconnect();
            prompt->show_message("Kung Fu Chess", "No opponent found within a minute. Please try again later.");
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
