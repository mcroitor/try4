#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <sstream>
#include "chess/configuration.hpp"
#include "chess/search.hpp"
#include "chess/uci.hpp"

TEST_CASE("Configuration example and normalized snapshot preserve defaults", "[config]") {
    std::string error;
    const auto file = chess::LoadConfiguration(std::string(TRY4_SOURCE_DIR) + "/config/try4.ini", &error);
    INFO(error);
    REQUIRE(file);
    REQUIRE(chess::ConfigurationSnapshot(*file) == chess::ConfigurationSnapshot(chess::UciOptions{}));
    std::istringstream stream(chess::ConfigurationSnapshot(*file));
    const auto again = chess::ParseConfiguration(stream, "snapshot", &error);
    REQUIRE(again);
    REQUIRE(chess::ConfigurationSnapshot(*again) == chess::ConfigurationSnapshot(*file));
}

TEST_CASE("Configuration parses BOM CRLF paths and parameter arrays", "[config]") {
    std::istringstream stream("\xef\xbb\xbf# comment\r\n[uci]\r\nbook_file=C:\\book #1;test=a.bin\r\n"
        "hash_mb=32\n[evaluation]\nmaterial_mg=0,100,350,330,500,900,0\n"
        "pst_eg_percent=80\n[search]\nforcing_checks=true\n");
    std::string error;
    const auto options = chess::ParseConfiguration(stream, "memory.ini", &error);
    INFO(error);
    REQUIRE(options);
    REQUIRE(options->book_path == "C:\\book #1;test=a.bin");
    REQUIRE(options->hash_mb == 32);
    REQUIRE(options->evaluation.material_mg[2] == 350);
    REQUIRE(options->evaluation[chess::EvalWeight::PstEgPercent] == 80);
    REQUIRE(options->forcing.checks);
}

TEST_CASE("Malformed config is rejected with location and useful reason", "[config]") {
    const std::string invalid_samples[] = {
        "bad", "[bad]\na=1",
#ifdef TRY4_SMP_POC
        // Task 048 PoC: threads=2 is valid here; the 1-4 range tests live in
        // the thread-count case below.
#else
        "[uci]\nthreads=2",
#endif
        "[uci]\nhash_mb=0",
        "[uci]\nhash_mb=99999999999999999999", "[uci]\nown_book=TRUE",
        "[uci]\nhash_mb=32garbage", "[uci]\nhash_mb=32\nhash_mb=16",
        "[evaluation]\nmaterial_mg=1,2", "[evaluation]\nmaterial_mg=0,100,320,330,500,900,1",
        "[evaluation]\npassed_mg=0,1,,2,3,4,5,0", "[evaluation]\ntempo=-1",
        "[search]\nforcing_max_plies=33\nforcing_hard_plies=32", "[uci]\nbook_file=\xff"};
    for (const std::string& text : invalid_samples) {
        std::istringstream stream(text);
        std::string error;
        const auto options = chess::ParseConfiguration(stream, "bad.ini", &error);
        INFO(text);
        REQUIRE_FALSE(options);
        REQUIRE(error.find("bad.ini:") == 0);
        REQUIRE(error.size() > 12);
    }
}

TEST_CASE("Unsupported thread counts are explained", "[config]") {
#ifdef TRY4_SMP_POC
    // Task 048 PoC: 1-4 accepted (2 exercised below), everything else is
    // rejected with the range in the message.
    {
        std::istringstream stream("[uci]\nthreads=2\n");
        std::string error;
        const auto options = chess::ParseConfiguration(stream, "try4.ini", &error);
        INFO(error);
        REQUIRE(options.has_value());
        REQUIRE(options->threads == 2);
    }
    for (const std::string value : {"0", "5", "8"}) {
        std::istringstream stream("[uci]\nthreads=" + value);
        std::string error;
        const auto options = chess::ParseConfiguration(stream, "try4.ini", &error);
        INFO(error);
        REQUIRE_FALSE(options);
        REQUIRE(error.find("try4.ini:2: uci.threads=" + value) == 0);
        REQUIRE(error.find("range 1-4") != std::string::npos);
    }
#else
    for (const std::string value : {"0", "2", "8"}) {
        std::istringstream stream("[uci]\nthreads=" + value);
        std::string error;
        const auto options = chess::ParseConfiguration(stream, "try4.ini", &error);
        INFO(error);
        REQUIRE_FALSE(options);
        REQUIRE(error.find("try4.ini:2: uci.threads=" + value) == 0);
        REQUIRE(error.find("only one search thread is implemented") != std::string::npos);
    }
#endif
}

TEST_CASE("Configured evaluation changes targeted terms and isolates engines", "[config][evaluation]") {
    const auto position = chess::Position::FromFen("7k/8/8/8/8/8/P7/7K w - - 0 1");
    REQUIRE(position);
    chess::EvaluationParameters parameters;
    const auto before = chess::EvaluateDetailed(*position, parameters);
    parameters.material_eg[1] += 50;
    const auto after = chess::EvaluateDetailed(*position, parameters);
    REQUIRE(after.score == before.score + 50);
    REQUIRE(after.pawn_structure_eg == before.pawn_structure_eg);
    REQUIRE(chess::Evaluate(*position) == before.score);
    parameters[chess::EvalWeight::Tempo] += 17;
    REQUIRE(chess::Evaluate(*position, parameters) == after.score + 17);
    parameters.material_mg[1] = 10000;
    REQUIRE(chess::ValidateEvaluation(parameters));
    parameters.material_mg[6] = 1;
    REQUIRE_FALSE(chess::ValidateEvaluation(parameters));
    parameters = chess::EvaluationParameters{};
    parameters.material_mg[5] = 10000;
    parameters.material_eg[5] = 10000;
    parameters.weights.fill(1000);
    chess::Position crowded;
    crowded.Clear();
    for (std::uint8_t i = 0; i < 64; ++i) {
        REQUIRE(crowded.SetPiece(chess::Square(i), chess::Piece(chess::Color::White, chess::PieceType::Queen)));
    }
    REQUIRE(chess::ValidateEvaluation(parameters));
    REQUIRE(chess::Evaluate(crowded, parameters) == chess::kMaxStaticScore);
    REQUIRE(chess::EvaluateMaterial(crowded, parameters) == chess::kMaxStaticScore);
}

TEST_CASE("Search invalidates cached scores for evaluation and forcing policies", "[config][search]") {
    auto position = chess::Position::FromFen("7k/8/8/8/8/8/PN6/7K w - - 0 1");
    REQUIRE(position);
    chess::SearchLimits limits;
    limits.depth = 2;
    chess::Searcher reused;
    std::atomic_bool stop{false};
    const auto initial = reused.Search(*position, limits, stop);
    limits.evaluation.material_eg[2] += 150;
    limits.forcing.checks = true;
    limits.forcing.max_plies = 2;
    chess::Searcher fresh;
    const auto changed = reused.Search(*position, limits, stop);
    const auto reference = fresh.Search(*position, limits, stop);
    REQUIRE(changed.scoreCp == reference.scoreCp);
    REQUIRE(changed.scoreCp != initial.scoreCp);
    REQUIRE(*position == *chess::Position::FromFen(position->ToFen()));
}

TEST_CASE("Search parses resign threshold and records it in the snapshot", "[config][search][resign]") {
    std::istringstream input("[search]\nresign_cp=-400\n");
    std::string error;
    const auto parsed = chess::ParseConfiguration(input, "test.ini", &error);
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->resign_cp == -400);
    REQUIRE(chess::ConfigurationSnapshot(*parsed).find("resign_cp=-400") != std::string::npos);

    std::istringstream disabled("[search]\nresign_cp=0\n");
    const auto off = chess::ParseConfiguration(disabled, "test.ini", &error);
    REQUIRE(off.has_value());
    REQUIRE(off->resign_cp == 0);

    std::istringstream low("[search]\nresign_cp=-100001\n");
    const auto rejected = chess::ParseConfiguration(low, "test.ini", &error);
    REQUIRE_FALSE(rejected.has_value());
    REQUIRE(error.find("test.ini") != std::string::npos);

    std::istringstream high("[search]\nresign_cp=1\n");
    REQUIRE_FALSE(chess::ParseConfiguration(high, "test.ini", &error).has_value());

    std::istringstream malformed("[search]\nresign_cp=-600x\n");
    REQUIRE_FALSE(chess::ParseConfiguration(malformed, "test.ini", &error).has_value());

    std::istringstream duplicate("[search]\nresign_cp=-600\nresign_cp=-500\n");
    REQUIRE_FALSE(chess::ParseConfiguration(duplicate, "test.ini", &error).has_value());

    // The built-in default is the documented -600.
    chess::SearchInfo default_info;
    default_info.depth = 1;
    default_info.scoreCp = -600;
    CHECK(chess::ShouldResign(default_info, -600) == false);  // equality holds
    CHECK(chess::ShouldResign(default_info, -599) == true);   // -601 < -599
    default_info.scoreCp = -601;
    CHECK(chess::ShouldResign(default_info, -600) == true);
    CHECK(chess::ShouldResign(default_info, 0) == false);      // disabled
    CHECK(chess::ShouldResign(default_info, -100001) == false);  // out of range
    default_info.depth = 0;
    CHECK(chess::ShouldResign(default_info, -600) == false);   // no iteration
    default_info.depth = 1;
    default_info.scoreCp = chess::kMateScore - 1;
    CHECK(chess::ShouldResign(default_info, -600) == false);   // positive mate
    default_info.scoreCp = -(chess::kMateScore - 1);
    CHECK(chess::ShouldResign(default_info, -600) == true);    // negative mate
}

TEST_CASE("Search invalidates cached scores for pruning policies", "[config][search]") {
    auto position = chess::Position::FromFen("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
    REQUIRE(position);
    chess::SearchLimits limits;
    limits.depth = 3;
    chess::Searcher reused;
    std::atomic_bool stop{false};
    const auto initial = reused.Search(*position, limits, stop);
    limits.pruning.useNullMove = !limits.pruning.useNullMove;
    chess::Searcher fresh;
    const auto changed = reused.Search(*position, limits, stop);
    const auto reference = fresh.Search(*position, limits, stop);
    REQUIRE(changed.scoreCp == reference.scoreCp);
    REQUIRE(*position == *chess::Position::FromFen(position->ToFen()));
}
