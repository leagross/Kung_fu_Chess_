#include "../../../include/kfc/texttests/command_processor.hpp"

#include <stdexcept>
#include <string>

#include "../../../include/kfc/io/parse_error.hpp"
#include "../../../include/kfc/io/text_tokenizer.hpp"

namespace kfc::texttests {

namespace {

// Rejects trailing junk ("100abc") that std::stoi would silently accept.
int parse_int(const std::string& token) {
    try {
        std::size_t consumed = 0;
        int value = std::stoi(token, &consumed);
        if (consumed != token.size()) {
            throw kfc::io::ParseError("INVALID_NUMBER");
        }
        return value;
    } catch (const std::invalid_argument&) {
        throw kfc::io::ParseError("INVALID_NUMBER");
    } catch (const std::out_of_range&) {
        throw kfc::io::ParseError("INVALID_NUMBER");
    }
}

}  // namespace

void CommandProcessor::run(Game& game, const std::vector<std::string>& command_lines, std::ostream& out) {
    for (const std::string& line : command_lines) {
        std::vector<std::string> tokens = kfc::io::tokenize(line);
        if (tokens.empty()) {
            continue;
        }

        const std::string& command = tokens[0];
        if (command == "click" || command == "jump") {
            if (tokens.size() != 3) {
                throw kfc::io::ParseError("INVALID_ARGUMENT_COUNT");
            }
            int x = parse_int(tokens[1]);
            int y = parse_int(tokens[2]);
            if (command == "click") {
                game.click(x, y);
            } else {
                game.jump(x, y);
            }
        } else if (command == "wait") {
            if (tokens.size() != 2) {
                throw kfc::io::ParseError("INVALID_ARGUMENT_COUNT");
            }
            int ms = parse_int(tokens[1]);
            if (ms < 0) {
                throw kfc::io::ParseError("INVALID_NUMBER");
            }
            game.wait(ms);
        } else if (command == "print") {
            if (tokens.size() != 2 || tokens[1] != "board") {
                throw kfc::io::ParseError("INVALID_ARGUMENT_COUNT");
            }
            out << game.print_board();
        } else {
            throw kfc::io::ParseError("UNKNOWN_COMMAND");
        }
    }
}

}  // namespace kfc::texttests
