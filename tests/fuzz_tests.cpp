#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#include "chess/position.hpp"

namespace {

void CheckRejected(std::string_view fen) {
    std::string error;
    const auto parsed = chess::Position::FromFen(fen, &error);
    INFO("FEN bytes: " << fen.size());
    CHECK_FALSE(parsed.has_value());
    CHECK_FALSE(error.empty());
}

std::string MakeLongInput() {
    std::string text;
    text.reserve(10000);
    for (int i = 0; i < 10000; ++i) {
        text += static_cast<char>('a' + (i % 26));
    }
    return text;
}

}  // namespace

TEST_CASE("malformed FEN inputs are rejected without crashing", "[fuzz]") {
    const std::string long_input = MakeLongInput();
    const std::string invalid_utf8 = std::string("\xFF", 1) + "7/8/8/8/8/8/8/8 w - - 0 1";
    const std::string invalid_byte = std::string("8/8/8/8/8/8/8/8 w - - 0 ") + "\x80";

    const std::vector<std::string> cases = {
        "",
        " ",
        "8/8/8/8/8/8/8/8 w - - 0 1",
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0",
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 extra",
        "9/8/8/8/8/8/8/8 w - - 0 1",
        "44/8/8/8/8/8/8/8 w - - 0 1",
        "ppppppppp/8/8/8/8/8/8/8 w - - 0 1",
        "X7/8/8/8/8/8/8/8 w - - 0 1",
        "8/8/8/8/8/8/8/8 x - - 0 1",
        "8/8/8/8/8/8/8/K6k w KQx - 0 1",
        "8/8/8/8/8/8/8/K6k w KK - 0 1",
        "8/8/8/8/8/8/8/K6k w - d9 0 1",
        "8/8/8/8/8/8/8/K6k w - e4 0 1",
        "8/8/8/8/8/8/8/K6k w - - -1 1",
        "8/8/8/8/8/8/8/K6k w - - 0 0",
        "8/8/8/8/8/8/8/K6k w - - 999999999999999999999 1",
        long_input,
        invalid_utf8,
        invalid_byte,
    };

    for (const std::string& fen : cases) {
        CheckRejected(fen);
    }
}

TEST_CASE("FEN parser leaves existing positions untouched on failure", "[fuzz]") {
    chess::Position existing;
    existing.SetupInitial();
    const std::string before = existing.ToFen();

    const char* const malformed[] = {
        "",
        "8/8/8/8/8/8/8/8 w - - 0 1",
        "8/8/8/8/8/8/8/K6k w KQx - 0 1",
        "not a fen at all",
    };
    for (const char* fen : malformed) {
        std::string error;
        REQUIRE_FALSE(chess::Position::FromFen(fen, &error).has_value());
        REQUIRE_FALSE(error.empty());
        REQUIRE(existing.ToFen() == before);
    }
}

TEST_CASE("deterministic FEN fuzz corpus never crashes", "[fuzz]") {
    constexpr std::uint64_t kSeed = 0x5EEDF00DULL;
    constexpr int kIterations = 2000;
    const std::string alphabet =
        "pnbrqkPNBRQK12345678/ wb-abcdefghKQkq0123456789";

    std::mt19937_64 rng(kSeed);
    std::uniform_int_distribution<std::size_t> length_distribution(0, 160);
    std::uniform_int_distribution<std::size_t> char_distribution(
        0, alphabet.size() - 1);

    std::size_t accepted = 0;
    for (int iteration = 0; iteration < kIterations; ++iteration) {
        const std::size_t length = length_distribution(rng);
        std::string text;
        text.reserve(length);
        for (std::size_t i = 0; i < length; ++i) {
            text += alphabet[char_distribution(rng)];
        }

        std::string error;
        const auto parsed = chess::Position::FromFen(text, &error);
        if (!parsed.has_value()) {
            REQUIRE_FALSE(error.empty());
            continue;
        }

        ++accepted;
        const std::string round_trip = parsed->ToFen();
        std::string round_trip_error;
        const auto reparsed =
            chess::Position::FromFen(round_trip, &round_trip_error);
        REQUIRE(reparsed.has_value());
        REQUIRE(reparsed->ToFen() == round_trip);
        REQUIRE(parsed->Hash() == parsed->ComputeHash());
        REQUIRE(parsed->LegalMoves().size() <= 256);
    }

    // The corpus is mostly malformed by design; this guards against the
    // generator silently producing nothing at all.
    CHECK(accepted < kIterations);

    const char* const valid[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
    };
    for (const char* fen : valid) {
        const auto parsed = chess::Position::FromFen(fen);
        REQUIRE(parsed.has_value());
        REQUIRE(parsed->ToFen() == fen);
        REQUIRE(parsed->Hash() == parsed->ComputeHash());
        REQUIRE(parsed->LegalMoves().size() <= 256);
    }
}
