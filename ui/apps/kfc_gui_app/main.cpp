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

namespace {

/// Everything drawn once at startup and reused every frame: the composed
/// background+frame backdrop, the bare board (pieces draw on top of it), and
/// the layout numbers later stages need to place things on the canvas.
struct Scene {
    kfc::graphics::Img board_texture;
    kfc::graphics::Img static_backdrop;
    kfc::graphics::Img background_source;
    int canvas_width;
    int canvas_height;
    int grid_offset_x;
    int grid_offset_y;
    int board_column_width;
    int board_column_height;
};

Scene build_scene(int board_pixel_width, int board_pixel_height) {
    kfc::graphics::BoardLayout layout = kfc::graphics::compute_board_layout(board_pixel_width, board_pixel_height);

    std::filesystem::path board_path =
        kfc::graphics::assets_root() / kfc::graphics::kDefaultAssetPackName / kfc::graphics::kBoardImageFilename;
    kfc::graphics::Img framed_board_texture;
    framed_board_texture.read(board_path.string(), {layout.framed_board_width, layout.framed_board_height});
    // The playable grid, cropped back out of the scaled framed board, so
    // everything downstream draws unaware the source image has a frame.
    kfc::graphics::Img board_texture = framed_board_texture.cropped(
        layout.framed_board_inset_x, layout.framed_board_inset_y, board_pixel_width, board_pixel_height);

    // background.png read once at native resolution; every "cover"-scale use
    // (canvas below, and per-frame window) scales fresh from this source,
    // since scaling an already-cropped result would compound the crop and
    // show less of the scene than the target allows.
    std::filesystem::path background_path =
        kfc::graphics::assets_root() / kfc::graphics::kDefaultAssetPackName / "background.png";
    kfc::graphics::Img background_source;
    background_source.read(background_path.string());

    kfc::graphics::Img background_texture = background_source.cover_scaled(layout.canvas_width, layout.canvas_height);

    // board.png carries residual near-255 alpha noise even where meant fully
    // opaque, which would force draw_on's expensive per-pixel blend path for
    // no benefit; force_opaque avoids that. board_texture itself is left
    // alone since pieces need its real alpha to blend. background_texture
    // stays 4-channel so HudRenderer's panels can genuinely alpha-blend onto
    // it (see canvas.force_opaque() in the render loop).
    framed_board_texture.force_opaque();

    // Background + frame never change frame-to-frame; compose once instead
    // of redrawing the large framed board every frame.
    kfc::graphics::Img static_backdrop = background_texture.clone();
    framed_board_texture.draw_on(static_backdrop, layout.framed_board_x, layout.framed_board_y);

    return Scene{std::move(board_texture),
                 std::move(static_backdrop),
                 std::move(background_source),
                 layout.canvas_width,
                 layout.canvas_height,
                 layout.grid_offset_x,
                 layout.grid_offset_y,
                 layout.board_column_width,
                 layout.board_column_height};
}

/// The OpenCV window this process shows, and the size it started at (the
/// window is resizable; the render loop re-measures it every frame).
struct DisplayWindow {
    std::string name;
    int display_width;
    int display_height;
};

DisplayWindow setup_window(int canvas_width, int canvas_height) {
    // The canvas's logical resolution can exceed the actual screen; this is
    // only the window's starting size, since it's resizable below.
    kfc::graphics::platform::ScreenSize screen = kfc::graphics::platform::prepare_display_and_measure_screen();
    constexpr double kScreenFitFraction = 0.93;  // leave a little room for the title bar/taskbar
    double display_scale = std::min({1.0, kScreenFitFraction * screen.width / canvas_width,
                                      kScreenFitFraction * screen.height / canvas_height});
    int display_width = static_cast<int>(std::lround(canvas_width * display_scale));
    int display_height = static_cast<int>(std::lround(canvas_height * display_scale));

    DisplayWindow window{"Image", display_width, display_height};
    // WINDOW_NORMAL: user can drag-resize; content is rescaled per-frame and
    // clicks mapped back via ScreenMapper (see the render loop).
    cv::namedWindow(window.name, cv::WINDOW_NORMAL);
    cv::resizeWindow(window.name, display_width, display_height);
    return window;
}

/// Runs the home screen until the player either connects or closes the
/// window. Returns the room name (empty for matchmaking or local play), or
/// std::nullopt if the window was closed before a connection was made.
std::optional<std::string> connect_or_exit(kfc::graphics::app::GameSession& session, const std::string& window_name,
                                            const kfc::graphics::Img& background_source,
                                            kfc::graphics::dialogs::IRoomPrompt& prompt) {
    if (!session.is_networked()) {
        return std::string{};
    }
    // Loops rather than exits on a refused join, so a mistyped room name
    // doesn't force relaunching the whole app.
    while (true) {
        std::optional<kfc::protocol::ClientMessage> action =
            kfc::graphics::app::run_home_screen(window_name, background_source, prompt);
        if (!action.has_value()) {
            return std::nullopt;  // window closed on the home screen
        }
        if (session.connect(*action)) {
            return session.room_name();
        }
        prompt.show_message("Kung Fu Chess", session.join_failure_message());
    }
}

/// Everything the per-frame render loop reads or writes, gathered so the
/// loop itself can be one function instead of inline in main().
struct RenderLoopContext {
    kfc::graphics::app::GameSession& session;
    kfc::texttests::IGameView& game_view;
    kfc::graphics::PieceAnimatorRegistry& animator_registry;
    kfc::graphics::AnimatedPieceRenderer& piece_renderer;
    kfc::graphics::app::MatchOverlay& overlay;
    kfc::graphics::HudRenderer& hud_renderer;
    kfc::model::MoveLogObserver& move_log;
    kfc::model::ScoreObserver& score;
    const kfc::graphics::Img& board_texture;
    const kfc::graphics::Img& static_backdrop;
    const kfc::graphics::Img& background_source;
    const std::string& window_name;
    const std::string& room_name;
    bool spectating;
    int board_pixel_width;
    int board_pixel_height;
    int canvas_width;
    int canvas_height;
    int grid_offset_x;
    int grid_offset_y;
    int board_column_width;
    int board_column_height;
    int display_width;
    int display_height;
};

/// Runs until the window closes, a key is pressed, or matchmaking search
/// times out. Returns true only for the search-timeout case.
bool run_render_loop(RenderLoopContext& ctx) {
    constexpr int kSearchTimeoutMs = 60000;
    auto search_started_at = std::chrono::steady_clock::now();

    std::cout << "Click a piece, then click its destination. Press any key to quit.\n";

    // Cached by window size, since it only changes on an actual resize.
    kfc::graphics::Img cached_window_background;
    int cached_window_background_width = -1;
    int cached_window_background_height = -1;

    auto last_frame_at = std::chrono::steady_clock::now();
    while (true) {
        auto now = std::chrono::steady_clock::now();
        int elapsed_ms =
            static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(now - last_frame_at).count());
        last_frame_at = now;
        // Real delta time keeps the simulated clock tracking real time
        // regardless of render cost; clamped so a paused/dragged window
        // never dumps a multi-second jump into the simulation.
        elapsed_ms = std::clamp(elapsed_ms, 1, 100);

        ctx.game_view.wait(elapsed_ms);
        ctx.animator_registry.advance(elapsed_ms, ctx.game_view);

        bool searching = ctx.session.is_networked() && !ctx.session.is_match_started();
        if (searching && std::chrono::duration_cast<std::chrono::milliseconds>(now - search_started_at).count() >
                              kSearchTimeoutMs) {
            return true;
        }

        kfc::graphics::Img board_frame = ctx.board_texture.clone();
        ctx.piece_renderer.draw(ctx.animator_registry, board_frame);
        switch (ctx.overlay.current(searching, now)) {
            case kfc::graphics::app::Overlay::Searching:
                kfc::graphics::draw_searching_banner(ctx.board_pixel_width, ctx.board_pixel_height, board_frame);
                break;
            case kfc::graphics::app::Overlay::GameOver:
                kfc::graphics::draw_game_over_banner(ctx.overlay.winner(), ctx.board_pixel_width,
                                                      ctx.board_pixel_height, board_frame);
                break;
            case kfc::graphics::app::Overlay::Countdown:
                kfc::graphics::draw_countdown_banner(ctx.overlay.countdown_seconds(), ctx.board_pixel_width,
                                                      ctx.board_pixel_height, board_frame);
                break;
            case kfc::graphics::app::Overlay::Intro:
                kfc::graphics::draw_intro_banner(ctx.board_pixel_width, ctx.board_pixel_height,
                                                  ctx.overlay.intro_opacity(now), board_frame);
                break;
            case kfc::graphics::app::Overlay::None:
                break;
        }

        kfc::graphics::Img canvas = ctx.static_backdrop.clone();
        board_frame.draw_on(canvas, ctx.grid_offset_x, ctx.grid_offset_y);
        ctx.hud_renderer.draw(ctx.move_log, ctx.score, ctx.board_column_width, ctx.board_column_height, canvas);
        if (!ctx.room_name.empty()) {
            std::string label = (ctx.spectating ? "WATCHING ROOM: " : "ROOM: ") + ctx.room_name;
            int baseline = 0;
            cv::Size ts = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 1.0, 2, &baseline);
            canvas.put_text(label, (ctx.canvas_width - ts.width) / 2, 36, 1.0, cv::Scalar(255, 255, 255, 255), 2);
        }
        // Flattens the HUD panels' translucency so the larger per-frame
        // resize/composite below takes draw_on's cheap copyTo path.
        canvas.force_opaque();

        // Picked up fresh every frame so resizing the window takes effect
        // immediately. Background covers the window completely (cropped if
        // needed); game content is scaled uniformly and centered on top.
        // ScreenMapper's click mapping mirrors the content placement, not
        // the background's.
        cv::Rect current_window_rect = cv::getWindowImageRect(ctx.window_name);
        int current_window_width =
            current_window_rect.width > 0 ? current_window_rect.width : ctx.display_width;
        int current_window_height =
            current_window_rect.height > 0 ? current_window_rect.height : ctx.display_height;

        // Scaled fresh from background_source (not the canvas backdrop,
        // already cropped to the canvas's aspect); only redone when the
        // window size actually changed.
        if (current_window_width != cached_window_background_width ||
            current_window_height != cached_window_background_height) {
            cached_window_background =
                ctx.background_source.cover_scaled(current_window_width, current_window_height);
            cached_window_background_width = current_window_width;
            cached_window_background_height = current_window_height;
        }
        kfc::graphics::Img window_frame = cached_window_background.clone();

        double content_fit_scale = std::min(static_cast<double>(current_window_width) / ctx.canvas_width,
                                             static_cast<double>(current_window_height) / ctx.canvas_height);
        int content_width =
            std::min(current_window_width, static_cast<int>(std::lround(ctx.canvas_width * content_fit_scale)));
        int content_height =
            std::min(current_window_height, static_cast<int>(std::lround(ctx.canvas_height * content_fit_scale)));
        int content_x = std::max(0, (current_window_width - content_width) / 2);
        int content_y = std::max(0, (current_window_height - content_height) / 2);
        canvas.resized(content_width, content_height).draw_on(window_frame, content_x, content_y);

        cv::imshow(ctx.window_name, window_frame.get_mat());
        if (cv::waitKey(1) >= 0) {
            return false;
        }
        // Closing via the X button sends no key; without this the loop
        // would spin on a destroyed window and never close the socket.
        if (cv::getWindowProperty(ctx.window_name, cv::WND_PROP_VISIBLE) < 1.0) {
            return false;
        }

        // Caps this process's own frame rate so it doesn't burn a full CPU
        // core; harmless to simulation timing since elapsed_ms is measured
        // from real wall-clock time regardless.
        constexpr std::chrono::milliseconds kTargetFrameDuration{16};  // ~60 FPS
        auto frame_duration = std::chrono::steady_clock::now() - now;
        if (frame_duration < kTargetFrameDuration) {
            std::this_thread::sleep_for(kTargetFrameDuration - frame_duration);
        }
    }
}

}  // namespace

int main(int argc, char** argv) {
    try {
        kfc::graphics::PiecesMineCodeScheme code_scheme;
        kfc::graphics::app::GameSession session(argc, argv);
        std::unique_ptr<kfc::graphics::dialogs::IRoomPrompt> prompt = kfc::graphics::dialogs::make_room_prompt();
        constexpr int kIntroDurationMs = kfc::graphics::app::kDefaultIntroDurationMs;

        int board_pixel_width = session.board_width() * kfc::input::kCellSizePixels;
        int board_pixel_height = session.board_height() * kfc::input::kCellSizePixels;

        Scene scene = build_scene(board_pixel_width, board_pixel_height);
        DisplayWindow window = setup_window(scene.canvas_width, scene.canvas_height);
        kfc::graphics::ScreenMapper screen_mapper(window.name, scene.canvas_width, scene.canvas_height);

        std::optional<std::string> room_name_opt =
            connect_or_exit(session, window.name, scene.background_source, *prompt);
        if (!room_name_opt.has_value()) {
            cv::destroyAllWindows();
            return 0;
        }
        std::string room_name = std::move(*room_name_opt);

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

        kfc::graphics::PieceAssetLibrary asset_library(code_scheme);
        kfc::graphics::PieceAnimatorRegistry animator_registry(asset_library);
        kfc::graphics::AnimatedPieceRenderer piece_renderer(/*show_rest_ring=*/true);
        kfc::graphics::HudRenderer hud_renderer;

        std::unique_ptr<kfc::audio::ISoundPlayer> sound_player =
            kfc::graphics::audio::make_sound_player(kfc::graphics::assets_root() / "sounds");
        kfc::audio::SoundBoard sound_board(game_view->events(), *sound_player);

        // A spectator watches without playing, so no mouse input is wired up.
        bool spectating = session.is_spectator();
        std::optional<kfc::graphics::MouseInputAdapter> mouse_adapter;
        if (!spectating) {
            mouse_adapter.emplace(window.name, *game_view, screen_mapper, scene.grid_offset_x, scene.grid_offset_y);
        }

        RenderLoopContext ctx{.session = session,
                               .game_view = *game_view,
                               .animator_registry = animator_registry,
                               .piece_renderer = piece_renderer,
                               .overlay = overlay,
                               .hud_renderer = hud_renderer,
                               .move_log = move_log,
                               .score = score,
                               .board_texture = scene.board_texture,
                               .static_backdrop = scene.static_backdrop,
                               .background_source = scene.background_source,
                               .window_name = window.name,
                               .room_name = room_name,
                               .spectating = spectating,
                               .board_pixel_width = board_pixel_width,
                               .board_pixel_height = board_pixel_height,
                               .canvas_width = scene.canvas_width,
                               .canvas_height = scene.canvas_height,
                               .grid_offset_x = scene.grid_offset_x,
                               .grid_offset_y = scene.grid_offset_y,
                               .board_column_width = scene.board_column_width,
                               .board_column_height = scene.board_column_height,
                               .display_width = window.display_width,
                               .display_height = window.display_height};
        bool search_timed_out = run_render_loop(ctx);

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
