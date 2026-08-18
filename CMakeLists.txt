cmake_minimum_required(VERSION 3.20)
# C as well as C++: Argon2 (fetched below) is C.
project(kfc LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Nothing in this project is ever `cmake --install`ed -- every FetchContent
# dependency below is only ever linked in-tree into kfc_server/kfc_tests/
# kfc_app/kfc_gui_app. Some of those dependencies' own CMakeLists.txt still
# define install(EXPORT ...) rules for being consumed as an installed
# package, which CMake validates at *generate* time regardless of whether
# --install is ever run -- and that validation fails once one of them
# (ixwebsocket, once TLS is on) privately links a target from another
# FetchContent'd dependency (Mbed TLS) that was never itself part of an
# export set. Skipping install-rule generation entirely sidesteps a
# consistency check for a feature this project never uses.
set(CMAKE_SKIP_INSTALL_RULES ON)

include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.15.2
)
FetchContent_MakeAvailable(googletest)

# JSON parsing for reading piece config.json files (ConfigPieceSpeedProvider,
# frontend-only -- kfc_core stays JSON-free, same reasoning as staying
# OpenCV-free).
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# Mbed TLS -- the TLS backend ixwebsocket (declared below) needs so
# ServerLink, in kfc_gui_app, can open a real wss:// connection. kfc_server
# itself never terminates TLS (Caddy does -- see DOCKER/Caddyfile: it
# reverse_proxies to kfc_server over plain ws:// on the compose-internal
# network), but it links the same ixwebsocket target kfc_gui_app does, one
# build for both, so this is fetched unconditionally rather than only for
# the GUI target.
#
# Mbed TLS, not OpenSSL: ixwebsocket's own CMakeLists.txt finds either one
# via a bare find_package(...REQUIRED), i.e. a *system* install -- fine on
# Linux (apt has libssl-dev), a real obstacle on Windows, where neither
# OpenSSL nor Mbed TLS is preinstalled and find_package(OpenSSL) simply
# fails ("Could NOT find OpenSSL"). Fetching Mbed TLS here and manually
# satisfying find_package(MbedTLS)'s own success variables below --
# MBEDTLS_FOUND/INCLUDE_DIRS/LIBRARIES, set before ixwebsocket's
# FetchContent_MakeAvailable runs its own `if (NOT MBEDTLS_FOUND)
# find_package(...)` -- keeps this project's own promise that every
# dependency is a `git clone` away, the same promise every other
# FetchContent_Declare in this file already keeps (OpenCV, below, is the
# one deliberate exception, and already documented as one).
#
# ENABLE_TESTING/PROGRAMS off: this project links three of Mbed TLS's own
# libraries, not its test suite or its command-line tools.
set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  mbedtls
  GIT_REPOSITORY https://github.com/Mbed-TLS/mbedtls.git
  GIT_TAG v3.6.7
)
FetchContent_MakeAvailable(mbedtls)
set(MBEDTLS_FOUND TRUE CACHE BOOL "" FORCE)
set(MBEDTLS_INCLUDE_DIRS "${mbedtls_SOURCE_DIR}/include" CACHE PATH "" FORCE)
set(MBEDTLS_LIBRARIES mbedtls mbedx509 mbedcrypto CACHE STRING "" FORCE)

# WebSocket transport for the client-server split (kfc_protocol/kfc_server/
# frontend's ServerLink) -- both client and server roles come from this one
# dependency. USE_TLS OFF (what this used to be) was the original, single-VM
# dev setup, when nothing yet reached the internet; the Roadmap-stage-1 work
# to actually deploy behind Caddy, wss:// and all, is exactly what makes it
# wrong now -- see the Mbed TLS fetch just above for why that backend and
# not OpenSSL. zlib (permessage-deflate compression) stays off -- messages
# are small JSON, not worth the dependency.
set(USE_TLS ON CACHE BOOL "" FORCE)
set(USE_MBED_TLS ON CACHE BOOL "" FORCE)
set(USE_ZLIB OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  ixwebsocket
  GIT_REPOSITORY https://github.com/machinezone/IXWebSocket.git
  GIT_TAG v11.4.5
)
FetchContent_MakeAvailable(ixwebsocket)
# ixwebsocket's own CMakeLists.txt only adds this (needed for Mbed TLS 3.x's
# API, which moved a few functions this backend calls) inside the
# find_package(MbedTLS) branch our MBEDTLS_FOUND override above skips --
# added by hand here since v3.6.7 is definitely "version 3".
target_compile_definitions(ixwebsocket PRIVATE IXWEBSOCKET_USE_MBED_TLS_MIN_VERSION_3)

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
  GIT_REPOSITORY https://github.com/SRombauts/SQLiteCpp.git
  GIT_TAG 3.3.2
)
FetchContent_MakeAvailable(SQLiteCpp)

# hiredis -- the Redis client behind RedisRoomDirectory (server/), which
# routes a room name to whichever kfc_server worker actually owns it once
# more than one worker exists (see Roadmap.md's stage 2). Only three
# commands are ever issued (SET/GET/DEL), but a project whose own
# Server_Design.md targets 10M concurrent users should get its routing
# layer's network protocol handling from a maintained client, not a
# hand-rolled one. Tests/examples off, same reasoning as SQLiteCpp above.
set(DISABLE_TESTS ON CACHE BOOL "" FORCE)
set(ENABLE_SSL OFF CACHE BOOL "" FORCE)
set(ENABLE_SSL_TESTS OFF CACHE BOOL "" FORCE)
# hiredis 1.2.0's own CMakeLists.txt asks for a CMake older than 3.5, which
# current CMake refuses to evaluate at all -- this floor is what
# cmake_minimum_required(VERSION 2.8) inside its CMakeLists.txt is actually
# run as. Only relaxes the *minimum*; nothing here changes how our own
# CMakeLists.txt (VERSION 3.20) is interpreted.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)
FetchContent_Declare(
  hiredis
  GIT_REPOSITORY https://github.com/redis/hiredis.git
  GIT_TAG v1.2.0
)
FetchContent_MakeAvailable(hiredis)

# Argon2 -- the password hash (see database/README.md for why not a plain
# SHA-256). Fetched like every other dependency here; it just has no CMake
# build of its own (upstream ships a Makefile), so the library is declared by
# hand below, pointed at argon2_SOURCE_DIR instead of a downloaded
# CMakeLists.txt. SOURCE_SUBDIR names a directory that does not exist, the same
# guard miniaudio uses below -- there is nothing to add_subdirectory here, only
# source to compile ourselves.
#
# ref.c, not opt.c: the portable compression function, so there is no
# architecture detection to get wrong. ARGON2_NO_THREADS because we hash with
# parallelism 1 -- the threading support would only add a pthread dependency for
# code that never runs.
FetchContent_Declare(
  argon2
  GIT_REPOSITORY https://github.com/P-H-C/phc-winner-argon2.git
  GIT_TAG 20190702
  SOURCE_SUBDIR do-not-configure
)
FetchContent_MakeAvailable(argon2)

add_library(kfc_argon2 STATIC
  ${argon2_SOURCE_DIR}/src/argon2.c
  ${argon2_SOURCE_DIR}/src/core.c
  ${argon2_SOURCE_DIR}/src/encoding.c
  ${argon2_SOURCE_DIR}/src/ref.c
  ${argon2_SOURCE_DIR}/src/blake2/blake2b.c
  ${argon2_SOURCE_DIR}/src/thread.c
)
target_include_directories(kfc_argon2 PUBLIC ${argon2_SOURCE_DIR}/include)
target_include_directories(kfc_argon2 PRIVATE ${argon2_SOURCE_DIR}/src)
target_compile_definitions(kfc_argon2 PRIVATE ARGON2_NO_THREADS)
if (MSVC)
  # Upstream C, built as-is: its warnings are not ours to fix, and treating them
  # as errors would make every upstream refresh a merge conflict.
  target_compile_options(kfc_argon2 PRIVATE /w)
else()
  target_compile_options(kfc_argon2 PRIVATE -w)
endif()

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
    GIT_REPOSITORY https://github.com/mackron/miniaudio.git
    GIT_TAG 0.11.21
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
                 "(kfc_gui_app is skipped)")
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
  protocol/tests/unit/test_file_logger.cpp
  server/tests/unit/test_match.cpp
  server/tests/unit/test_match_scheduler.cpp
  server/tests/unit/test_room_manager.cpp
  server/tests/unit/test_match_audience.cpp
  server/tests/unit/test_client_session.cpp
  server/tests/unit/test_session_registry.cpp
  server/tests/unit/test_http_api.cpp
  server/tests/unit/test_auth_token_store.cpp
  server/tests/unit/test_rate_limiter.cpp
  server/tests/unit/test_metrics.cpp
  server/tests/integration/test_websocket_end_to_end.cpp
  ui/tests/unit/test_match_overlay.cpp
  ui/src/kfc/graphics/app/match_overlay.cpp
  database/tests/unit/test_elo.cpp
  database/tests/unit/test_user_repository.cpp
  database/tests/unit/test_rating_service.cpp
  server/src/kfc/server/match.cpp
  server/src/kfc/server/match_scheduler.cpp
  server/src/kfc/server/room_manager.cpp
  server/src/kfc/server/match_audience.cpp
  server/src/kfc/server/client_session.cpp
  server/src/kfc/server/session_registry.cpp
  server/src/kfc/server/websocket_game_server.cpp
  server/src/kfc/server/disconnect_watch.cpp
  server/src/kfc/server/http_api.cpp
  server/src/kfc/server/auth_token_store.cpp
  server/src/kfc/server/rate_limiter.cpp
  server/src/kfc/server/metrics.cpp
  database/src/kfc/database/elo.cpp
  database/src/kfc/database/password_hash.cpp
  database/src/kfc/database/user_repository.cpp
  database/src/kfc/database/rating_service.cpp
)
# ui/include is on the test path only for the OpenCV-free parts of the client
# (MatchOverlay). Nothing here pulls in OpenCV, so the suite still builds on a
# machine without it -- which CI relies on.
target_include_directories(kfc_tests PRIVATE server/include database/include ui/include)
# ixwebsocket is here only for the end-to-end tests, which bind a real port
# and speak the real protocol -- see server/tests/integration. mbedcrypto is
# for AuthTokenStore's own CSPRNG (see its own comment) -- a direct
# dependency of this test binary's sources, not something ixwebsocket
# happens to expose (it links mbedcrypto privately, for its own TLS backend).
target_link_libraries(kfc_tests PRIVATE kfc_core kfc_protocol GTest::gtest_main SQLiteCpp kfc_argon2 ixwebsocket
                                        mbedcrypto)
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
  server/src/kfc/server/match_scheduler.cpp
  server/src/kfc/server/room_manager.cpp
  server/src/kfc/server/match_audience.cpp
  server/src/kfc/server/client_session.cpp
  server/src/kfc/server/session_registry.cpp
  server/src/kfc/server/disconnect_watch.cpp
  server/src/kfc/server/websocket_game_server.cpp
  server/src/kfc/server/http_api.cpp
  server/src/kfc/server/auth_token_store.cpp
  server/src/kfc/server/rate_limiter.cpp
  server/src/kfc/server/metrics.cpp
  server/src/kfc/server/redis_room_directory.cpp
  database/src/kfc/database/elo.cpp
  database/src/kfc/database/password_hash.cpp
  database/src/kfc/database/user_repository.cpp
  database/src/kfc/database/rating_service.cpp
)
target_include_directories(kfc_server PRIVATE server/include database/include)
# hiredis is kfc_server-only -- RedisRoomDirectory is the one thing that uses
# it, and it's deliberately not exercised by kfc_tests (see
# fake_room_directory.hpp's own comment); the fast suite stays Redis-free.
# mbedcrypto -- see kfc_tests' own target_link_libraries comment above.
target_link_libraries(kfc_server PRIVATE kfc_core kfc_protocol ixwebsocket SQLiteCpp kfc_argon2 hiredis::hiredis
                                         mbedcrypto)
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
    ui/src/kfc/graphics/app/home_screen.cpp
    ui/src/kfc/graphics/app/match_overlay.cpp
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

  # A plain add_executable() links the console subsystem on Windows, so every
  # launch also opened a console window -- alongside the OpenCV game window
  # and, for networked play, the native Login dialog, three top-level windows
  # fighting for foreground focus at once. That console used to be load-
  # bearing (the password prompt read from it -- see game_session.cpp's own
  # history), but nothing in this app reads stdin any more now that the
  # Login dialog collects both fields graphically; the only remaining
  # std::cout/cerr calls are breadcrumbs, fine to go nowhere. WIN32_EXECUTABLE
  # switches the linked subsystem to GUI (no console is ever created);
  # /ENTRY:mainCRTStartup keeps the ordinary `int main()` this file already
  # has as the entry point instead of requiring a WinMain rewrite -- the
  # standard MSVC trick for a GUI-subsystem app that doesn't want WinMain.
  set_target_properties(kfc_gui_app PROPERTIES WIN32_EXECUTABLE TRUE)
  target_link_options(kfc_gui_app PRIVATE /ENTRY:mainCRTStartup)
  endif()

endif()
