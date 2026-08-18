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
        // code_scheme is needed below for asset/rendering lookups
        // (PieceAssetLibrary).
        kfc::graphics::PiecesMineCodeScheme code_scheme;
        kfc::graphics::app::GameSession session(argc, argv);
        // The platform's dialogs. Built once and used for the Room dialog and
        // every message box below.
        std::unique_ptr<kfc::graphics::dialogs::IRoomPrompt> prompt = kfc::graphics::dialogs::make_room_prompt();
        // How long the "KUNG FU CHESS" intro splash takes to fade out.
        constexpr int kIntroDurationMs = kfc::graphics::app::kDefaultIntroDurationMs;

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

        // Asked once, before the home screen exists at all -- a username never
        // seen before registers on its first successful login (see
        // IRoomPrompt::ask_login's own doc comment), so there is no separate
        // Register step, just this one dialog. Local play skips it entirely:
        // is_networked() is false before any --server flag, so nothing here
        // is ever shown or asked for.
        if (session.is_networked()) {
            kfc::graphics::dialogs::LoginChoice login = prompt->ask_login();
            if (login.cancelled) {
                cv::destroyAllWindows();
                return 0;
            }
            session.set_credentials(login.username, login.password);
        }

        // Records the most recent left-click while the MENU button (below) is
        // showing -- the game-over screen's own equivalent of home_screen.cpp's
        // HomeClick, installed as this window's mouse callback only once a game
        // has actually ended (see show_menu_button below), so it never competes
        // with MouseInputAdapter's own callback for board clicks during play.
        struct MenuClick {
            int x = -1;
            int y = -1;
            bool clicked = false;
        };

        // One game per iteration. Local play (no menu to return to) always
        // takes the single `break` at the bottom and never loops; networked
        // play loops back here when the player clicks MENU after a game ends,
        // landing back on the home screen in this same window rather than
        // quitting the process. See GameSession::connect's own doc comment for
        // why disconnect() must run first (view_ has to go back to nullptr) --
        // otherwise the next connect() below would silently no-op.
        while (true) {
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
                        kfc::graphics::app::run_home_screen(window_name, background_source, *prompt);
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

            // Which banner the board wears, and the rules deciding between them,
            // live in MatchOverlay -- it subscribes to the whole-game signals here
            // and answers one question per frame below.
            kfc::graphics::app::MatchOverlay overlay(game_view->events(), kIntroDurationMs);

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

            // Set once the MENU button (shown only once the game is over -- see
            // show_menu_button below) is clicked. Checked right after the render
            // loop, before cv::destroyAllWindows() would otherwise run, so this
            // path can skip that and loop back to the home screen in the same
            // window instead of quitting.
            bool return_to_menu = false;
            // The click recorder installed as this window's mouse callback once
            // show_menu_button first turns true for this game -- see MenuClick's
            // own comment above the outer loop.
            MenuClick menu_click;
            bool menu_click_handler_installed = false;

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
                // Read once, not once per branch below (the switch and the MENU
                // button both need it, and MatchOverlay::current advances timers
                // as a side effect -- calling it twice per frame would double
                // that advance).
                kfc::graphics::app::Overlay overlay_state = overlay.current(searching, now);
                // Only for networked play: local play has no home screen to
                // return to, so it never shows this button at all.
                bool show_menu_button =
                    session.is_networked() && overlay_state == kfc::graphics::app::Overlay::GameOver;
                switch (overlay_state) {
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
                hud_renderer.draw(move_log, score, board_column_width, board_column_height, canvas,
                                  session.white_username(), session.black_username());
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

                // The one clickable thing this loop draws itself -- everything
                // else on screen either is the board (MouseInputAdapter handles
                // that) or is drawn by run_home_screen in its own loop. Placed
                // below draw_game_over_banner's centered "WHITE WINS"/"BLACK
                // WINS"/"DRAW" text so the two never overlap. Drawn in canvas
                // (not board_frame) coordinates -- see MenuClick's own comment
                // above the outer loop for why this needs ScreenMapper, the same
                // way MouseInputAdapter does, rather than raw window pixels.
                kfc::graphics::app::Button menu_button{};
                if (show_menu_button) {
                    if (!menu_click_handler_installed) {
                        cv::setMouseCallback(
                            window_name,
                            [](int event, int x, int y, int, void* userdata) {
                                if (event == cv::EVENT_LBUTTONDOWN) {
                                    auto* recorded = static_cast<MenuClick*>(userdata);
                                    recorded->x = x;
                                    recorded->y = y;
                                    recorded->clicked = true;
                                }
                            },
                            &menu_click);
                        menu_click_handler_installed = true;
                    }
                    int button_cx = grid_offset_x + board_pixel_width / 2;
                    int button_cy = grid_offset_y + board_pixel_height / 2 + 90;
                    menu_button = kfc::graphics::app::draw_button(canvas, "MENU", button_cx, button_cy, 220, 60);
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
                    cached_window_background =
                        background_source.cover_scaled(current_window_width, current_window_height);
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

                // MENU was clicked: give the seat back and go back to the home
                // screen, in this same window, instead of either quitting or
                // continuing to render a game that is already over. Checked
                // last (after imshow), same as the two quits above, so the
                // banner and button are always shown for at least the frame the
                // click landed on rather than vanishing a frame early.
                if (show_menu_button && menu_click.clicked) {
                    menu_click.clicked = false;
                    kfc::graphics::PixelPoint clicked_at = screen_mapper.to_canvas_pixels(menu_click.x, menu_click.y);
                    if (menu_button.hit(clicked_at.x, clicked_at.y)) {
                        return_to_menu = true;
                        break;
                    }
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

            if (return_to_menu) {
                // Same reason as the "no opponent found" disconnect below: the
                // seat must not be left sitting there for the next Play to be
                // matched against a player who has already left. Also what
                // makes the next loop iteration's connect() actually dial out
                // again instead of no-op'ing (see GameSession::connect).
                session.disconnect();
                continue;
            }

            if (search_timed_out) {
                // Hand the seat back *first*, then tell the player -- the other
                // order leaves this client sitting in a joinable room for as
                // long as the (modal, blocking) box goes unanswered, so the next
                // player to press Play is matched into it and immediately
                // watches a disconnect countdown for an opponent who was never
                // there.
                //
                // Back to the home screen afterward, in this same window, not a
                // quit -- "no opponent found" used to end the whole process here
                // (the same way any other break out of the render loop did,
                // before return_to_menu existed above), which meant the one
                // realistic way a networked game ends without ever starting also
                // happened to be the one path that couldn't just try Play again
                // without relaunching the app.
                session.disconnect();
                prompt->show_message("Kung Fu Chess", "No opponent found within a minute. Please try again later.");
                continue;
            }

            cv::destroyAllWindows();
            break;  // local play, or the window was closed / a key was pressed mid-game
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
