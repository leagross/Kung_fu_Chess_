#include "kfc/protocol/json.hpp"

#include "kfc/model/piece_kind_names.hpp"

#include <nlohmann/json.hpp>
#include <cstddef>
#include <stdexcept>

using nlohmann::json;

// nlohmann::json's automatic (de)serialization for a type -- including
// implicitly, whenever that type shows up as a field value or inside a
// std::vector<T> -- finds to_json/from_json via ADL in the *type's own*
// namespace, not the namespace of whatever code happens to call it. Every
// kfc::model type's hooks therefore have to live in kfc::model here, even
// though kfc_core itself stays JSON-free -- this translation unit is the
// one place that pairing exists, and it's the only place it needs to.
namespace kfc::model {

// Forward-declared so the ArrivalEvent (de)serializers below can reference
// the MotionKind ones, whose definitions live further down this file next to
// Motion's.
void to_json(json& j, MotionKind kind);
void from_json(const json& j, MotionKind& kind);

void to_json(json& j, const Position& pos) {
    j = json{{"row", pos.row}, {"col", pos.col}};
}

void from_json(const json& j, Position& pos) {
    j.at("row").get_to(pos.row);
    j.at("col").get_to(pos.col);
}

namespace {
std::string color_to_string(PieceColor color) {
    return color == PieceColor::White ? "White" : "Black";
}

PieceColor color_from_string(const std::string& text) {
    if (text == "White") return PieceColor::White;
    if (text == "Black") return PieceColor::Black;
    throw std::runtime_error("Unknown PieceColor '" + text + "'");
}

std::string kind_to_string(PieceKind kind) {
    return std::string(name_of(kind));
}

PieceKind kind_from_string(const std::string& text) {
    // Throws rather than returning nullopt: a kind we cannot read makes the
    // whole message unusable, and decode_* turns the exception into nullopt.
    std::optional<PieceKind> kind = piece_kind_from_name(text);
    if (!kind.has_value()) {
        throw std::runtime_error("Unknown PieceKind '" + text + "'");
    }
    return *kind;
}

std::string state_to_string(PieceState state) {
    switch (state) {
        case PieceState::Idle: return "Idle";
        case PieceState::Moving: return "Moving";
        case PieceState::Airborne: return "Airborne";
        case PieceState::Captured: return "Captured";
    }
    throw std::runtime_error("Unknown PieceState");
}

PieceState state_from_string(const std::string& text) {
    if (text == "Idle") return PieceState::Idle;
    if (text == "Moving") return PieceState::Moving;
    if (text == "Airborne") return PieceState::Airborne;
    if (text == "Captured") return PieceState::Captured;
    throw std::runtime_error("Unknown PieceState '" + text + "'");
}
}  // namespace

// Readable full names, not the compact chess-notation letters kfc::io
// uses -- these are for logs/wire traffic a human is expected to read
// while debugging, per the CTD SERVER lecture's logging requirement.

void to_json(json& j, PieceColor color) {
    j = color_to_string(color);
}

void from_json(const json& j, PieceColor& color) {
    color = color_from_string(j.get<std::string>());
}

void to_json(json& j, PieceKind kind) {
    j = kind_to_string(kind);
}

void from_json(const json& j, PieceKind& kind) {
    kind = kind_from_string(j.get<std::string>());
}

void to_json(json& j, PieceState state) {
    j = state_to_string(state);
}

void from_json(const json& j, PieceState& state) {
    state = state_from_string(j.get<std::string>());
}

void to_json(json& j, const Piece& piece) {
    j = json{
        {"id", piece.id.value},
        {"color", piece.color},
        {"kind", piece.kind},
        {"cell", piece.cell},
        {"state", piece.state},
        {"has_moved", piece.has_moved},
    };
}

void from_json(const json& j, Piece& piece) {
    piece.id = PieceId{j.at("id").get<int>()};
    j.at("color").get_to(piece.color);
    j.at("kind").get_to(piece.kind);
    j.at("cell").get_to(piece.cell);
    j.at("state").get_to(piece.state);
    j.at("has_moved").get_to(piece.has_moved);
}

void to_json(json& j, const ArrivalEvent& event) {
    j = json{
        {"moved_piece", event.moved_piece},
        {"source", event.source},
        {"destination", event.destination},
        {"kind", event.kind},
        {"was_promotion", event.was_promotion},
        {"arrived_at_ms", event.arrived_at_ms},
    };
    if (event.captured_piece.has_value()) {
        j["captured_piece"] = *event.captured_piece;
    }
}

void from_json(const json& j, ArrivalEvent& event) {
    j.at("moved_piece").get_to(event.moved_piece);
    j.at("source").get_to(event.source);
    j.at("destination").get_to(event.destination);
    j.at("kind").get_to(event.kind);
    j.at("was_promotion").get_to(event.was_promotion);
    j.at("arrived_at_ms").get_to(event.arrived_at_ms);
    if (j.contains("captured_piece")) {
        event.captured_piece = j.at("captured_piece").get<Piece>();
    } else {
        event.captured_piece = std::nullopt;
    }
}

std::string motion_kind_to_string(MotionKind kind) {
    return kind == MotionKind::Move ? "Move" : "JumpInPlace";
}

MotionKind motion_kind_from_string(const std::string& text) {
    if (text == "Move") return MotionKind::Move;
    if (text == "JumpInPlace") return MotionKind::JumpInPlace;
    throw std::runtime_error("Unknown MotionKind '" + text + "'");
}

void to_json(json& j, MotionKind kind) {
    j = motion_kind_to_string(kind);
}

void from_json(const json& j, MotionKind& kind) {
    kind = motion_kind_from_string(j.get<std::string>());
}

void to_json(json& j, const Motion& motion) {
    j = json{
        {"moving_piece", motion.moving_piece}, {"source", motion.source},
        {"destination", motion.destination},   {"kind", motion.kind},
        {"duration_ms", motion.duration_ms},   {"elapsed_ms", motion.elapsed_ms},
        {"cooldown_ms", motion.cooldown_ms},
    };
}

void from_json(const json& j, Motion& motion) {
    j.at("moving_piece").get_to(motion.moving_piece);
    j.at("source").get_to(motion.source);
    j.at("destination").get_to(motion.destination);
    j.at("kind").get_to(motion.kind);
    j.at("duration_ms").get_to(motion.duration_ms);
    j.at("elapsed_ms").get_to(motion.elapsed_ms);
    j.at("cooldown_ms").get_to(motion.cooldown_ms);
}

}  // namespace kfc::model

namespace kfc::protocol {

// Same ADL requirement as the kfc::model hooks above: must be at plain
// kfc::protocol scope (not nested in an anonymous namespace), or nlohmann's
// internal adl_serializer calls fail to find them.
void to_json(json& j, const BoardSnapshot& snapshot) {
    j = json{{"width", snapshot.width}, {"height", snapshot.height}, {"pieces", snapshot.pieces}};
}

void from_json(const json& j, BoardSnapshot& snapshot) {
    j.at("width").get_to(snapshot.width);
    j.at("height").get_to(snapshot.height);
    j.at("pieces").get_to(snapshot.pieces);
}

namespace {

json envelope(const std::string& type, json payload) {
    return json{{"type", type}, {"payload", std::move(payload)}};
}

}  // namespace

BoardSnapshot snapshot_of(const kfc::model::Board& board) {
    BoardSnapshot snapshot{board.width(), board.height(), {}};
    for (int row = 0; row < board.height(); ++row) {
        for (int col = 0; col < board.width(); ++col) {
            std::optional<kfc::model::Piece> piece = board.piece_at(kfc::model::Position{row, col});
            if (piece.has_value()) {
                snapshot.pieces.push_back(*piece);
            }
        }
    }
    return snapshot;
}

std::string encode(const ClientMessage& message) {
    json j = std::visit(
        [](const auto& m) -> json {
            using T = std::decay_t<decltype(m)>;
            if constexpr (std::is_same_v<T, Login>) {
                return envelope("Login", json{{"username", m.username}, {"password", m.password}});
            } else if constexpr (std::is_same_v<T, MoveRequest>) {
                return envelope("Move", json{{"source", m.source}, {"destination", m.destination}});
            } else if constexpr (std::is_same_v<T, JumpRequest>) {
                return envelope("Jump", json{{"cell", m.cell}});
            } else if constexpr (std::is_same_v<T, Resign>) {
                return envelope("Resign", json::object());
            } else if constexpr (std::is_same_v<T, Play>) {
                return envelope("Play", json::object());
            } else if constexpr (std::is_same_v<T, CreateRoom>) {
                return envelope("CreateRoom", json::object());
            } else if constexpr (std::is_same_v<T, JoinRoom>) {
                return envelope("JoinRoom", json{{"name", m.name}});
            }
        },
        message);
    return j.dump();
}

std::string encode(const ServerMessage& message) {
    json j = std::visit(
        [](const auto& m) -> json {
            using T = std::decay_t<decltype(m)>;
            if constexpr (std::is_same_v<T, Welcome>) {
                return envelope("Welcome", json{{"assigned_color", m.assigned_color},
                                                {"board", m.board},
                                                {"spectator", m.spectator},
                                                {"room", m.room},
                                                {"history", m.history}});
            } else if constexpr (std::is_same_v<T, MotionStarted>) {
                return envelope("MotionStarted", json{{"motion", m.motion}});
            } else if constexpr (std::is_same_v<T, BoardUpdate>) {
                return envelope("BoardUpdate", json{{"arrival_events", m.arrival_events}});
            } else if constexpr (std::is_same_v<T, MoveRejected>) {
                return envelope("MoveRejected", json{{"reason", m.reason}});
            } else if constexpr (std::is_same_v<T, GameOver>) {
                json payload = json::object();
                if (m.winner.has_value()) {
                    payload["winner"] = *m.winner;
                }
                return envelope("GameOver", payload);
            } else if constexpr (std::is_same_v<T, OpponentDisconnected>) {
                return envelope("OpponentDisconnected", json{{"seconds_remaining", m.seconds_remaining}});
            } else if constexpr (std::is_same_v<T, MatchStart>) {
                return envelope("MatchStart", json::object());
            } else if constexpr (std::is_same_v<T, JoinFailed>) {
                return envelope("JoinFailed", json{{"reason", m.reason}});
            } else if constexpr (std::is_same_v<T, LoginFailed>) {
                return envelope("LoginFailed", json{{"reason", m.reason}});
            } else if constexpr (std::is_same_v<T, OpponentReconnected>) {
                return envelope("OpponentReconnected", json::object());
            }
        },
        message);
    return j.dump();
}

std::optional<ClientMessage> decode_client_message(const std::string& text) {
    try {
        json j = json::parse(text);
        std::string type = j.at("type").get<std::string>();
        const json& payload = j.at("payload");

        if (type == "Login") {
            Login login;
            login.username = payload.at("username").get<std::string>();
            login.password = payload.at("password").get<std::string>();
            return ClientMessage{login};
        }
        if (type == "Move") {
            MoveRequest request;
            payload.at("source").get_to(request.source);
            payload.at("destination").get_to(request.destination);
            return ClientMessage{request};
        }
        if (type == "Jump") {
            JumpRequest request;
            payload.at("cell").get_to(request.cell);
            return ClientMessage{request};
        }
        if (type == "Resign") {
            return ClientMessage{Resign{}};
        }
        if (type == "Play") {
            return ClientMessage{Play{}};
        }
        if (type == "CreateRoom") {
            return ClientMessage{CreateRoom{}};
        }
        if (type == "JoinRoom") {
            return ClientMessage{JoinRoom{payload.at("name").get<std::string>()}};
        }
        return std::nullopt;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

std::string redact_for_log(const std::string& text) {
    // Scanned rather than parsed: see the header for why a message we cannot
    // decode still has to come out redacted.
    static constexpr char kKey[] = "\"password\":\"";
    static constexpr std::size_t kKeyLength = sizeof(kKey) - 1;

    std::string out;
    std::size_t at = 0;
    while (true) {
        std::size_t key = text.find(kKey, at);
        if (key == std::string::npos) {
            out.append(text, at, std::string::npos);
            return out;
        }

        std::size_t value = key + kKeyLength;
        // Walk to the closing quote, stepping over backslash escapes -- a
        // password containing a quote is escaped on the wire, and stopping at
        // it would leave the rest of the password in the log.
        std::size_t end = value;
        while (end < text.size() && text[end] != '"') {
            end += (text[end] == '\\') ? 2 : 1;
        }
        if (end >= text.size()) {
            // Truncated message: drop the remainder rather than risk emitting
            // a partial password.
            out.append(text, at, key - at).append(kKey).append("***");
            return out;
        }

        out.append(text, at, value - at).append("***");
        at = end;
    }
}

std::optional<ServerMessage> decode_server_message(const std::string& text) {
    try {
        json j = json::parse(text);
        std::string type = j.at("type").get<std::string>();
        const json& payload = j.at("payload");

        if (type == "Welcome") {
            Welcome welcome;
            payload.at("assigned_color").get_to(welcome.assigned_color);
            payload.at("board").get_to(welcome.board);
            // Optional on the wire: a Welcome from before spectators existed
            // (or any hand-written one) simply means "a player".
            welcome.spectator = payload.value("spectator", false);
            welcome.room = payload.value("room", std::string{});
            if (payload.contains("history")) {
                payload.at("history").get_to(welcome.history);
            }
            return ServerMessage{welcome};
        }
        if (type == "MotionStarted") {
            MotionStarted started;
            payload.at("motion").get_to(started.motion);
            return ServerMessage{started};
        }
        if (type == "BoardUpdate") {
            BoardUpdate update;
            payload.at("arrival_events").get_to(update.arrival_events);
            return ServerMessage{update};
        }
        if (type == "MoveRejected") {
            return ServerMessage{MoveRejected{payload.at("reason").get<std::string>()}};
        }
        if (type == "GameOver") {
            GameOver game_over;
            if (payload.contains("winner")) {
                game_over.winner = payload.at("winner").get<kfc::model::PieceColor>();
            } else {
                game_over.winner = std::nullopt;
            }
            return ServerMessage{game_over};
        }
        if (type == "OpponentDisconnected") {
            return ServerMessage{OpponentDisconnected{payload.at("seconds_remaining").get<int>()}};
        }
        if (type == "MatchStart") {
            return ServerMessage{MatchStart{}};
        }
        if (type == "JoinFailed") {
            return ServerMessage{JoinFailed{payload.at("reason").get<std::string>()}};
        }
        if (type == "LoginFailed") {
            return ServerMessage{LoginFailed{payload.at("reason").get<std::string>()}};
        }
        if (type == "OpponentReconnected") {
            return ServerMessage{OpponentReconnected{}};
        }
        return std::nullopt;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

}  // namespace kfc::protocol
