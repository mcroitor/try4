#include <catch2/catch_test_macros.hpp>

#include <array>
#include <fstream>
#include <string>
#include <string_view>

#include "chess/position.hpp"
#include "test_support.hpp"

#ifndef TRY4_TEST_DATA_DIR
#error "TRY4_TEST_DATA_DIR must be defined by the build"
#endif

namespace {

std::string ReadFixture(const std::string& name) {
    const std::string path =
        std::string(TRY4_TEST_DATA_DIR) + "/" + name;
    std::ifstream file(path);
    REQUIRE(file.is_open());

    std::string fen;
    std::getline(file, fen);
    while (!fen.empty() && (fen.back() == '\r' || fen.back() == '\n' ||
                            fen.back() == ' ')) {
        fen.pop_back();
    }
    return fen;
}

struct InvalidFenCase {
    std::string_view fen;
    std::string_view expected_error;
};

}  // namespace

TEST_CASE("canonical FEN fixtures round-trip", "[fen]") {
    const std::array<const char*, 9> fixtures = {
        "startpos.fen",
        "kiwipete.fen",
        "position3.fen",
        "position4.fen",
        "position5.fen",
        "position6.fen",
        "valid_black_to_move.fen",
        "valid_clock_extremes.fen",
        "valid_en_passant.fen",
    };

    for (const char* fixture : fixtures) {
        const std::string fen = ReadFixture(fixture);
        std::string error;
        const auto position = chess::Position::FromFen(fen, &error);

        INFO("fixture: " << fixture);
        INFO("FEN: " << fen);
        INFO("error: " << error);
        REQUIRE(position.has_value());
        REQUIRE(error.empty());
        REQUIRE(position->ToFen() == fen);
    }
}

TEST_CASE("start position parses into the expected state", "[fen]") {
    const std::string fen = ReadFixture("startpos.fen");
    std::string error;
    const auto position = chess::Position::FromFen(fen, &error);

    REQUIRE(position.has_value());
    REQUIRE(error.empty());
    REQUIRE(position->SideToMove() == chess::Color::White);
    REQUIRE(position->CastlingRights() == chess::kAllCastlingRights);
    REQUIRE_FALSE(position->EnPassant().has_value());
    REQUIRE(position->HalfmoveClock() == 0);
    REQUIRE(position->FullmoveNumber() == 1);

    chess::Position initial;
    initial.SetupInitial();
    REQUIRE(*position == initial);
}

TEST_CASE("FEN data preserves side, clocks, and en passant", "[fen]") {
    std::string error;

    const auto black_to_move =
        chess::Position::FromFen(ReadFixture("valid_black_to_move.fen"),
                                 &error);
    REQUIRE(black_to_move.has_value());
    REQUIRE(black_to_move->SideToMove() == chess::Color::Black);

    const auto clock_extremes =
        chess::Position::FromFen(ReadFixture("valid_clock_extremes.fen"),
                                 &error);
    REQUIRE(clock_extremes.has_value());
    REQUIRE(clock_extremes->HalfmoveClock() == 2147483647);
    REQUIRE(clock_extremes->FullmoveNumber() == 2147483647);

    const auto en_passant =
        chess::Position::FromFen(ReadFixture("valid_en_passant.fen"), &error);
    REQUIRE(en_passant.has_value());
    REQUIRE(en_passant->EnPassant() == try4_test::Sq("d6"));
}

TEST_CASE("castling rights are emitted in canonical order", "[fen]") {
    std::string error;
    const auto position = chess::Position::FromFen(
        "4k3/8/8/8/8/8/8/4K3 w qkQK - 0 1", &error);

    REQUIRE(position.has_value());
    REQUIRE(position->ToFen() ==
            "4k3/8/8/8/8/8/8/4K3 w KQkq - 0 1");
}

TEST_CASE("invalid FEN input is rejected with a useful error", "[fen]") {
    const std::array<InvalidFenCase, 13> cases = {
        InvalidFenCase{"8/8/8/8/8/8/8/8 w - - 0 1", "king"},
        InvalidFenCase{"KK6/8/8/8/8/8/8/4k3 w - - 0 1", "king"},
        InvalidFenCase{"4k3/8/8/8/8/8/8 w - - 0 1", "eight ranks"},
        InvalidFenCase{"4k2/8/8/8/8/8/8/4K3 w - - 0 1", "eight files"},
        InvalidFenCase{"4k3/8/8/8/8/8/8/4KX2 w - - 0 1", "invalid piece"},
        InvalidFenCase{"4k3/8/8/8/8/8/8/4K3 x - - 0 1", "side to move"},
        InvalidFenCase{"4k3/8/8/8/8/8/8/4K3 w KQkqK - 0 1", "castling"},
        InvalidFenCase{"4k3/8/8/8/8/8/8/4K3 w - z3 0 1", "en passant"},
        InvalidFenCase{"4k3/8/8/8/8/8/8/4K3 w - e4 0 1", "rank 3 or 6"},
        InvalidFenCase{"4k3/8/8/8/8/8/8/4K3 w - - -1 1", "halfmove"},
        InvalidFenCase{"4k3/8/8/8/8/8/8/4K3 w - - 0 0", "fullmove"},
        InvalidFenCase{"4k3/8/8/8/8/8/8/4K3 w - - 0", "six"},
        InvalidFenCase{"4k3/8/8/8/8/8/8/4K3 w  - 0 1", "fields"},
    };

    for (const InvalidFenCase& test_case : cases) {
        std::string error;
        const auto position =
            chess::Position::FromFen(test_case.fen, &error);

        INFO("FEN: " << std::string(test_case.fen));
        INFO("error: " << error);
        REQUIRE_FALSE(position.has_value());
        REQUIRE_FALSE(error.empty());
        REQUIRE(error.find(test_case.expected_error) != std::string::npos);
    }
}

TEST_CASE("failed FEN parsing does not modify an existing position", "[fen]") {
    chess::Position position;
    position.SetupInitial();
    const std::string before = position.ToFen();

    std::string error;
    const auto parsed = chess::Position::FromFen(
        "4k3/8/8/8/8/8/8/4K3 w - - -1 1", &error);

    REQUIRE_FALSE(parsed.has_value());
    REQUIRE(position.ToFen() == before);
}
