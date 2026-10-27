#include "chess/cli.hpp"

#include <charconv>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <istream>
#include <optional>
#include <ostream>
#include <string>
#include <system_error>
#include <vector>

#include "chess/notation.hpp"
#include "chess/perft.hpp"

namespace chess {
namespace {

std::string Trim(std::string text) {
    std::size_t begin = 0;
    while (begin < text.size() &&
           std::isspace(static_cast<unsigned char>(text[begin])) != 0) {
        ++begin;
    }
    std::size_t end = text.size();
    while (end > begin &&
           std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
        --end;
    }
    return text.substr(begin, end - begin);
}

std::string ResultToString(GameResult result) {
    switch (result) {
        case GameResult::Ongoing:
            return "ongoing";
        case GameResult::Checkmate:
            return "checkmate";
        case GameResult::Stalemate:
            return "stalemate";
        case GameResult::DrawFiftyMove:
            return "fifty-move";
        case GameResult::DrawRepetition:
            return "threefold-repetition";
        case GameResult::DrawInsufficientMaterial:
            return "insufficient-material";
    }
    return "unknown";
}

bool ParseDepth(const std::string& text, int* depth) {
    if (text.empty()) {
        return false;
    }
    const char* const begin = text.data();
    const char* const end = begin + text.size();
    const std::from_chars_result result = std::from_chars(begin, end, *depth);
    return result.ec == std::errc{} && result.ptr == end && *depth >= 0;
}

}  // namespace

Cli::Cli(std::istream& input, std::ostream& output)
    : input_(input), output_(output) {
    position_.SetupInitial();
}

int Cli::Run() {
    std::string line;
    while (std::getline(input_, line)) {
        if (!ProcessLine(line)) {
            return 0;
        }
    }
    return 0;
}

bool Cli::ProcessLine(const std::string& line) {
    const std::string trimmed = Trim(line);
    if (trimmed.empty()) {
        return true;
    }

    const std::size_t separator = trimmed.find_first_of(" \t");
    const std::string command =
        trimmed.substr(0, separator);
    const std::string arguments =
        separator == std::string::npos
            ? std::string()
            : Trim(trimmed.substr(separator + 1));

    if (command == "quit") {
        return false;
    }
    if (command == "startpos") {
        position_.SetupInitial();
        output_ << "ok\n";
        return true;
    }
    if (command == "fen") {
        std::string error;
        const auto parsed = Position::FromFen(arguments, &error);
        if (!parsed.has_value()) {
            output_ << "error: " << error << '\n';
            return true;
        }
        position_ = *parsed;
        output_ << "ok\n";
        return true;
    }
    if (command == "board") {
        output_ << position_.ToString();
        return true;
    }
    if (command == "moves") {
        PrintMoves();
        return true;
    }
    if (command == "move") {
        std::optional<Move> move = ParseUci(position_, arguments);
        if (!move.has_value()) {
            move = ParseSan(position_, arguments);
        }
        if (!move.has_value()) {
            output_ << "error: illegal or ambiguous move\n";
            return true;
        }

        const std::string san = ToSan(position_, *move);
        if (san.empty() || !position_.MakeMove(*move)) {
            output_ << "error: illegal move\n";
            return true;
        }
        output_ << "ok " << move->ToUci() << ' ' << san << '\n';
        return true;
    }
    if (command == "undo") {
        if (!position_.UndoMove()) {
            output_ << "error: no moves to undo\n";
        } else {
            output_ << "ok\n";
        }
        return true;
    }
    if (command == "perft") {
        int depth = 0;
        if (!ParseDepth(arguments, &depth)) {
            output_ << "error: perft depth must be a non-negative integer\n";
            return true;
        }
        output_ << "perft " << depth << ' ' << Perft(position_, depth) << '\n';
        return true;
    }
    if (command == "result") {
        PrintResult();
        return true;
    }
    if (command == "help") {
        output_ << "commands: startpos, fen <FEN>, board, moves, move <uci|san>, "
                   "undo, perft <depth>, result, help, quit\n";
        return true;
    }

    output_ << "error: unknown command '" << command << "'\n";
    return true;
}

void Cli::PrintMoves() const {
    const std::vector<Move> moves = position_.LegalMoves();
    if (moves.empty()) {
        output_ << "no legal moves\n";
        return;
    }
    for (const Move& move : moves) {
        output_ << move.ToUci() << ' ' << ToSan(position_, move) << '\n';
    }
}

void Cli::PrintResult() const {
    output_ << ResultToString(position_.Result()) << '\n';
}

}  // namespace chess
