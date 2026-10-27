#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <string>
#include <vector>

#include "chess/search.hpp"
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

std::vector<std::string> PvUci(const std::vector<chess::Move>& pv) {
    std::vector<std::string> result;
    for (const chess::Move& move : pv) {
        result.push_back(move.ToUci());
    }
    return result;
}

}  // namespace

TEST_CASE("search finds mate in one", "[search]") {
    const chess::Position position =
        FromFen("6k1/5ppp/8/8/8/8/8/R5K1 w - - 0 1");
    chess::SearchLimits limits;
    limits.depth = 2;
    std::atomic_bool stop{false};

    const chess::SearchInfo info =
        chess::Searcher().Search(position, limits, stop);
    REQUIRE(info.bestMove.has_value());
    REQUIRE(info.bestMove->ToUci() == "a1a8");
    REQUIRE(info.pv.size() == 1);
    REQUIRE(info.pv.front().ToUci() == "a1a8");
    REQUIRE(info.scoreCp >= chess::kMateThreshold);
    REQUIRE(info.depth == 1);
}

TEST_CASE("search handles terminal positions without a best move", "[search]") {
    chess::SearchLimits limits;
    limits.depth = 3;
    std::atomic_bool stop{false};

    const chess::Position checkmate =
        FromFen("7k/6Q1/6K1/8/8/8/8/8 b - - 0 1");
    const chess::SearchInfo mate_info =
        chess::Searcher().Search(checkmate, limits, stop);
    REQUIRE_FALSE(mate_info.bestMove.has_value());
    REQUIRE(mate_info.pv.empty());
    REQUIRE(mate_info.depth == 0);
    REQUIRE(mate_info.scoreCp == -chess::kMateScore);

    const chess::Position stalemate =
        FromFen("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1");
    const chess::SearchInfo stale_info =
        chess::Searcher().Search(stalemate, limits, stop);
    REQUIRE_FALSE(stale_info.bestMove.has_value());
    REQUIRE(stale_info.pv.empty());
    REQUIRE(stale_info.scoreCp == 0);
}

TEST_CASE("search chooses a forced winning capture", "[search]") {
    const chess::Position position =
        FromFen("4k3/8/8/3q4/4P3/8/8/4K3 w - - 0 1");
    chess::SearchLimits limits;
    limits.depth = 1;
    std::atomic_bool stop{false};

    const chess::SearchInfo info =
        chess::Searcher().Search(position, limits, stop);
    REQUIRE(info.bestMove.has_value());
    REQUIRE(info.bestMove->ToUci() == "e4d5");
    REQUIRE(info.pv.size() == 1);
    REQUIRE(info.scoreCp > 0);
}

TEST_CASE("fixed-depth search is deterministic and leaves the caller intact",
          "[search]") {
    chess::Position position;
    position.SetupInitial();
    const std::string before = position.ToFen();

    chess::SearchLimits limits;
    limits.depth = 3;
    std::atomic_bool first_stop{false};
    std::atomic_bool second_stop{false};

    const chess::SearchInfo first =
        chess::Searcher().Search(position, limits, first_stop);
    const chess::SearchInfo second =
        chess::Searcher().Search(position, limits, second_stop);

    REQUIRE(position.ToFen() == before);
    REQUIRE(position.HistoryDepth() == 0);
    REQUIRE(first.bestMove.has_value());
    REQUIRE(second.bestMove.has_value());
    REQUIRE(first.bestMove->ToUci() == second.bestMove->ToUci());
    REQUIRE(first.depth == second.depth);
    REQUIRE(first.scoreCp == second.scoreCp);
    REQUIRE(PvUci(first.pv) == PvUci(second.pv));
    REQUIRE(first.nodes == second.nodes);
}

TEST_CASE("PV moves are legal and can be replayed", "[search]") {
    const chess::Position position =
        FromFen("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
    chess::SearchLimits limits;
    limits.depth = 3;
    std::atomic_bool stop{false};

    const chess::SearchInfo info =
        chess::Searcher().Search(position, limits, stop);
    REQUIRE(info.bestMove.has_value());
    REQUIRE_FALSE(info.pv.empty());
    REQUIRE(info.pv.front().ToUci() == info.bestMove->ToUci());

    chess::Position replay = position;
    for (const chess::Move& move : info.pv) {
        INFO("PV move: " << move.ToUci());
        REQUIRE(replay.IsLegal(move));
        REQUIRE(replay.MakeMove(move));
    }
}

TEST_CASE("quiescence resolves a recapture beyond the nominal depth",
          "[search]") {
    const chess::Position position =
        FromFen("4k3/8/2p5/3p4/8/8/8/3QK3 w - - 0 1");

    chess::SearchLimits shallow;
    shallow.depth = 1;
    shallow.useQuiescence = false;
    std::atomic_bool stop{false};
    const chess::SearchInfo without_q =
        chess::Searcher().Search(position, shallow, stop);
    REQUIRE(without_q.bestMove.has_value());
    REQUIRE(without_q.bestMove->ToUci() == "d1d5");

    chess::SearchLimits with_q = shallow;
    with_q.useQuiescence = true;
    const chess::SearchInfo with_quiescence =
        chess::Searcher().Search(position, with_q, stop);
    REQUIRE(with_quiescence.bestMove.has_value());
    REQUIRE(with_quiescence.bestMove->ToUci() != "d1d5");
    REQUIRE(with_quiescence.scoreCp > 0);
}

TEST_CASE("move ordering preserves the minimax score and reduces work",
          "[search]") {
    const chess::Position position =
        FromFen("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1");

    chess::SearchLimits ordered;
    ordered.depth = 3;
    std::atomic_bool stop{false};
    const chess::SearchInfo ordered_info =
        chess::Searcher().Search(position, ordered, stop);

    chess::SearchLimits unordered = ordered;
    unordered.useMoveOrdering = false;
    const chess::SearchInfo unordered_info =
        chess::Searcher().Search(position, unordered, stop);

    REQUIRE(ordered_info.bestMove.has_value());
    REQUIRE(unordered_info.bestMove.has_value());
    REQUIRE(ordered_info.scoreCp == unordered_info.scoreCp);
    REQUIRE(ordered_info.depth == unordered_info.depth);
    REQUIRE(ordered_info.nodes <= unordered_info.nodes);
    REQUIRE(position.IsLegal(*ordered_info.bestMove));
    REQUIRE(position.IsLegal(*unordered_info.bestMove));
}

TEST_CASE("node limits and pre-set stop return a legal fallback", "[search]") {
    chess::Position position;
    position.SetupInitial();

    chess::SearchLimits limited;
    limited.depth = 6;
    limited.nodeLimit = 500;
    std::atomic_bool stop{false};
    const chess::SearchInfo limited_info =
        chess::Searcher().Search(position, limited, stop);
    REQUIRE(limited_info.bestMove.has_value());
    REQUIRE(limited_info.nodes <= 500);
    REQUIRE(position.IsLegal(*limited_info.bestMove));

    chess::SearchLimits deep;
    deep.depth = 6;
    std::atomic_bool already_stopped{true};
    const chess::SearchInfo stopped_info =
        chess::Searcher().Search(position, deep, already_stopped);
    REQUIRE(stopped_info.depth == 0);
    REQUIRE(stopped_info.bestMove.has_value());
    REQUIRE(position.IsLegal(*stopped_info.bestMove));
}

TEST_CASE("a drawn root reports a draw score without losing the legal move",
          "[search]") {
    const chess::Position position =
        FromFen("4k3/8/8/8/8/8/4P3/4K3 w - - 100 1");
    REQUIRE(position.Result() == chess::GameResult::DrawFiftyMove);

    chess::SearchLimits limits;
    limits.depth = 2;
    std::atomic_bool stop{false};
    const chess::SearchInfo info =
        chess::Searcher().Search(position, limits, stop);

    REQUIRE(info.scoreCp == 0);
    REQUIRE(info.bestMove.has_value());
    REQUIRE(position.IsLegal(*info.bestMove));
}
