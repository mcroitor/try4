#include <catch2/catch_test_macros.hpp>

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "chess/position.hpp"
#include "test_support.hpp"

namespace {

constexpr std::uint64_t kSeed = 0x5EEDC0FFEEULL;
constexpr int kDefaultGameCount = 10000;
constexpr int kDefaultMaxPlies = 16;

std::optional<std::string> EnvironmentValue(const char* name) {
#ifdef _MSC_VER
    char* raw = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&raw, &length, name) != 0 || raw == nullptr) {
        return std::nullopt;
    }
    std::string value(raw);
    std::free(raw);
    return value;
#else
    const char* const raw = std::getenv(name);
    if (raw == nullptr) {
        return std::nullopt;
    }
    return std::string(raw);
#endif
}

int PositiveEnvInt(const char* name, int fallback) {
    const std::optional<std::string> stored = EnvironmentValue(name);
    if (!stored || stored->empty()) {
        return fallback;
    }
    int parsed = 0;
    const char* const begin = stored->data();
    const char* const end = begin + stored->size();
    const std::from_chars_result result =
        std::from_chars(begin, end, parsed);
    if (result.ec != std::errc{} || result.ptr != end || parsed <= 0) {
        return fallback;
    }
    return parsed;
}

struct Snapshot {
    std::string fen;
    std::string board;
    chess::Color side = chess::Color::White;
    chess::CastlingRights rights = chess::kNoCastlingRights;
    std::optional<chess::Square> en_passant;
    int halfmove_clock = 0;
    int fullmove_number = 1;
    std::size_t history_depth = 0;
    std::uint64_t hash = 0;
};

Snapshot TakeSnapshot(const chess::Position& position) {
    Snapshot snapshot;
    snapshot.fen = position.ToFen();
    snapshot.board = position.ToString();
    snapshot.side = position.SideToMove();
    snapshot.rights = position.CastlingRights();
    snapshot.en_passant = position.EnPassant();
    snapshot.halfmove_clock = position.HalfmoveClock();
    snapshot.fullmove_number = position.FullmoveNumber();
    snapshot.history_depth = position.HistoryDepth();
    snapshot.hash = position.Hash();
    return snapshot;
}

bool SameSnapshot(const Snapshot& lhs, const Snapshot& rhs) {
    return lhs.fen == rhs.fen && lhs.board == rhs.board &&
           lhs.side == rhs.side && lhs.rights == rhs.rights &&
           lhs.en_passant == rhs.en_passant &&
           lhs.halfmove_clock == rhs.halfmove_clock &&
           lhs.fullmove_number == rhs.fullmove_number &&
           lhs.history_depth == rhs.history_depth &&
           lhs.hash == rhs.hash;
}

char PromotionChar(chess::PieceType type) {
    switch (type) {
        case chess::PieceType::Knight:
            return 'n';
        case chess::PieceType::Bishop:
            return 'b';
        case chess::PieceType::Rook:
            return 'r';
        case chess::PieceType::Queen:
            return 'q';
        default:
            return '\0';
    }
}

std::string MoveToUci(const chess::Move& move) {
    std::string text = move.from.ToString() + move.to.ToString();
    const char promotion = PromotionChar(move.promotion);
    if (promotion != '\0') {
        text += promotion;
    }
    return text;
}

}  // namespace

TEST_CASE("random legal playouts preserve make/undo invariants",
          "[property]") {
    const int game_count =
        PositiveEnvInt("TRY4_PROPERTY_GAMES", kDefaultGameCount);
    const int max_plies =
        PositiveEnvInt("TRY4_PROPERTY_MAX_PLIES", kDefaultMaxPlies);
    INFO("seed = " << kSeed);
    INFO("games = " << game_count);
    INFO("max plies = " << max_plies);
    std::mt19937_64 rng(kSeed);

    for (int game = 0; game < game_count; ++game) {
        chess::Position position;
        position.SetupInitial();

        const std::string initial_fen = position.ToFen();
        REQUIRE_FALSE(position.MakeMove(chess::Move{}));
        REQUIRE(position.ToFen() == initial_fen);
        REQUIRE(position.HistoryDepth() == 0);

        std::string move_log;
        for (int ply = 0; ply < max_plies; ++ply) {
            const std::vector<chess::Move> legal = position.LegalMoves();
            if (legal.empty()) {
                const chess::GameResult result = position.Result();
                REQUIRE((result == chess::GameResult::Checkmate ||
                         result == chess::GameResult::Stalemate));
                break;
            }

            std::uniform_int_distribution<std::size_t> choose(
                0, legal.size() - 1);
            const chess::Move move = legal[choose(rng)];
            move_log += MoveToUci(move);
            move_log += ' ';

            const Snapshot before = TakeSnapshot(position);
            REQUIRE(position.MakeMove(move));
            REQUIRE(position.Hash() == position.ComputeHash());
            chess::Position after = position;
            REQUIRE(position.UndoMove());
            REQUIRE(position.Hash() == position.ComputeHash());

            const Snapshot restored = TakeSnapshot(position);
            if (!SameSnapshot(before, restored)) {
                INFO("game = " << game);
                INFO("ply = " << ply);
                INFO("moves = " << move_log);
                INFO("before FEN = " << before.fen);
                INFO("after undo FEN = " << restored.fen);
                REQUIRE(SameSnapshot(before, restored));
            }

            position = std::move(after);
        }
    }
}
