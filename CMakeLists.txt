cmake_minimum_required(VERSION 3.20)
project(kfc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
)
FetchContent_MakeAvailable(googletest)

# JSON parsing for reading piece config.json files (ConfigPieceSpeedProvider,
# frontend-only -- kfc_core stays JSON-free, same reasoning as staying
# OpenCV-free).
FetchContent_Declare(
  nlohmann_json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(nlohmann_json)

# WebSocket transport for the client-server split (kfc_protocol/kfc_server/
# frontend's ServerLink) -- both client and server roles come from this one
# dependency. TLS is off: everything talks over plain ws://localhost, never
# wss://, so there is nothing to verify a certificate for. zlib (permessage-
# deflate compression) is off too -- messages are small JSON, not worth a
# system zlib dependency this project has no other reason to require.
set(USE_TLS OFF CACHE BOOL "" FORCE)
set(USE_ZLIB OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  ixwebsocket
  URL https://github.com/machinezone/IXWebSocket/archive/refs/tags/v11.4.5.zip
)
FetchContent_MakeAvailable(ixwebsocket)

# SQLite (via the SQLiteCpp RAII wrapper, which vendors the sqlite3 amalgamation
# itself -- exactly the "single file, no install" database the CTD SERVER spec
# asks for, for accounts/passwords/ELO ratings). Only kfc_server and the test
# suite link it; kfc_core/kfc_graphics stay database-free, same layering as the
# JSON/OpenCV deps. Wrapper extras (tests/examples/lint) are off -- we want just
# the library.
set(SQLITECPP_RUN_CPPLINT OFF CACHE BOOL "" FORCE)
set(SQLITECPP_RUN_CPPCHECK OFF CACHE BOOL "" FORCE)
set(SQLITECPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SQLITECPP_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  SQLiteCpp
  URL https://github.com/SRombauts/SQLiteCpp/archive/refs/tags/3.3.2.zip
)
FetchContent_MakeAvailable(SQLiteCpp)

# miniaudio: the audio backend for every platform except Windows, which has
# PlaySound built into the OS (see make_sound_player). A single public-domain
# header, so this only has to be downloaded and put on the include path -- there
# is nothing to build. It opens ALSA/PulseAudio/CoreAudio itself at runtime via
# dlopen, so no audio -dev package is needed to compile against it.
#
# SOURCE_SUBDIR names a directory that does not exist on purpose: it stops
# FetchContent from running add_subdirectory on miniaudio's own CMakeLists,
# which would build its tests and examples we have no use for.
if (NOT WIN32)
  FetchContent_Declare(
    miniaudio
    URL https://github.com/mackron/miniaudio/archive/refs/tags/0.11.21.tar.gz
    SOURCE_SUBDIR do-not-configure
  )
  FetchContent_MakeAvailable(miniaudio)
endif()

# OpenCV. The library itself is cross-platform; only the way we obtain it is
# not, so that choice is made here and every target below just links `kfc_opencv`
# without caring which branch produced it.
#
#   Windows -- the prebuilt "world" build unpacked under third_party/OpenCV_451
#              (not committed -- see .gitignore). It ships no OpenCVConfig.cmake,
#              so find_package can't see it; it is wrapped as one IMPORTED target
#              instead of repeating raw include/lib paths on every target.
#   Anything else -- the system OpenCV (apt install libopencv-dev, brew install
#              opencv, ...), found the ordinary way. Nothing is vendored,
#              because those platforms have a real package for it.
#
# Optional on purpose. Only the GUI needs it, so a machine without it (a CI
# runner, or anyone who just wants the server) still configures and builds the
# whole backend and the entire test suite -- which is the point of keeping
# kfc_core/protocol/database/server free of it. When it is missing the GUI
# targets are simply not defined; nothing fails at configure time.
set(KFC_HAVE_OPENCV FALSE)
if (WIN32)
set(OPENCV_451_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/third_party/OpenCV_451)
if (EXISTS "${OPENCV_451_ROOT}/include/opencv2/opencv.hpp")
set(KFC_HAVE_OPENCV TRUE)
add_library(opencv_world451 SHARED IMPORTED)
set_target_properties(opencv_world451 PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES    "${OPENCV_451_ROOT}/include"
  IMPORTED_IMPLIB_DEBUG            "${OPENCV_451_ROOT}/bin/opencv_world451d.lib"
  IMPORTED_LOCATION_DEBUG          "${OPENCV_451_ROOT}/bin/opencv_world451d.dll"
  IMPORTED_IMPLIB_RELEASE          "${OPENCV_451_ROOT}/bin/opencv_world451.lib"
  IMPORTED_LOCATION_RELEASE        "${OPENCV_451_ROOT}/bin/opencv_world451.dll"
  IMPORTED_IMPLIB_RELWITHDEBINFO   "${OPENCV_451_ROOT}/bin/opencv_world451.lib"
  IMPORTED_LOCATION_RELWITHDEBINFO "${OPENCV_451_ROOT}/bin/opencv_world451.dll"
  IMPORTED_IMPLIB_MINSIZEREL       "${OPENCV_451_ROOT}/bin/opencv_world451.lib"
  IMPORTED_LOCATION_MINSIZEREL     "${OPENCV_451_ROOT}/bin/opencv_world451.dll"
)
add_library(kfc_opencv INTERFACE)
target_link_libraries(kfc_opencv INTERFACE opencv_world451)
endif()
else()
# QUIET, not REQUIRED: see above -- a missing OpenCV skips the GUI, it does
# not stop the build.
find_package(OpenCV QUIET)
if (OpenCV_FOUND)
set(KFC_HAVE_OPENCV TRUE)
add_library(kfc_opencv INTERFACE)
target_include_directories(kfc_opencv INTERFACE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(kfc_opencv INTERFACE ${OpenCV_LIBS})
endif()
endif()

if (KFC_HAVE_OPENCV)
  message(STATUS "OpenCV found -- building the GUI client as well")
else()
  message(STATUS "OpenCV not found -- building the backend, server and tests only "
                 "(kfc_gui_app and kfc_mouse_demo are skipped)")
endif()

# Backend: the headless, OpenCV-free business logic (model/rules/realtime/
# engine/input/io/texttests), kept in its own logic/ tree, mirroring how
# ui/ holds everything graphics-related.
add_library(kfc_core
  logic/src/kfc/model/board.cpp
  logic/src/kfc/rules/movement_geometry.cpp
  logic/src/kfc/rules/rook_rule.cpp
  logic/src/kfc/rules/bishop_rule.cpp
  logic/src/kfc/rules/queen_rule.cpp
  logic/src/kfc/rules/knight_rule.cpp
  logic/src/kfc/rules/king_rule.cpp
  logic/src/kfc/rules/pawn_rule.cpp
  logic/src/kfc/rules/drone_rule.cpp
  logic/src/kfc/rules/piece_rule_registry.cpp
  logic/src/kfc/rules/full_piece_rule_registry.cpp
  logic/src/kfc/rules/rule_engine.cpp
  logic/src/kfc/engine/game_engine.cpp
  logic/src/kfc/engine/game_core.cpp
  logic/src/kfc/realtime/real_time_arbiter.cpp
  logic/src/kfc/realtime/collision_resolver.cpp
  logic/src/kfc/realtime/standard_cooldown_policy.cpp
  logic/src/kfc/realtime/jump_cooldown_policy.cpp
  logic/src/kfc/realtime/motion_factory.cpp
  logic/src/kfc/realtime/piece_speed_provider.cpp
  logic/src/kfc/realtime/piece_value_provider.cpp
  logic/src/kfc/realtime/move_log_observer.cpp
  logic/src/kfc/realtime/score_observer.cpp
  logic/src/kfc/realtime/game_over_observer.cpp
  logic/src/kfc/io/parse_error.cpp
  logic/src/kfc/io/piece_token.cpp
  logic/src/kfc/io/text_tokenizer.cpp
  logic/src/kfc/io/board_parser.cpp
  logic/src/kfc/io/board_printer.cpp
  logic/src/kfc/input/board_mapper.cpp
  logic/src/kfc/input/controller.cpp
  logic/src/kfc/audio/sound_board.cpp
  logic/src/kfc/texttests/game.cpp
  logic/src/kfc/texttests/input_reader.cpp
  logic/src/kfc/texttests/command_processor.cpp
)
target_include_directories(kfc_core PUBLIC logic/include)

# Wire protocol for the client-server split: message structs + JSON
# (de)serialization, shared by the server and the client's ServerLink so both
# sides speak the exact same message shapes. Depends on kfc_core (reuses
# Position/Piece/ArrivalEvent as payload field types) and nlohmann_json --
# kfc_core itself stays untouched/JSON-free, same as kfc_graphics's own
# pattern of adding a dependency without polluting the core.
#
# Defined here, before the server and the test suite, because both link it.
add_library(kfc_protocol
  protocol/src/kfc/protocol/json.cpp
  protocol/src/kfc/protocol/file_logger.cpp
  protocol/src/kfc/protocol/gameplay_config.cpp
)
target_include_directories(kfc_protocol PUBLIC protocol/include)
target_link_libraries(kfc_protocol PUBLIC kfc_core nlohmann_json::nlohmann_json)
# The one shared gameplay-timing/values file both the server and the client
# load (see GameplayConfig). Baked in as a path constant, the same way the
# board file and asset dir are, so both binaries find it without a working-dir
# assumption.
target_compile_definitions(kfc_protocol PUBLIC
  KFC_GAMEPLAY_CONFIG_FILE="${CMAKE_CURRENT_SOURCE_DIR}/config/gameplay.json"
)

enable_testing()
add_executable(kfc_tests
  logic/tests/unit/test_position.cpp
  logic/tests/unit/test_piece.cpp
  logic/tests/unit/test_board.cpp
  logic/tests/unit/test_piece_rules.cpp
  logic/tests/unit/test_movement_geometry.cpp
  logic/tests/unit/test_rule_engine.cpp
  logic/tests/unit/test_game_engine.cpp
  logic/tests/unit/test_real_time_arbiter.cpp
  logic/tests/unit/test_collision_resolver.cpp
  logic/tests/unit/test_pawn_promotion.cpp
  logic/tests/unit/test_piece_rules_remaining.cpp
  logic/tests/unit/test_jump_in_place.cpp
  logic/tests/unit/test_drone_rule.cpp
  logic/tests/unit/test_board_parser.cpp
  logic/tests/unit/test_board_printer.cpp
  logic/tests/unit/test_board_mapper.cpp
  logic/tests/unit/test_controller.cpp
  logic/tests/unit/test_input_reader.cpp
  logic/tests/unit/test_piece_speed_provider.cpp
  logic/tests/unit/test_game_observers.cpp
  logic/tests/unit/test_game_observer_registration.cpp
  logic/tests/unit/test_event_bus.cpp
  logic/tests/unit/test_sound_board.cpp
  logic/tests/integration/test_text_scripts.cpp
  protocol/tests/unit/test_protocol_json.cpp
  protocol/tests/unit/test_gameplay_config.cpp
  server/tests/unit/test_match.cpp
  server/tests/unit/test_room_manager.cpp
  database/tests/unit/test_elo.cpp
  database/tests/unit/test_sha256.cpp
  database/tests/unit/test_user_repository.cpp
  database/tests/unit/test_rating_service.cpp
  server/src/kfc/server/match.cpp
  server/src/kfc/server/room_manager.cpp
  server/src/kfc/server/match_audience.cpp
  server/src/kfc/server/disconnect_watch.cpp
  database/src/kfc/database/elo.cpp
  database/src/kfc/database/sha256.cpp
  database/src/kfc/database/user_repository.cpp
  database/src/kfc/database/rating_service.cpp
)
target_include_directories(kfc_tests PRIVATE server/include database/include)
target_link_libraries(kfc_tests PRIVATE kfc_core kfc_protocol GTest::gtest_main SQLiteCpp)
# Test setup code deliberately fires request_move/request_jump for their board
# side effect while arranging a scenario, discarding the [[nodiscard]]
# MoveResult on purpose. Suppress C4834 for this target only -- production code
# (strict everywhere else) discards no nodiscard result, which is the point.
if (MSVC)
  target_compile_options(kfc_tests PRIVATE /wd4834)
endif()

include(GoogleTest)
gtest_discover_tests(kfc_tests)

# The real playable entry point: reads a "Board:"/"Commands:" fixture from
# stdin and drives it through Game/CommandProcessor, matching section 15's
# ScriptRunner -> Controller/GameEngine/BoardPrinter flow.
add_executable(kfc_app
  logic/apps/kfc_app/main.cpp
)
target_link_libraries(kfc_app PRIVATE kfc_core)

# The server executable: hosts every room over WebSocket. Headless like
# kfc_core/kfc_app -- no OpenCV, no graphics dependency, matching the project's
# existing layering.
#
# Two source trees, one binary: server/ is rooms/matches/transport (kfc::server)
# and database/ is accounts/passwords/ratings (kfc::database). The dependency
# only ever points one way -- the server reads and writes the store, the store
# knows nothing of rooms or sockets.
add_executable(kfc_server
  server/apps/kfc_server/main.cpp
  server/src/kfc/server/match.cpp
  server/src/kfc/server/room_manager.cpp
  server/src/kfc/server/match_audience.cpp
  server/src/kfc/server/disconnect_watch.cpp
  server/src/kfc/server/websocket_game_server.cpp
  database/src/kfc/database/elo.cpp
  database/src/kfc/database/sha256.cpp
  database/src/kfc/database/user_repository.cpp
  database/src/kfc/database/rating_service.cpp
)
target_include_directories(kfc_server PRIVATE server/include database/include)
target_link_libraries(kfc_server PRIVATE kfc_core kfc_protocol ixwebsocket SQLiteCpp)
target_compile_definitions(kfc_server PRIVATE
  KFC_SERVER_DEFAULT_BOARD_FILE="${CMAKE_CURRENT_SOURCE_DIR}/server/apps/kfc_server/default_board.txt"
)

# ---------------------------------------------------------------------------
# The GUI client. Everything below needs OpenCV, so it is defined only when
# OpenCV was actually found -- see the detection block above. Without it the
# backend, the server and the full test suite still build normally.
# ---------------------------------------------------------------------------
if (KFC_HAVE_OPENCV)

  # Graphics/UI layer, kept in its own ui/ tree (not mixed into src/ and
  # include/, which are the headless backend) -- OpenCV/Img-based rendering
  # code only, kept out of kfc_core so the engine and its test suite never need
  # OpenCV on their include path. Still links kfc_core PUBLIC below, so it
  # transitively sees the backend's own include/ for model/rules/etc headers.
  add_library(kfc_graphics
    ui/src/kfc/graphics/primitives/img.cpp
    ui/src/kfc/graphics/assets/piece_code_scheme.cpp
    ui/src/kfc/graphics/assets/piece_sprite_path.cpp
    ui/src/kfc/graphics/geometry/board_geometry.cpp
    ui/src/kfc/graphics/io/board_file_loader.cpp
    ui/src/kfc/graphics/input/mouse_input_adapter.cpp
    ui/src/kfc/graphics/input/screen_mapper.cpp
    ui/src/kfc/graphics/animation/piece_animation_set.cpp
    ui/src/kfc/graphics/animation/animation_config_loader.cpp
    ui/src/kfc/graphics/animation/piece_animator.cpp
    ui/src/kfc/graphics/animation/piece_asset_library.cpp
    ui/src/kfc/graphics/animation/piece_animator_registry.cpp
    ui/src/kfc/graphics/rendering/animated_piece_renderer.cpp
    ui/src/kfc/graphics/rendering/hud_renderer.cpp
    ui/src/kfc/graphics/rendering/game_over_banner.cpp
    ui/src/kfc/graphics/rendering/board_layout.cpp
    ui/src/kfc/graphics/net/server_link.cpp
    ui/src/kfc/graphics/app/game_session.cpp
    ui/src/kfc/graphics/platform/screen_metrics.cpp
  )

  # The two per-platform implementations. Exactly one of each is compiled, and
  # together they are the only files in the project that know which OS this is --
  # see IRoomPrompt and make_sound_player.
  if (WIN32)
    target_sources(kfc_graphics PRIVATE
      ui/src/kfc/graphics/dialogs/win_room_prompt.cpp
      ui/src/kfc/graphics/audio/win_sound_player.cpp
    )
  else()
    target_sources(kfc_graphics PRIVATE
      ui/src/kfc/graphics/dialogs/opencv_room_prompt.cpp
      ui/src/kfc/graphics/audio/miniaudio_sound_player.cpp
    )
  endif()

  target_include_directories(kfc_graphics PUBLIC ui/include)
  target_link_libraries(kfc_graphics PUBLIC kfc_core kfc_opencv nlohmann_json::nlohmann_json kfc_protocol ixwebsocket)

  # The audio backend's own dependencies, and only its.
  if (WIN32)
    # winmm: Windows' PlaySound, used by WinSoundPlayer.
    target_link_libraries(kfc_graphics PUBLIC winmm)
  else()
    # miniaudio is header-only, so it is an include path rather than a library.
    # It does need dl (it dlopens the system's audio API), pthread and libm.
    target_include_directories(kfc_graphics PRIVATE ${miniaudio_SOURCE_DIR})
    find_package(Threads REQUIRED)
    target_link_libraries(kfc_graphics PUBLIC Threads::Threads ${CMAKE_DL_LIBS} m)
  endif()
  # Bakes two locations into the binary as string constants, both computed
  # here (once, by CMake, from this file's own location) instead of hardcoded
  # in C++ -- see assets_root()/default_board_file() in constants.hpp. PUBLIC
  # (not PRIVATE) because constants.hpp is a header: kfc_graphics's own .cpp
  # files need these macros to compile it, and so does anything (kfc_gui_app)
  # that links kfc_graphics and includes constants.hpp itself.
  target_compile_definitions(kfc_graphics PUBLIC
    KFC_GRAPHICS_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
    KFC_GUI_APP_DIR="${CMAKE_CURRENT_SOURCE_DIR}/ui/apps/kfc_gui_app"
  )

  # The windowed entry point (as opposed to kfc_app's console fixture runner).
  add_executable(kfc_gui_app
    ui/apps/kfc_gui_app/main.cpp
  )
  target_link_libraries(kfc_gui_app PRIVATE kfc_graphics)

  # opencv_world451.dll must sit next to the .exe at runtime (Windows has no
  # rpath); copy whichever config (debug/release) was actually linked. Not needed
  # elsewhere: a system OpenCV is already on the loader's path.
  if (WIN32)
  add_custom_command(TARGET kfc_gui_app POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      $<TARGET_FILE:opencv_world451>
      $<TARGET_FILE_DIR:kfc_gui_app>
  )
  endif()

  # Standalone mouse-coordinate verification tool -- not part of the real game,
  # the debugging step the graphics lecture recommended before wiring pixel
  # clicks into Controller for real.
  add_executable(kfc_mouse_demo
    ui/apps/kfc_mouse_demo/main.cpp
  )
  target_link_libraries(kfc_mouse_demo PRIVATE kfc_graphics)

  if (WIN32)
  add_custom_command(TARGET kfc_mouse_demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      $<TARGET_FILE:opencv_world451>
      $<TARGET_FILE_DIR:kfc_mouse_demo>
  )
  endif()


endif()
