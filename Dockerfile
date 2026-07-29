# Two stages: a build image with a full toolchain, and a runtime image with
# nothing but the server. The toolchain is ~1 GB and is not something to ship or
# to expose in production.

# ---------------------------------------------------------------------------
# Stage 1 -- build
# ---------------------------------------------------------------------------
FROM ubuntu:24.04 AS build

# ca-certificates is not optional: CMake fetches GoogleTest, nlohmann/json,
# IXWebSocket and SQLiteCpp over HTTPS at configure time, and without root
# certificates every one of those downloads fails.
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential cmake ninja-build ca-certificates \
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

RUN apt-get update && apt-get install -y --no-install-recommends \
        libstdc++6 \
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
CMD ["kfc_server", "8080"]
