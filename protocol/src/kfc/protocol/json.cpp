#include "kfc/protocol/json.hpp"

#include "kfc/model/piece_names.hpp"
#include "kfc/realtime/motion_kind_names.hpp"

#include <nlohmann/json.hpp>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

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

// The string a json value holds, without copying it out. get<std::string>()
// allocates a duplicate of a value we only ever compare against a handful of
// fixed names; get_ref hands back the parsed string itself. It throws when the
// value is not a string at all -- which is exactly as unusable as an unknown
// name, and decode_* below turns either into std::nullopt.
[[nodiscard]] std::string_view text_of(const json& j) {
    return j.get_ref<const std::string&>();
}

// Reads one enum written as its name. Throws rather than returning nullopt: a
// field we cannot read makes the whole message unusable, and the decoders turn
// the exception into nullopt for the caller.
template <typename Enum, std::size_t N>
[[nodiscard]] Enum read_named(const json& j, const kfc::util::EnumNames<Enum, N>& names, std::string_view what) {
    std::string_view text = text_of(j);
    std::optional<Enum> value = names.value_of(text);
    if (!value.has_value()) {
        throw std::runtime_error("Unknown " + std::string(what) + " '" + std::string(text) + "'");
    }
    return *value;
}

}  // namespace

// Every enum on the wire is written as its name from the tables in
// kfc/model/piece_names.hpp and kfc/realtime/motion_kind_names.hpp -- the
// readable full words, not the compact chess-notation letters kfc::io uses,
// because this traffic is what a human reads in the logs while debugging (the
// CTD SERVER lecture's logging requirement). Writing is a table lookup
// returning a view of a literal, so none of these hooks allocates a name.

void to_json(json& j, PieceColor color) {
    j = name_of(color);
}

void from_json(const json& j, PieceColor& color) {
    color = read_named(j, kPieceColorNames, "PieceColor");
}

void to_json(json& j, PieceKind kind) {
    j = name_of(kind);
}

void from_json(const json& j, PieceKind& kind) {
    kind = read_named(j, kPieceKindNames, "PieceKind");
}

void to_json(json& j, PieceState state) {
    j = name_of(state);
}

void from_json(const json& j, PieceState& state) {
    state = read_named(j, kPieceStateNames, "PieceState");
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

void to_json(json& j, MotionKind kind) {
    j = name_of(kind);
}

void from_json(const json& j, MotionKind& kind) {
    kind = read_named(j, kMotionKindNames, "MotionKind");
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

// Every call site passes a literal, so the parameter is a view: a const
// std::string& bound one freshly built std::string per encoded message purely
// to be copied into the json a line later.
json envelope(std::string_view type, json payload) {
    return json{{"type", type}, {"payload", std::move(payload)}};
}

// The "type" tag of a decoded envelope, as a view onto the parsed message
// rather than a copy of it -- see text_of above. Throws if the field is absent
// or not a string, which decode_* reports as an undecodable message.
[[nodiscard]] std::string_view type_of(const json& j) {
    return j.at("type").get_ref<const std::string&>();
}

// Parses one wire text. The length check comes first, and without parsing: the
// whole point is to not hand an arbitrary number of bytes to the JSON parser.
// Throws, like json::parse itself does, so both failures leave the decoders
// below by the one path they already have.
[[nodiscard]] json parse_envelope(const std::string& text) {
    if (text.size() > kMaxMessageBytes) {
        throw std::runtime_error("message of " + std::to_string(text.size()) + " bytes exceeds the limit");
    }
    return json::parse(text);
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
                                                {"history", m.history},
                                                {"revision", m.revision},
                                                {"white_username", m.white_username},
                                                {"black_username", m.black_username},
                                                {"white_rating", m.white_rating},
                                                {"black_rating", m.black_rating}});
            } else if constexpr (std::is_same_v<T, MotionStarted>) {
                return envelope("MotionStarted", json{{"motion", m.motion}});
            } else if constexpr (std::is_same_v<T, BoardUpdate>) {
                return envelope("BoardUpdate", json{{"arrival_events", m.arrival_events}, {"revision", m.revision}});
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
                return envelope("MatchStart", json{{"white_username", m.white_username},
                                                    {"black_username", m.black_username},
                                                    {"white_rating", m.white_rating},
                                                    {"black_rating", m.black_rating}});
            } else if constexpr (std::is_same_v<T, JoinFailed>) {
                return envelope("JoinFailed", json{{"reason", m.reason}});
            } else if constexpr (std::is_same_v<T, JoinRedirect>) {
                return envelope("JoinRedirect", json{{"url", m.url}});
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
        json j = parse_envelope(text);
        std::string_view type = type_of(j);
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
    //
    // The key and its value are located tolerantly, not by matching the exact
    // byte sequence our own encoder happens to emit. JSON allows whitespace
    // around a colon, and this ran against real traffic that had it -- a peer
    // sending `"password": "hunter2"`, which any pretty-printing or
    // hand-written client does, wrote the password straight into the log. The
    // whole point of this function is to be correct for messages we did not
    // produce, so it cannot assume our own formatting.
    static constexpr std::string_view kKey = "\"password\"";

    auto skip_whitespace = [&text](std::size_t from) {
        while (from < text.size() &&
               (text[from] == ' ' || text[from] == '\t' || text[from] == '\n' || text[from] == '\r')) {
            ++from;
        }
        return from;
    };

    std::string out;
    std::size_t at = 0;
    while (true) {
        std::size_t key = text.find(kKey, at);
        if (key == std::string::npos) {
            out.append(text, at, std::string::npos);
            return out;
        }

        std::size_t cursor = skip_whitespace(key + kKey.size());
        if (cursor >= text.size() || text[cursor] != ':') {
            // "password" appearing as a value rather than a key. Nothing to
            // redact; copy what we scanned and carry on past it.
            out.append(text, at, cursor - at);
            at = cursor;
            continue;
        }
        cursor = skip_whitespace(cursor + 1);

        if (cursor < text.size() && text[cursor] == '"') {
            std::size_t value = cursor + 1;
            // Walk to the closing quote, stepping over backslash escapes -- a
            // password containing a quote is escaped on the wire, and stopping
            // at it would leave the rest of the password in the log.
            std::size_t end = value;
            while (end < text.size() && text[end] != '"') {
                end += (text[end] == '\\') ? 2 : 1;
            }
            out.append(text, at, value - at).append("***");
            if (end >= text.size()) {
                // Truncated message: drop the remainder rather than risk
                // emitting a partial password.
                return out;
            }
            at = end;
            continue;
        }

        // A password that is not a quoted string -- a number, or null, from a
        // client that built the message loosely. Still a secret, so it goes
        // too: everything up to whatever ends the value.
        std::size_t end = cursor;
        while (end < text.size() && text[end] != ',' && text[end] != '}' && text[end] != ']') {
            ++end;
        }
        out.append(text, at, cursor - at).append("***");
        at = end;
    }
}

std::optional<ServerMessage> decode_server_message(const std::string& text) {
    try {
        json j = parse_envelope(text);
        std::string_view type = type_of(j);
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
            welcome.revision = payload.value("revision", std::uint64_t{0});
            welcome.white_username = payload.value("white_username", std::string{});
            welcome.black_username = payload.value("black_username", std::string{});
            welcome.white_rating = payload.value("white_rating", 0);
            welcome.black_rating = payload.value("black_rating", 0);
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
            update.revision = payload.value("revision", std::uint64_t{0});
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
            MatchStart start;
            start.white_username = payload.value("white_username", std::string{});
            start.black_username = payload.value("black_username", std::string{});
            start.white_rating = payload.value("white_rating", 0);
            start.black_rating = payload.value("black_rating", 0);
            return ServerMessage{start};
        }
        if (type == "JoinFailed") {
            return ServerMessage{JoinFailed{payload.at("reason").get<std::string>()}};
        }
        if (type == "JoinRedirect") {
            return ServerMessage{JoinRedirect{payload.at("url").get<std::string>()}};
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
