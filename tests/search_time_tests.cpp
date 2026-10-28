#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <thread>

#include "chess/search.hpp"
#include "chess/time_manager.hpp"
#include "chess/transposition_table.hpp"
#include "test_support.hpp"

namespace {

chess::Position FromFen(const char* fen) {
    std::string error;
    const auto position = chess::Position::FromFen(fen, &error);
    INFO("FEN: " << fen);
    INFO("error: " << error);
    REQUIRE(position.has_value());
    return *position;
}

}  // namespace

TEST_CASE("transposition table stores and probes full entries", "[search]") {
    chess::TranspositionTable table(1);
    REQUIRE(table.Capacity() > 1);
    REQUIRE(table.Megabytes() == 1);

    const chess::Move move = try4_test::Move("e2", "e4");
    table.Store(0x123456789ABCDEF0ULL, 5, 42, chess::TTBound::Exact, move);

    chess::TTEntry entry;
    REQUIRE(table.Probe(0x123456789ABCDEF0ULL, &entry));
    REQUIRE(entry.key == 0x123456789ABCDEF0ULL);
    REQUIRE(entry.depth == 5);
    REQUIRE(entry.score == 42);
    REQUIRE(entry.bound == chess::TTBound::Exact);
    REQUIRE(entry.bestMove.ToUci() == move.ToUci());

    table.Clear();
    REQUIRE_FALSE(table.Probe(0x123456789ABCDEF0ULL, &entry));

    table.Resize(2);
    REQUIRE(table.Megabytes() == 2);
    REQUIRE(table.Capacity() > 1);
    REQUIRE_FALSE(table.Probe(0x123456789ABCDEF0ULL, &entry));
}

TEST_CASE("two-way TT buckets keep two entries and replace the shallower one",
          "[search]") {
    chess::TranspositionTable table(1);
    const std::uint64_t first = 0x1111222233334444ULL;
    const std::uint64_t same_bucket = first + table.Capacity();
    const std::uint64_t collision = same_bucket + table.Capacity();

    table.Store(first, 4, 11, chess::TTBound::Lower,
                try4_test::Move("a2", "a3"));
    chess::TTEntry entry;
    REQUIRE(table.Probe(first, &entry));
    REQUIRE_FALSE(table.Probe(same_bucket, &entry));

    table.Store(same_bucket, 6, 22, chess::TTBound::Upper,
                try4_test::Move("b2", "b3"));
    REQUIRE(table.Probe(first, &entry));
    REQUIRE(entry.depth == 4);
    REQUIRE(table.Probe(same_bucket, &entry));
    REQUIRE(entry.depth == 6);

    table.Store(collision, 2, 33, chess::TTBound::Upper,
                try4_test::Move("c2", "c3"));
    REQUIRE_FALSE(table.Probe(first, &entry));
    REQUIRE(table.Probe(same_bucket, &entry));
    REQUIRE(table.Probe(collision, &entry));
    REQUIRE(entry.score == 33);
}

TEST_CASE("mate scores are adjusted across plies", "[search]") {
    const int positive = chess::kMateScore - 4;
    const int negative = -chess::kMateScore + 4;

    REQUIRE(chess::ScoreFromTT(chess::ScoreToTT(positive, 2), 2) == positive);
    REQUIRE(chess::ScoreFromTT(chess::ScoreToTT(negative, 2), 2) == negative);
    REQUIRE(chess::ScoreFromTT(chess::ScoreToTT(positive, 2), 5) ==
            positive - 3);
    REQUIRE(chess::ScoreFromTT(chess::ScoreToTT(negative, 2), 5) ==
            negative + 3);
    REQUIRE(chess::ScoreToTT(37, 5) == 37);

    chess::TranspositionTable table(1);
    const int stored = chess::ScoreToTT(positive, 3);
    table.Store(99, 4, stored, chess::TTBound::Exact, chess::Move{});
    chess::TTEntry entry;
    REQUIRE(table.Probe(99, &entry));
    REQUIRE(chess::ScoreFromTT(entry.score, 3) == positive);
}

TEST_CASE("TT use does not change the fixed-depth minimax result", "[search]") {
    const chess::Position position =
        FromFen("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1");
    chess::SearchLimits with_tt;
    with_tt.depth = 3;
    std::atomic_bool stop{false};

    chess::Searcher tt_searcher;
    const chess::SearchInfo tt_info =
        tt_searcher.Search(position, with_tt, stop);

    chess::SearchLimits without_tt = with_tt;
    without_tt.useTranspositionTable = false;
    chess::Searcher plain_searcher;
    const chess::SearchInfo plain_info =
        plain_searcher.Search(position, without_tt, stop);

    REQUIRE(tt_info.bestMove.has_value());
    REQUIRE(plain_info.bestMove.has_value());
    REQUIRE(tt_info.depth == 3);
    REQUIRE(plain_info.depth == 3);
    REQUIRE(tt_info.scoreCp == plain_info.scoreCp);
    REQUIRE(position.IsLegal(*tt_info.bestMove));
    REQUIRE(position.IsLegal(*plain_info.bestMove));
    REQUIRE(tt_info.nodes <= plain_info.nodes);

    tt_searcher.ClearTranspositionTable();
    REQUIRE(tt_searcher.TranspositionTableMegabytes() == 16);
    tt_searcher.ResizeTranspositionTable(2);
    REQUIRE(tt_searcher.TranspositionTableMegabytes() == 2);
}

TEST_CASE("time budgets handle movetime, clocks, and increments",
          "[search]") {
    chess::SearchLimits movetime;
    movetime.moveTimeMs = 100;
    const chess::TimeBudget fixed =
        chess::ComputeTimeBudget(movetime, chess::Color::White);
    REQUIRE(fixed.softMs == 100);
    REQUIRE(fixed.hardMs == 100);

    chess::SearchLimits clock;
    clock.whiteTimeMs = 1000;
    clock.blackTimeMs = 2000;
    clock.movesToGo = 10;
    const chess::TimeBudget white =
        chess::ComputeTimeBudget(clock, chess::Color::White);
    REQUIRE(white.softMs == 100);
    REQUIRE(white.hardMs == 200);
    const chess::TimeBudget black =
        chess::ComputeTimeBudget(clock, chess::Color::Black);
    REQUIRE(black.softMs == 200);
    REQUIRE(black.hardMs == 400);

    chess::SearchLimits increment = clock;
    increment.whiteIncrementMs = 100;
    const chess::TimeBudget with_increment =
        chess::ComputeTimeBudget(increment, chess::Color::White);
    REQUIRE(with_increment.softMs == 150);
    REQUIRE(with_increment.hardMs == 300);

    REQUIRE_FALSE(chess::ComputeTimeBudget(chess::SearchLimits{},
                                           chess::Color::White)
                      .HasLimit());
}

TEST_CASE("search honors movetime and clock budgets", "[search]") {
    chess::Position position;
    position.SetupInitial();

    chess::SearchLimits limits;
    limits.moveTimeMs = 100;
    std::atomic_bool stop{false};
    const auto begin = std::chrono::steady_clock::now();
    const chess::SearchInfo info = chess::Searcher().Search(position, limits, stop);
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::steady_clock::now() - begin)
                             .count();

    REQUIRE(info.bestMove.has_value());
    REQUIRE(position.IsLegal(*info.bestMove));
    REQUIRE(elapsed <= 150);

    chess::SearchLimits clock;
    clock.whiteTimeMs = 1000;
    clock.movesToGo = 10;
    const auto clock_begin = std::chrono::steady_clock::now();
    const chess::SearchInfo clock_info =
        chess::Searcher().Search(position, clock, stop);
    const auto clock_elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - clock_begin)
            .count();
    REQUIRE(clock_info.bestMove.has_value());
    REQUIRE(position.IsLegal(*clock_info.bestMove));
    REQUIRE(clock_elapsed <= 250);
}

TEST_CASE("stop flag returns a legal move promptly", "[search]") {
    chess::Position position;
    position.SetupInitial();

    chess::SearchLimits limits;
    limits.depth = 64;
    std::atomic_bool stop{false};
    chess::SearchInfo info;
    chess::Searcher searcher;
    std::thread worker([&]() { info = searcher.Search(position, limits, stop); });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    const auto stop_time = std::chrono::steady_clock::now();
    stop.store(true);
    worker.join();
    const auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::steady_clock::now() - stop_time)
                             .count();

    REQUIRE(info.bestMove.has_value());
    REQUIRE(position.IsLegal(*info.bestMove));
    REQUIRE(latency <= 100);
}
