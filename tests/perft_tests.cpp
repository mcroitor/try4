#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "chess/perft.hpp"
#include "chess/position.hpp"
#include "test_support.hpp"

namespace {

struct PerftReference {
    std::string name;
    std::string fen;
    std::array<std::uint64_t, 6> counts{};
};

std::vector<PerftReference> LoadReferences() {
    const std::string path =
        std::string(TRY4_PERFT_DATA_DIR) + "/reference.tsv";
    std::ifstream input(path);
    REQUIRE(input.is_open());

    std::vector<PerftReference> references;
    std::string line;
    while (std::getline(input, line)) {
        if (line.empty() || line[0] == '#' || line.rfind("name\t", 0) == 0) {
            continue;
        }

        std::istringstream fields(line);
        PerftReference reference;
        std::getline(fields, reference.name, '\t');
        std::getline(fields, reference.fen, '\t');
        for (std::uint64_t& count : reference.counts) {
            std::string value;
            REQUIRE(std::getline(fields, value, '\t'));
            count = std::stoull(value);
        }
        REQUIRE_FALSE(reference.name.empty());
        REQUIRE_FALSE(reference.fen.empty());
        references.push_back(reference);
    }
    return references;
}

}  // namespace

TEST_CASE("perft reference data has six positions", "[perft]") {
    const std::vector<PerftReference> references = LoadReferences();
    REQUIRE(references.size() == 6);
    for (const PerftReference& reference : references) {
        INFO(reference.name);
        REQUIRE(reference.counts[0] > 0);
        REQUIRE(reference.counts[5] > reference.counts[4]);
    }
}

TEST_CASE("perft matches all reference positions through depth 4", "[perft]") {
    const std::vector<PerftReference> references = LoadReferences();
    REQUIRE(references.size() == 6);

    for (const PerftReference& reference : references) {
        INFO("position: " << reference.name);
        std::string error;
        const auto parsed = chess::Position::FromFen(reference.fen, &error);
        INFO("FEN: " << reference.fen);
        INFO("error: " << error);
        REQUIRE(parsed.has_value());

        chess::Position position = *parsed;
        const std::string before = position.ToFen();

        for (int depth = 1; depth <= 4; ++depth) {
            INFO("depth: " << depth);
            const std::uint64_t nodes = chess::Perft(position, depth);
            CHECK(nodes == reference.counts[static_cast<std::size_t>(
                                depth - 1)]);
            CHECK(position.ToFen() == before);
            CHECK(position.HistoryDepth() == 0);
        }
    }
}

TEST_CASE("bulk depth-one counting matches make-and-count", "[perft]") {
    const std::vector<PerftReference> references = LoadReferences();
    REQUIRE(references.size() == 6);

    for (const PerftReference& reference : references) {
        INFO("position: " << reference.name);
        std::string error;
        const auto parsed = chess::Position::FromFen(reference.fen, &error);
        REQUIRE(parsed.has_value());

        chess::Position position = *parsed;
        const std::string before = position.ToFen();

        std::uint64_t manual = 0;
        const std::vector<chess::Move> root_moves = position.LegalMoves();
        for (const chess::Move& move : root_moves) {
            REQUIRE(position.MakeMove(move));
            manual += position.LegalMoves().size();
            REQUIRE(position.UndoMove());
        }

        CHECK(chess::Perft(position, 2) == manual);
        CHECK(position.ToFen() == before);
        CHECK(position.HistoryDepth() == 0);
    }
}

TEST_CASE("perft preserves existing history", "[perft]") {
    chess::Position position;
    position.SetupInitial();
    REQUIRE(position.MakeMove(try4_test::Move("e2", "e4")));

    const std::string before = position.ToFen();
    REQUIRE(chess::Perft(position, 2) == 600);
    CHECK(position.ToFen() == before);
    CHECK(position.HistoryDepth() == 1);
    REQUIRE(position.UndoMove());
    REQUIRE(position.ToFen() ==
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}
