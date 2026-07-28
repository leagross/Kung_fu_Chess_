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

// The two things this client cannot write once and compile everywhere -- native
// dialogs and audio -- reached only through their factories. There is no
// #ifdef, and no <windows.h>, anywhere in this file: see room_prompt.hpp.
#include "../../include/kfc/graphics/audio/sound_player_factory.hpp"
#include "../../include/kfc/graphics/dialogs/room_prompt.hpp"
#include "../../include/kfc/graphics/platform/screen_metrics.hpp"

#include "../../include/kfc/graphics/animation/piece_animator_registry.hpp"
#include "../../include/kfc/graphics/animation/piece_asset_library.hpp"
#include "../../include/kfc/graphics/app/game_session.hpp"
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

// The Room dialog and the message boxes are the only native GUI here beyond the
// OpenCV window, and the only thing that differs per platform -- reached
// through IRoomPrompt so this file stays free of any #ifdef. RoomChoice is
// that interface's own type now.
using kfc::graphics::dialogs::RoomChoice;

// --- Home screen ---

// Records the most recent left-click, for the home-screen buttons.
struct HomeClick {
    int x = -1;
    int y = -1;
    bool clicked = false;
};

struct Button {
    int x = 0, y = 0, w = 0, h = 0;
    [[nodiscard]] bool hit(int px, int py) const { return px >= x && px < x + w && py >= y && py < y + h; }
};

// Draws a bordered button centred at (cx, cy) and returns its rectangle.
Button draw_button(kfc::graphics::Img& frame, const std::string& label, int cx, int cy, int w, int h) {
    Button b{cx - w / 2, cy - h / 2, w, h};
    kfc::graphics::Img::blank(w, h, cv::Scalar(220, 220, 220, 255)).draw_on(frame, b.x, b.y);
    kfc::graphics::Img::blank(w - 6, h - 6, cv::Scalar(30, 30, 30, 255)).draw_on(frame, b.x + 3, b.y + 3);
    double font = h / 45.0;
    int baseline = 0;
    cv::Size ts = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, font, 3, &baseline);
    frame.put_text(label, b.x + (w - ts.width) / 2, b.y + (h + ts.height) / 2, font, cv::Scalar(255, 255, 255, 255), 3);
    return b;
}

// Home screen: PLAY (matchmaking) and ROOM (named room via the dialog). Blocks
// until the user picks one -- returning the seating action to send after Login
// -- or closes the window / presses Esc, returning std::nullopt. Uses only the
// window main already created, so play continues in it.
std::optional<kfc::protocol::ClientMessage> run_home_screen(const std::string& window_name,
                                                            const kfc::graphics::Img& background_source,
                                                            kfc::graphics::dialogs::IRoomPrompt& prompt) {
    HomeClick click;
    cv::setMouseCallback(
        window_name,
        [](int event, int x, int y, int, void* userdata) {
            if (event == cv::EVENT_LBUTTONDOWN) {
                auto* c = static_cast<HomeClick*>(userdata);
                c->x = x;
                c->y = y;
                c->clicked = true;
            }
        },
        &click);
    // Detach the callback from `click` (a local) before returning, so nothing
    // dereferences it during the connect() that follows.
    auto finish = [&](std::optional<kfc::protocol::ClientMessage> result) {
        cv::setMouseCallback(window_name, [](int, int, int, int, void*) {}, nullptr);
        return result;
    };

    while (true) {
        cv::Rect rect = cv::getWindowImageRect(window_name);
        int w = rect.width > 0 ? rect.width : 960;
        int h = rect.height > 0 ? rect.height : 720;

        kfc::graphics::Img frame = background_source.cover_scaled(w, h);
        int bw = std::max(200, w / 4);
        int bh = std::max(64, h / 11);
        Button play = draw_button(frame, "PLAY", w / 2, h / 2 - bh, bw, bh);
        Button room = draw_button(frame, "ROOM", w / 2, h / 2 + bh, bw, bh);

        cv::imshow(window_name, frame.get_mat());
        if (cv::waitKey(16) == 27) {  // Esc
            return finish(std::nullopt);
        }
        if (cv::getWindowProperty(window_name, cv::WND_PROP_VISIBLE) < 1.0) {
            return finish(std::nullopt);  // window closed
        }
        if (click.clicked) {
            click.clicked = false;
            if (play.hit(click.x, click.y)) {
                return finish(kfc::protocol::Play{});
            }
            if (room.hit(click.x, click.y)) {
                RoomChoice choice = prompt.ask_room();
                if (choice.action == RoomChoice::Action::Create) {
                    // No name: the server generates the id (see the dialog's
                    // own comment) and reports it back in the Welcome.
                    return finish(kfc::protocol::CreateRoom{});
                }
                if (choice.action == RoomChoice::Action::Join && !choice.room_id.empty()) {
                    return finish(kfc::protocol::JoinRoom{choice.room_id});
                }
                // Cancel, or Join with nothing typed -> stay on the home screen.
            }
        }
    }
}

}  // namespace

int main(int argc, char** argv) {
    try {
        // code_scheme is needed below for asset/rendering lookups
        // (PieceAssetLibrary).
        kfc::graphics::PiecesMineCodeScheme code_scheme;
        kfc::graphics::app::GameSession session(argc, argv);
        // The platform's dialogs. Built once and used for the Room dialog and
        // every message box below.
        std::unique_ptr<kfc::graphics::dialogs::IRoomPrompt> prompt = kfc::graphics::dialogs::make_room_prompt();
        // How long the "KUNG FU CHESS" intro splash takes to fade out.
        constexpr int kIntroDurationMs = 1500;

        // Board dimensions are known up front (GameSession reads the board file
        // before any networked connection), so the window can be laid out and
        // the home screen shown before we ever dial the server.
        int board_pixel_width = session.board_width() * kfc::input::kCellSizePixels;
        int board_pixel_height = session.board_height() * kfc::input::kCellSizePixels;

        // See BoardLayout's own doc comment for what each field means and
        // where board.png's fixed dimensions come from.
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
        // The playable grid, cropped back out of the (now correctly scaled)
        // framed board -- everything downstream (pieces, rest overlays, the
        // game-over banner) keeps drawing onto this exactly as before,
        // unaware the source board image has a frame around it at all.
        kfc::graphics::Img board_texture =
            framed_board_texture.cropped(framed_board_inset_x, framed_board_inset_y, board_pixel_width,
                                          board_pixel_height);

        // background.png, read once at its own native resolution -- never
        // resized here. Every place that needs it "cover"-scaled to some
        // target size (the fixed canvas below, and separately, every frame,
        // the actual current window) scales fresh from *this* full-res
        // source, so cropping only ever happens once per use. Cropping this
        // already-cropped result a second time (which the per-frame window
        // cover used to do) needlessly throws away more of the scene than
        // necessary -- the whole point of "cover" is showing as much of the
        // source as the target's aspect ratio allows, not compounding the
        // crop.
        std::filesystem::path background_path =
            kfc::graphics::assets_root() / kfc::graphics::kDefaultAssetPackName / "background.png";
        kfc::graphics::Img background_source;
        background_source.read(background_path.string());

        kfc::graphics::Img background_texture = background_source.cover_scaled(canvas_width, canvas_height);

        // board.png routinely carries a bit of residual near-255 alpha
        // noise (anti-aliased edges) even where it's meant to be fully
        // opaque, which would otherwise make every draw_on of this (large)
        // image take its expensive per-pixel double-precision blend path
        // for no visual benefit. board_texture is deliberately left alone
        // here -- pieces need its real alpha channel to blend correctly
        // onto it.
        //
        // background_texture is deliberately left 4-channel (not forced
        // opaque) so that HudRenderer's translucent panel boxes, drawn
        // onto static_backdrop/canvas below, can genuinely alpha-blend
        // against it instead of being pasted as solid rectangles (which is
        // all a channel-count mismatch would allow) -- see canvas.
        // force_opaque() in the render loop below for how the *per-frame*
        // cost of that stays bounded despite this.
        //
        // A fully-transparent canvas (letting the per-frame window
        // background show through the board's own margins directly) was
        // tried and reverted -- resizing a transparent-plus-opaque image
        // every frame is fragile (dark edge fringing, or worse, if the
        // un-premultiply math has any bug at all) and broke rendering
        // outright.
        framed_board_texture.force_opaque();

        // The background + framed board frame never change from one frame
        // to the next -- only the pieces do. Composing this once, instead
        // of redrawing the (large) framed board onto a fresh background
        // every single frame, is real per-frame cost saved 30+ times a
        // second.
        kfc::graphics::Img static_backdrop = background_texture.clone();
        framed_board_texture.draw_on(static_backdrop, framed_board_x, framed_board_y);

        kfc::graphics::PieceAssetLibrary asset_library(code_scheme);
        kfc::graphics::PieceAnimatorRegistry animator_registry(asset_library);
        kfc::graphics::AnimatedPieceRenderer piece_renderer(/*show_rest_ring=*/true);
        kfc::graphics::HudRenderer hud_renderer;

        // The canvas above is composed at its full "logical" resolution --
        // matching kfc::input::kCellSizePixels exactly, so every existing
        // piece of coordinate math (cell_top_left, BoardMapper) stays
        // untouched -- which can easily be larger than the actual screen.
        // An initial display size fitting the screen with room to spare;
        // the window is resizable (see below), so this is only the
        // *starting* size, not a fixed one.
        kfc::graphics::platform::ScreenSize screen = kfc::graphics::platform::prepare_display_and_measure_screen();
        int screen_width = screen.width;
        int screen_height = screen.height;
        constexpr double kScreenFitFraction = 0.93;  // leave a little room for the title bar/taskbar
        double display_scale = std::min({1.0, kScreenFitFraction * screen_width / canvas_width,
                                          kScreenFitFraction * screen_height / canvas_height});
        int display_width = static_cast<int>(std::lround(canvas_width * display_scale));
        int display_height = static_cast<int>(std::lround(canvas_height * display_scale));

        const std::string window_name = "Image";
        // WINDOW_NORMAL: the user can drag-resize the window, and the
        // background/board/HUD are rescaled to fill whatever size it
        // currently is, every frame (see the render loop below). Clicks are
        // mapped back to canvas pixel space via ScreenMapper, which queries
        // the window's current on-screen size at click-time -- verify
        // clicks still land on the right cell after actually resizing.
        cv::namedWindow(window_name, cv::WINDOW_NORMAL);
        cv::resizeWindow(window_name, display_width, display_height);

        kfc::graphics::ScreenMapper screen_mapper(window_name, canvas_width, canvas_height);

        // Networked play shows a home screen (Play / Room) first, and only
        // dials the server once a choice is made (see run_home_screen /
        // GameSession::connect). Local play skips straight in.
        std::string room_name;  // shown on-screen during a named-room game
        if (session.is_networked()) {
            // Loops rather than exits on a refused join: mistyping a room name
            // (or naming one whose game just ended) is an ordinary mistake, and
            // making the player relaunch the whole app to retry would be a poor
            // answer to it. The server now says exactly what was wrong, so the
            // message box can too, and the home screen simply comes back.
            while (true) {
                std::optional<kfc::protocol::ClientMessage> action =
                    run_home_screen(window_name, background_source, *prompt);
                if (!action.has_value()) {
                    cv::destroyAllWindows();
                    return 0;  // window closed on the home screen
                }
                if (session.connect(*action)) {
                    // The id to show comes from the server, not from what was
                    // typed: for Create the client has no other way to know it
                    // (the server generated it), and for Join it is the
                    // authoritative answer rather than an echo of the input.
                    room_name = session.room_name();
                    break;
                }
                std::string message = session.join_failure_message();
                prompt->show_message("Kung Fu Chess", message);
            }
        }

        kfc::texttests::IGameView* game_view = &session.view();

        kfc::model::MoveLogObserver move_log(game_view->board().height());
        // Scores with the same piece values the rest of the game is tuned with.
        kfc::model::ScoreObserver score(session.value_provider());
        // The move log and score panel react to the same per-arrival event via
        // the view's pub/sub bus. Wired here, before the render loop, on this
        // one thread (the bus is not internally synchronized).
        game_view->events().subscribe<kfc::model::ArrivalEvent>(
            [&move_log](const kfc::model::ArrivalEvent& event) { move_log.on_arrival(event); });
        game_view->events().subscribe<kfc::model::ArrivalEvent>(
            [&score](const kfc::model::ArrivalEvent& event) { score.on_arrival(event); });

        // Everything that happened before this client existed -- a spectator
        // walking into a game in progress, or a player returning after a
        // disconnect. Fed straight into the two observers rather than published
        // on the bus, because these arrivals already happened once: putting
        // them on the bus would replay every capture sound and re-run the
        // animations. Without this the HUD would show an empty move list and
        // 0-0 beside a board that is plainly mid-game.
        for (const kfc::model::ArrivalEvent& past : session.history()) {
            move_log.on_arrival(past);
            score.on_arrival(past);
        }

        // Start/end visuals off the same bus: GameEnded carries the winner
        // (covers every ending, local or networked); GameStarted stamps the
        // intro (published at real match start); OpponentCountdown drives the
        // disconnect countdown.
        bool game_started = false;
        bool game_ended = false;
        std::optional<kfc::model::PieceColor> end_winner;
        std::optional<int> countdown_seconds;  // set while a dropped opponent's grace counts down
        std::chrono::steady_clock::time_point started_at;
        game_view->events().subscribe<kfc::events::GameStarted>([&](const kfc::events::GameStarted&) {
            game_started = true;
            started_at = std::chrono::steady_clock::now();
        });
        game_view->events().subscribe<kfc::events::GameEnded>([&](const kfc::events::GameEnded& event) {
            game_ended = true;
            end_winner = event.winner;
            countdown_seconds.reset();  // the end banner supersedes the countdown
        });
        game_view->events().subscribe<kfc::events::OpponentCountdown>(
            [&](const kfc::events::OpponentCountdown& event) { countdown_seconds = event.seconds_remaining; });
        // The other way a countdown ends: they came back in time, so the banner
        // clears and play carries on (GameEnded covers the case where they
        // didn't).
        game_view->events().subscribe<kfc::events::OpponentReturned>(
            [&](const kfc::events::OpponentReturned&) { countdown_seconds.reset(); });

        // Sound is just another bus subscriber; silent until .wav files exist in
        // the sounds directory (see its README). Must outlive the render loop.
        std::unique_ptr<kfc::audio::ISoundPlayer> sound_player =
            kfc::graphics::audio::make_sound_player(kfc::graphics::assets_root() / "sounds");
        kfc::audio::SoundBoard sound_board(game_view->events(), *sound_player);

        // A viewer (a third or later joiner of a named room) watches the game
        // without playing it, so no mouse input is wired up at all -- its clicks
        // have nothing to command. ServerLink refuses them too; this just keeps
        // the pointer from behaving as if it could select a piece.
        bool spectating = session.is_spectator();
        std::optional<kfc::graphics::MouseInputAdapter> mouse_adapter;
        if (!spectating) {
            mouse_adapter.emplace(window_name, *game_view, screen_mapper, grid_offset_x, grid_offset_y);
        }

        // How long to wait for a rating-compatible opponent before giving up.
        constexpr int kSearchTimeoutMs = 60000;
        auto search_started_at = std::chrono::steady_clock::now();
        // Set when that timeout fires; the message box itself is shown after the
        // render loop, once the seat has been given back -- see below.
        bool search_timed_out = false;

        std::cout << "Click a piece, then click its destination. Press any key to quit.\n";

        // Real delta time, not a fixed nominal frame_ms -- this frame's
        // rendering (multiple resizes, blends, cv::imshow) can easily take
        // longer, in real wall-clock time, than any fixed constant assumed
        // for it. Advancing the simulated clock by only that constant every
        // iteration, regardless of how long the iteration actually took,
        // makes the game clock fall behind real time -- every move and
        // every rest then takes proportionally longer than its nominal
        // duration to actually play out. Measuring the real elapsed time
        // each iteration keeps simulated time tracking real time exactly,
        // no matter how fast or slow rendering happens to be.
        // The per-frame window-sized background (resized+cropped fresh from
        // background_source below) only actually changes when the window is
        // resized -- recomputing it on every frame regardless was wasted
        // work while the window sits still. Cached here, keyed by the last
        // window size it was built for; cleared (by mismatching size) the
        // frame after an actual resize.
        kfc::graphics::Img cached_window_background;
        int cached_window_background_width = -1;
        int cached_window_background_height = -1;

        auto last_frame_at = std::chrono::steady_clock::now();
        while (true) {
            auto now = std::chrono::steady_clock::now();
            int elapsed_ms = static_cast<int>(
                std::chrono::duration_cast<std::chrono::milliseconds>(now - last_frame_at).count());
            last_frame_at = now;
            // Clamp: never 0 (guarantees real progress even on a very fast
            // iteration) and never huge (a paused/dragged window shouldn't
            // suddenly dump a multi-second jump into the simulation).
            elapsed_ms = std::clamp(elapsed_ms, 1, 100);

            game_view->wait(elapsed_ms);
            animator_registry.advance(elapsed_ms, *game_view);

            // Networked and seated but no opponent yet -> still searching. Give
            // up (and tell the player) after the timeout, per the CTD SERVER
            // spec's one-minute Play search.
            bool searching = session.is_networked() && !session.is_match_started();
            if (searching &&
                std::chrono::duration_cast<std::chrono::milliseconds>(now - search_started_at).count() >
                    kSearchTimeoutMs) {
                search_timed_out = true;
                break;
            }

            kfc::graphics::Img board_frame = board_texture.clone();
            piece_renderer.draw(animator_registry, board_frame);
            // While searching, a "waiting for opponent" overlay; otherwise the
            // end banner (GameEnded, any ending), the disconnect countdown, or
            // the intro splash for its first moment.
            if (searching) {
                kfc::graphics::draw_searching_banner(board_pixel_width, board_pixel_height, board_frame);
            } else if (game_ended) {
                kfc::graphics::draw_game_over_banner(end_winner, board_pixel_width, board_pixel_height, board_frame);
            } else if (countdown_seconds.has_value()) {
                // A networked opponent dropped -- show their grace countdown
                // until they return (future) or it runs out into a GameEnded.
                kfc::graphics::draw_countdown_banner(*countdown_seconds, board_pixel_width, board_pixel_height,
                                                      board_frame);
            } else if (game_started) {
                int since_start_ms = static_cast<int>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(now - started_at).count());
                if (since_start_ms < kIntroDurationMs) {
                    double opacity = 1.0 - static_cast<double>(since_start_ms) / kIntroDurationMs;
                    kfc::graphics::draw_intro_banner(board_pixel_width, board_pixel_height, opacity, board_frame);
                }
            }

            kfc::graphics::Img canvas = static_backdrop.clone();
            board_frame.draw_on(canvas, grid_offset_x, grid_offset_y);
            hud_renderer.draw(move_log, score, board_column_width, board_column_height, canvas);
            // A named room shows its id across the top of the screen (spec:
            // "the room ID is written on the top of the screen").
            if (!room_name.empty()) {
                // A viewer's header says so, so it is never a mystery why
                // clicking the board does nothing.
                std::string label = (spectating ? "WATCHING ROOM: " : "ROOM: ") + room_name;
                int baseline = 0;
                cv::Size ts = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 1.0, 2, &baseline);
                canvas.put_text(label, (canvas_width - ts.width) / 2, 36, 1.0, cv::Scalar(255, 255, 255, 255), 2);
            }
            // Flattens whatever translucency the HUD panels blended into
            // canvas above -- their RGB already reflects that blend, so
            // this loses nothing visually -- so the much bigger per-frame
            // resize/composite below (potentially the whole window) always
            // takes draw_on's cheap copyTo path instead of its expensive
            // per-pixel blend one.
            canvas.force_opaque();

            // Fit whatever size the window is *right now* -- picked up
            // fresh every frame, so dragging the window bigger/smaller
            // takes effect immediately. Two separate layers, scaled
            // differently on purpose:
            //  - the background alone covers the window completely (no
            //    black bars anywhere), cropped if its aspect ratio doesn't
            //    match -- fine, it's just scenery.
            //  - the actual game content (board, pieces, HUD) is scaled
            //    *uniformly*, never stretched out of proportion and never
            //    cropped, then centered on top -- exactly the "rectangular
            //    window, square board in the middle" composition, at
            //    whatever size fits.
            // ScreenMapper's click mapping mirrors the content placement
            // (the only layer that receives clicks), not the background's.
            cv::Rect current_window_rect = cv::getWindowImageRect(window_name);
            int current_window_width = current_window_rect.width > 0 ? current_window_rect.width : display_width;
            int current_window_height =
                current_window_rect.height > 0 ? current_window_rect.height : display_height;

            // Scaled fresh from background_source (the untouched original),
            // not from background_texture (already cropped once to the
            // fixed canvas's aspect) -- cropping an already-cropped image
            // again would show less of the scene than this window's own
            // aspect ratio actually allows. Only actually redone when the
            // window size changed since the last frame -- see
            // cached_window_background above.
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
            // A minimal wait, just enough to pump OpenCV's UI event loop --
            // not a pacing mechanism itself (see the frame cap below for
            // that); the delta-time measurement above is what keeps the
            // simulation's clock correct regardless of how long any of this
            // actually takes.
            if (cv::waitKey(1) >= 0) {
                break;
            }
            // Also quit when the user closes the window with the X button --
            // that sends no key, so without this the loop would keep spinning on
            // a destroyed window and, in networked play, never close the socket,
            // so the opponent would never see the disconnect (no countdown).
            if (cv::getWindowProperty(window_name, cv::WND_PROP_VISIBLE) < 1.0) {
                break;
            }

            // Caps this process's own frame rate: without it, this loop
            // reruns as fast as the machine allows (hundreds of FPS for a
            // window this simple), burning a full CPU core for no visual
            // benefit and starving whatever else is competing for that
            // core -- most notably a second kfc_gui_app instance testing
            // networked play on the same machine, whose own animation
            // smoothness suffers as a direct result. elapsed_ms above is
            // measured from real wall-clock time regardless of how long an
            // iteration takes, so sleeping off any leftover time here is
            // purely a CPU-usage change, not a simulation-timing one.
            constexpr std::chrono::milliseconds kTargetFrameDuration{16};  // ~60 FPS
            auto frame_duration = std::chrono::steady_clock::now() - now;
            if (frame_duration < kTargetFrameDuration) {
                std::this_thread::sleep_for(kTargetFrameDuration - frame_duration);
            }
        }

        cv::destroyAllWindows();

        // Gave up searching: hand the seat back *first*, then tell the player.
        // The other order leaves this client sitting in a joinable room for as
        // long as the (modal, blocking) box goes unanswered -- the next player
        // to press Play is matched into it and immediately watches a disconnect
        // countdown for an opponent who was never there. Safe here: the window
        // is gone, so no mouse callback can reach the view being destroyed.
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
