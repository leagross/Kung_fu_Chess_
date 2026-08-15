// k6 load test for kfc_server's WebSocket game protocol -- Roadmap.md's
// stage 3 acceptance test: simulated clients pairing into real games over
// real WebSocket connections, publishing real moves/sec and latency numbers,
// not a synthetic HTTP benchmark that never touches RoomManager/MatchScheduler
// at all.
//
// Each VU is one player: registers a unique account (first Login on a new
// username creates it -- see UserRepository::authenticate), sends Play to
// join matchmaking (kfc::server::RoomManager::join_any pairs same-rating
// waiters, and every account here starts at the same 1200), then -- once
// paired -- sends a Move every ~move_interval_ms for the rest of the run.
// Moves are picked from the client's own pieces off the Welcome/BoardUpdate
// board state, one square "forward" for whichever colour we were dealt; many
// will be illegal (blocked, wrong turn, piece on cooldown) and come back as
// MoveRejected rather than a BoardUpdate. That is fine and expected -- this
// measures the server's capacity to decode, route and tick real protocol
// traffic (kfc_messages_received_total, kfc_moves_processed_total,
// kfc_tick_duration_seconds -- see server/include/kfc/server/metrics.hpp),
// not how good the simulated opponents are at chess.
//
// Run (from repo root, kfc_server already listening on ws://localhost:8080):
//   docker run --rm -i --network host \
//     -e TARGET_URL=ws://localhost:8080 -e VUS=200 -e DURATION=60s \
//     -v "$(pwd)/loadtest:/loadtest" grafana/k6 run /loadtest/k6_load_test.js
// (On Windows/Docker Desktop, --network host is not available -- use
//  -e TARGET_URL=ws://host.docker.internal:8080 instead, no --network flag.)
//
// VUS/DURATION are read at import time (see the `options` export below), so
// they must be set as environment variables, not `-e` k6 script arguments
// after the fact.

import ws from 'k6/ws';
import { check } from 'k6';
import { Counter, Trend } from 'k6/metrics';

const TARGET_URL = __ENV.TARGET_URL || 'ws://localhost:8080';
const VUS = parseInt(__ENV.VUS || '50', 10);
const DURATION = __ENV.DURATION || '30s';
const MOVE_INTERVAL_MS = parseInt(__ENV.MOVE_INTERVAL_MS || '1000', 10);

export const options = {
  scenarios: {
    players: {
      executor: 'constant-vus',
      vus: VUS,
      duration: DURATION,
    },
  },
};

// Custom metrics -- k6's own http_req_duration/etc. don't exist for a raw
// WebSocket connection, so everything worth reporting here is defined by
// hand.
const welcomesReceived = new Counter('kfc_welcomes_received');
const joinFailures = new Counter('kfc_join_failures');
const movesSent = new Counter('kfc_moves_sent');
// Approximate, not a per-message-correlated latency: the protocol carries no
// request id a BoardUpdate/MoveRejected could echo back, so this is "time
// until the next reply of either kind arrives on this socket" -- which is
// sometimes the *opponent's* move being broadcast to the room rather than a
// response to ours. At one move/sec that shows up as some samples in the
// hundreds of ms that are really "how long until the opponent happened to
// move," not server latency. Still a reasonable proxy for round-trip
// responsiveness under load, just not an exact one.
const moveRoundTrip = new Trend('kfc_move_round_trip_ms', true);
const timeToMatched = new Trend('kfc_time_to_matched_ms', true);

function frame(type, payload) {
  return JSON.stringify({ type, payload: payload || {} });
}

// One square toward the opponent's side for a piece of our own colour, taken
// from the board snapshot Welcome/BoardUpdate carries -- not hardcoded
// coordinates, so this still makes sense whatever default_board.txt lays
// out. White moves toward row 0, Black toward the last row (see
// board_parser.cpp's own convention, matched here rather than duplicated
// differently).
function pickMove(board, myColor) {
  const mine = board.pieces.filter((p) => p.color === myColor);
  if (mine.length === 0) {
    return null;
  }
  const piece = mine[Math.floor(Math.random() * mine.length)];
  const dRow = myColor === 'White' ? -1 : 1;
  const destination = { row: piece.cell.row + dRow, col: piece.cell.col };
  if (destination.row < 0 || destination.row >= board.height) {
    return null;
  }
  return { source: piece.cell, destination };
}

export default function () {
  const username = `loadtest_vu${__VU}_iter${__ITER}`;
  const startedAt = Date.now();
  let matchedAt = null;
  let lastMoveSentAt = null;
  let board = null;
  let myColor = null;

  const res = ws.connect(TARGET_URL, {}, function (socket) {
    socket.on('open', () => {
      socket.send(frame('Login', { username, password: 'hunter2' }));
      socket.send(frame('Play', {}));
    });

    socket.on('message', (data) => {
      let msg;
      try {
        msg = JSON.parse(data);
      } catch (e) {
        return; // not our concern for this test -- server-side decode errors are metrics.hpp's job
      }

      if (msg.type === 'Welcome') {
        welcomesReceived.add(1);
        matchedAt = Date.now();
        timeToMatched.add(matchedAt - startedAt);
        board = msg.payload.board;
        myColor = msg.payload.assigned_color;
        // First move a beat after being seated, then on the configured
        // interval -- a real client doesn't move the instant the board
        // renders either.
        socket.setInterval(() => {
          const move = pickMove(board, myColor);
          if (move) {
            socket.send(frame('Move', move));
            movesSent.add(1);
            lastMoveSentAt = Date.now();
          }
        }, MOVE_INTERVAL_MS);
        return;
      }

      if (msg.type === 'JoinFailed' || msg.type === 'LoginFailed') {
        joinFailures.add(1);
        return;
      }

      if (msg.type === 'BoardUpdate') {
        // The server's own authoritative state -- future moves are picked
        // from this, not from replaying our own guess of what happened.
        // arrival_events don't carry a full snapshot, only what changed, so
        // the board is patched in place rather than replaced.
        if (board) {
          for (const event of msg.payload.arrival_events) {
            const piece = board.pieces.find((p) => p.id === event.moved_piece.id);
            if (piece) {
              piece.cell = event.destination;
            }
            if (event.captured_piece) {
              board.pieces = board.pieces.filter((p) => p.id !== event.captured_piece.id);
            }
          }
        }
        if (lastMoveSentAt !== null) {
          moveRoundTrip.add(Date.now() - lastMoveSentAt);
          lastMoveSentAt = null;
        }
        return;
      }

      if (msg.type === 'MoveRejected') {
        if (lastMoveSentAt !== null) {
          moveRoundTrip.add(Date.now() - lastMoveSentAt);
          lastMoveSentAt = null;
        }
        return;
      }
    });

    // No explicit clear on close: k6's ws socket has no clearInterval (only
    // setInterval/setTimeout), and every pending timer is dropped along with
    // the socket once it closes regardless -- confirmed by k6 itself
    // throwing "Object has no member 'clearInterval'" the one time this
    // called it anyway.

    // The whole point is a long-lived connection playing for the scenario's
    // duration, not a single request/response -- close a little before k6
    // tears the VU down anyway, so the close frame has a chance to land
    // rather than the connection just being cut.
    socket.setTimeout(() => {
      socket.close();
    }, 1000 * 60 * 30); // 30 min ceiling -- effectively "until the scenario ends" for any DURATION this is meant to be run with
  });

  check(res, { 'websocket handshake succeeded': (r) => r && r.status === 101 });
}
