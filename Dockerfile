# Two stages: a build image with a full toolchain, and a runtime image with
# nothing but the server. The toolchain is ~1 GB and is not something to ship or
# to expose in production.

# ---------------------------------------------------------------------------
# Stage 1 -- build
# ---------------------------------------------------------------------------
FROM ubuntu:24.04 AS build

# ca-certificates is not optional: CMake fetches every dependency
# (googletest, nlohmann/json, IXWebSocket, SQLiteCpp, hiredis, argon2) via
# `git clone` over HTTPS at configure time (see CMakeLists.txt's own comment
# on why GIT_REPOSITORY rather than a zip/tar URL), which needs both git
# itself and root certificates to trust GitHub's.
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential cmake ninja-build ca-certificates git \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

# Release, not Debug: this is the image that would take real load, and the
# server's tick loop and Argon2 both care. OpenCV is deliberately absent, so
# CMake skips the GUI targets -- see the OpenCV section of CMakeLists.txt. Only
# kfc_server is built; the test binary belongs in CI, not in a runtime image.
RUN cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --target kfc_server

# ---------------------------------------------------------------------------
# Stage 2 -- runtime
# ---------------------------------------------------------------------------
FROM ubuntu:24.04

# curl is here only for HEALTHCHECK below to have something to run --
# GET /health already exists (see server/include/kfc/server/http_api.hpp)
# purely for this and for a load balancer's own probe; this is the first
# thing in this repo that actually calls it.
RUN apt-get update && apt-get install -y --no-install-recommends \
        libstdc++6 curl \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --system --uid 10001 --no-create-home kfc

COPY --from=build /src/build/kfc_server /usr/local/bin/kfc_server

# gameplay.json and the starting board are baked into the binary at *build* time
# as absolute source paths (KFC_GAMEPLAY_CONFIG_FILE and
# KFC_SERVER_DEFAULT_BOARD_FILE in CMakeLists.txt), so they have to sit at the
# same paths here. Copying them rather than the whole source tree keeps the
# runtime image to the two files it actually opens.
COPY --from=build /src/config/gameplay.json /src/config/gameplay.json
COPY --from=build /src/server/apps/kfc_server/default_board.txt /src/server/apps/kfc_server/default_board.txt

# The server writes kfc_users.db and kfc_server.log to its working directory, so
# that directory is the one thing that must survive a container being replaced.
# Mount a volume here (see compose.yaml) or accounts are lost on every deploy.
WORKDIR /data
RUN chown kfc:kfc /data

# Not root. A game server that accepts connections from the internet has no
# business running with the ability to modify its own image.
USER kfc

EXPOSE 8080
EXPOSE 8081

# Queries the same GET /health handle_health() already answers with a real
# UserRepository query (see http_api.cpp), not just "the process exists" --
# start-period gives the server a few seconds to open its SQLite connection
# and bind both ports before the first probe counts against it.
HEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=3 \
    CMD curl -f http://localhost:8081/health || exit 1

CMD ["kfc_server", "8080", "--http-port=8081"]
