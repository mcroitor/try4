#include <catch2/catch_test_macros.hpp>

#include <string>

#include "chess/evaluation.hpp"
#include "chess/position.hpp"

namespace {

chess::Position FromFen(const char* fen) {
    std::string error;
    const auto position = chess::Position::FromFen(fen, &error);
    INFO("FEN: " << fen);
    INFO("error: " << error);
    REQUIRE(position.has_value());
    return *position;
}

void CheckTaperFormula(const chess::EvaluationBreakdown& evaluation) {
    const int expected =
        (evaluation.middlegame * evaluation.phase +
         evaluation.endgame * (24 - evaluation.phase)) /
        24 +
        evaluation.tempo;
    CHECK(evaluation.score == expected);
}

}  // namespace

TEST_CASE("symmetric positions have only the tempo bonus", "[evaluation]") {
    chess::Position initial;
    initial.SetupInitial();
    const chess::EvaluationBreakdown start = chess::EvaluateDetailed(initial);

    CHECK(start.phase == 24);
    CHECK(start.middlegame == 0);
    CHECK(start.endgame == 0);
    CHECK(start.tempo == chess::kTempoBonus);
    CHECK(start.score == chess::kTempoBonus);
    CHECK(chess::Evaluate(initial) == chess::kTempoBonus);
    CHECK(chess::EvaluateMaterial(initial) == 0);
    CHECK(start.pawn_structure_mg == 0);
    CHECK(start.pawn_structure_eg == 0);
    CHECK(start.mobility_mg == 0);
    CHECK(start.mobility_eg == 0);
    CHECK(start.bishop_pair_mg == 0);
    CHECK(start.bishop_pair_eg == 0);
    CHECK(start.rook_files_mg == 0);
    CHECK(start.rook_files_eg == 0);
    CHECK(start.king_safety_mg == 0);
    CHECK(start.king_safety_eg == 0);
    CHECK(chess::PieceValue(chess::PieceType::Pawn) == 100);
    CHECK(chess::PieceValue(chess::PieceType::Knight) == 320);
    CHECK(chess::PieceValue(chess::PieceType::Bishop) == 330);
    CHECK(chess::PieceValue(chess::PieceType::Rook) == 500);
    CHECK(chess::PieceValue(chess::PieceType::Queen) == 900);
    CHECK(chess::PieceValue(chess::PieceType::King) == 0);
    CHECK(chess::PieceValue(chess::PieceType::None) == 0);

    const chess::Position rooks =
        FromFen("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
    const chess::EvaluationBreakdown mirrored_material =
        chess::EvaluateDetailed(rooks);
    CHECK(mirrored_material.middlegame == 0);
    CHECK(mirrored_material.endgame == 0);
    CHECK(mirrored_material.score == chess::kTempoBonus);
    CHECK(mirrored_material.phase == 8);
}

TEST_CASE("full evaluation is symmetric under color swap", "[evaluation]") {
    // The second position is the first with colors swapped and every square
    // reflected across the middle rank, so Black has the same relative setup.
    const chess::Position white =
        FromFen("r2qk2r/pp6/2n5/4p3/4P3/5N2/PP6/R1BQK2R w KQkq - 0 1");
    const chess::Position black =
        FromFen("r1bqk2r/pp6/5n2/4p3/4P3/2N5/PP6/R2QK2R b KQkq - 0 1");

    const chess::EvaluationBreakdown white_eval =
        chess::EvaluateDetailed(white);
    const chess::EvaluationBreakdown black_eval =
        chess::EvaluateDetailed(black);
    CHECK(white_eval.middlegame == black_eval.middlegame);
    CHECK(white_eval.endgame == black_eval.endgame);
    CHECK(white_eval.phase == black_eval.phase);
    CHECK(white_eval.pawn_structure_mg == black_eval.pawn_structure_mg);
    CHECK(white_eval.pawn_structure_eg == black_eval.pawn_structure_eg);
    CHECK(white_eval.mobility_mg == black_eval.mobility_mg);
    CHECK(white_eval.mobility_eg == black_eval.mobility_eg);
    CHECK(white_eval.bishop_pair_mg == black_eval.bishop_pair_mg);
    CHECK(white_eval.rook_files_mg == black_eval.rook_files_mg);
    CHECK(white_eval.king_safety_mg == black_eval.king_safety_mg);
    CHECK(white_eval.score == black_eval.score);
    CHECK(chess::Evaluate(white) == chess::Evaluate(black));
    CheckTaperFormula(white_eval);

    // Keeping White to move while swapping colors reverses the positional
    // score but not the tempo bonus.
    const chess::Position black_same_side =
        FromFen("r1bqk2r/pp6/5n2/4p3/4P3/2N5/PP6/R2QK2R w KQkq - 0 1");
    const chess::EvaluationBreakdown same_side_eval =
        chess::EvaluateDetailed(black_same_side);
    CHECK(same_side_eval.middlegame == -white_eval.middlegame);
    CHECK(same_side_eval.endgame == -white_eval.endgame);
    CHECK(same_side_eval.mobility_mg == -white_eval.mobility_mg);
    CHECK(same_side_eval.tempo == chess::kTempoBonus);
    CHECK(chess::Evaluate(black_same_side) ==
          -chess::Evaluate(white) + 2 * chess::kTempoBonus);

    // A horizontal board mirror (no color swap) is also symmetric.
    const chess::Position file_left =
        FromFen("4k3/8/8/8/8/2N5/8/4K3 w - - 0 1");
    const chess::Position file_right =
        FromFen("4k3/8/8/8/8/5N2/8/4K3 w - - 0 1");
    CHECK(chess::Evaluate(file_left) == chess::Evaluate(file_right));

    // King-table symmetry is observable without any other material.
    const chess::Position king_white =
        FromFen("7k/8/8/8/8/3K4/8/8 w - - 0 1");
    const chess::Position king_black =
        FromFen("8/8/3k4/8/8/8/8/K7 b - - 0 1");
    CHECK(chess::Evaluate(king_white) == chess::Evaluate(king_black));
}

TEST_CASE("a queen advantage keeps the expected sign", "[evaluation]") {
    const chess::Position white_to_move =
        FromFen("4k3/8/8/8/8/8/8/4K2Q w - - 0 1");
    const chess::Position black_to_move =
        FromFen("4k3/8/8/8/8/8/8/4K2Q b - - 0 1");

    const chess::EvaluationBreakdown white_eval =
        chess::EvaluateDetailed(white_to_move);
    const chess::EvaluationBreakdown black_eval =
        chess::EvaluateDetailed(black_to_move);
    CHECK(white_eval.score > 0);
    CHECK(black_eval.score < 0);
    CHECK(white_eval.middlegame == -black_eval.middlegame);
    CHECK(white_eval.score ==
          -black_eval.score + 2 * chess::kTempoBonus);
    CheckTaperFormula(white_eval);
    CheckTaperFormula(black_eval);

    const chess::Position black_queen =
        FromFen("4k2q/8/8/8/8/8/8/4K3 b - - 0 1");
    CHECK(chess::Evaluate(black_queen) > 0);
    CHECK(chess::Evaluate(black_queen) == chess::Evaluate(white_to_move));
}

TEST_CASE("tapered score moves from middlegame to endgame", "[evaluation]") {
    chess::Position initial;
    initial.SetupInitial();
    const chess::EvaluationBreakdown start = chess::EvaluateDetailed(initial);
    CHECK(start.phase == 24);
    CHECK(start.score == start.middlegame + chess::kTempoBonus);

    const chess::Position pawn_endgame =
        FromFen("4k3/8/8/8/8/8/4P3/4K3 w - - 0 1");
    const chess::EvaluationBreakdown endgame =
        chess::EvaluateDetailed(pawn_endgame);
    CHECK(endgame.phase == 0);
    CHECK(endgame.score == endgame.endgame + chess::kTempoBonus);
    CHECK(endgame.endgame > 0);
    CHECK(endgame.middlegame > 0);

    const chess::Position mixed =
        FromFen("4k3/8/8/8/8/8/8/RNBQKBNR w - - 0 1");
    const chess::EvaluationBreakdown tapered = chess::EvaluateDetailed(mixed);
    CHECK(tapered.phase == 12);
    CHECK(tapered.phase > 0);
    CHECK(tapered.phase < 24);
    CHECK(tapered.middlegame != tapered.endgame);
    CHECK(tapered.score != tapered.middlegame);
    CHECK(tapered.score != tapered.endgame);
    CheckTaperFormula(tapered);
}

TEST_CASE("pawn structure penalizes doubled and isolated pawns, rewards passers",
          "[evaluation]") {
    const chess::Position doubled =
        FromFen("4k3/pp6/8/8/8/PP6/PP6/4K3 w - - 0 1");
    const chess::Position connected =
        FromFen("4k3/pppp4/8/8/8/8/PPPP4/4K3 w - - 0 1");
    const chess::EvaluationBreakdown doubled_eval =
        chess::EvaluateDetailed(doubled);
    const chess::EvaluationBreakdown connected_eval =
        chess::EvaluateDetailed(connected);
    CHECK(doubled_eval.pawn_structure_mg == -30);
    CHECK(doubled_eval.pawn_structure_eg == -40);
    CHECK(connected_eval.pawn_structure_mg == 0);
    CHECK(connected_eval.pawn_structure_eg == 0);
    CHECK(doubled_eval.pawn_structure_mg <
          connected_eval.pawn_structure_mg);

    const chess::Position isolated =
        FromFen("4k3/8/8/8/8/8/P1P5/4K3 w - - 0 1");
    const chess::Position adjacent =
        FromFen("4k3/8/8/8/8/8/PP6/4K3 w - - 0 1");
    const chess::EvaluationBreakdown isolated_eval =
        chess::EvaluateDetailed(isolated);
    const chess::EvaluationBreakdown adjacent_eval =
        chess::EvaluateDetailed(adjacent);
    CHECK(isolated_eval.pawn_structure_mg <
          adjacent_eval.pawn_structure_mg);
    CHECK(adjacent_eval.pawn_structure_mg -
              isolated_eval.pawn_structure_mg ==
          24);
    CHECK(adjacent_eval.pawn_structure_eg -
              isolated_eval.pawn_structure_eg ==
          30);

    const chess::Position passed_high =
        FromFen("4k3/8/8/4P3/8/8/8/4K3 w - - 0 1");
    const chess::Position passed_low =
        FromFen("4k3/8/8/8/4P3/8/8/4K3 w - - 0 1");
    const chess::Position blocked =
        FromFen("4k3/4p3/8/4P3/8/8/8/4K3 w - - 0 1");
    const chess::EvaluationBreakdown high_eval =
        chess::EvaluateDetailed(passed_high);
    const chess::EvaluationBreakdown low_eval =
        chess::EvaluateDetailed(passed_low);
    const chess::EvaluationBreakdown blocked_eval =
        chess::EvaluateDetailed(blocked);
    CHECK(high_eval.pawn_structure_mg > low_eval.pawn_structure_mg);
    CHECK(low_eval.pawn_structure_mg > blocked_eval.pawn_structure_mg);
    CHECK(high_eval.pawn_structure_mg == 23);
    CHECK(low_eval.pawn_structure_mg == 8);
    CHECK(blocked_eval.pawn_structure_mg == 0);
}

TEST_CASE("mobility counts bishop, rook, and queen moves", "[evaluation]") {
    const chess::Position open_bishop =
        FromFen("4k3/8/8/8/8/8/8/2B1K3 w - - 0 1");
    const chess::Position blocked_bishop =
        FromFen("4k3/8/8/8/8/8/1P1P4/2B1K3 w - - 0 1");
    const chess::Position open_rook =
        FromFen("4k3/8/8/8/8/8/8/R3K3 w - - 0 1");
    const chess::Position open_queen =
        FromFen("4k3/8/8/8/8/8/8/3QK3 w - - 0 1");

    const chess::EvaluationBreakdown bishop_open =
        chess::EvaluateDetailed(open_bishop);
    const chess::EvaluationBreakdown bishop_blocked =
        chess::EvaluateDetailed(blocked_bishop);
    CHECK(bishop_open.mobility_mg == 21);
    CHECK(bishop_open.mobility_eg == 14);
    CHECK(bishop_blocked.mobility_mg == 0);
    CHECK(bishop_blocked.mobility_eg == 0);
    CHECK(bishop_open.mobility_mg > bishop_blocked.mobility_mg);

    // A rook must not count diagonal moves and a bishop must not count
    // orthogonal moves.
    CHECK(chess::EvaluateDetailed(open_rook).mobility_mg == 30);
    CHECK(chess::EvaluateDetailed(open_rook).mobility_eg == 20);
    CHECK(chess::EvaluateDetailed(open_queen).mobility_mg == 51);
    CHECK(chess::EvaluateDetailed(open_queen).mobility_eg == 34);
}

TEST_CASE("bishop pair requires both bishops", "[evaluation]") {
    const chess::Position pair =
        FromFen("4k3/8/8/8/8/8/8/2B1KB2 w - - 0 1");
    const chess::Position single =
        FromFen("4k3/8/8/8/8/8/8/2B1K3 w - - 0 1");
    const chess::EvaluationBreakdown pair_eval =
        chess::EvaluateDetailed(pair);
    const chess::EvaluationBreakdown single_eval =
        chess::EvaluateDetailed(single);
    CHECK(pair_eval.bishop_pair_mg == 30);
    CHECK(pair_eval.bishop_pair_eg == 50);
    CHECK(single_eval.bishop_pair_mg == 0);
    CHECK(single_eval.bishop_pair_eg == 0);
    CHECK(pair_eval.bishop_pair_mg > single_eval.bishop_pair_mg);
}

TEST_CASE("rook files distinguish open, semi-open, and closed files",
          "[evaluation]") {
    const chess::Position open =
        FromFen("4k3/8/8/8/8/8/8/R3K3 w - - 0 1");
    const chess::Position semi_open =
        FromFen("4k3/p7/8/8/8/8/8/R3K3 w - - 0 1");
    const chess::Position closed =
        FromFen("4k3/8/8/8/8/8/P7/R3K3 w - - 0 1");

    const chess::EvaluationBreakdown open_eval =
        chess::EvaluateDetailed(open);
    const chess::EvaluationBreakdown semi_eval =
        chess::EvaluateDetailed(semi_open);
    const chess::EvaluationBreakdown closed_eval =
        chess::EvaluateDetailed(closed);
    CHECK(open_eval.rook_files_mg == 20);
    CHECK(open_eval.rook_files_eg == 10);
    CHECK(semi_eval.rook_files_mg == 10);
    CHECK(semi_eval.rook_files_eg == 5);
    CHECK(closed_eval.rook_files_mg == 0);
    CHECK(closed_eval.rook_files_eg == 0);
    CHECK(open_eval.rook_files_mg > semi_eval.rook_files_mg);
    CHECK(semi_eval.rook_files_mg > closed_eval.rook_files_mg);
}

TEST_CASE("king safety rewards a pawn shield and penalizes attackers",
          "[evaluation]") {
    const chess::Position shielded =
        FromFen("6k1/8/8/8/8/8/5PPP/6K1 w - - 0 1");
    const chess::Position bare =
        FromFen("6k1/8/8/8/8/8/8/6K1 w - - 0 1");
    const chess::Position attacked =
        FromFen("6k1/8/8/8/7q/8/5PPP/6K1 w - - 0 1");

    const chess::EvaluationBreakdown shielded_eval =
        chess::EvaluateDetailed(shielded);
    const chess::EvaluationBreakdown bare_eval =
        chess::EvaluateDetailed(bare);
    const chess::EvaluationBreakdown attacked_eval =
        chess::EvaluateDetailed(attacked);
    CHECK(shielded_eval.king_safety_mg == 66);
    CHECK(shielded_eval.king_safety_eg == 15);
    CHECK(bare_eval.king_safety_mg == 0);
    CHECK(bare_eval.king_safety_eg == 0);
    CHECK(attacked_eval.king_safety_mg < shielded_eval.king_safety_mg);
    CHECK(attacked_eval.king_safety_eg < shielded_eval.king_safety_eg);
    CHECK(shielded_eval.king_safety_mg > bare_eval.king_safety_mg);

    // The shield and attacker weights are deliberately smaller in the endgame.
    CHECK(shielded_eval.king_safety_eg < shielded_eval.king_safety_mg);
    CHECK(attacked_eval.king_safety_eg < attacked_eval.king_safety_mg);
}

TEST_CASE("piece-square tables are color symmetric", "[evaluation]") {
    struct PieceCase {
        const char* name;
        const char* white_fen;
        const char* black_fen;
    };
    const PieceCase cases[] = {
        {"pawn", "4k3/8/8/8/8/3P4/8/4K3 w - - 0 1",
         "4k3/8/3p4/8/8/8/8/4K3 w - - 0 1"},
        {"knight", "4k3/8/8/8/8/3N4/8/4K3 w - - 0 1",
         "4k3/8/3n4/8/8/8/8/4K3 w - - 0 1"},
        {"bishop", "4k3/8/8/8/8/3B4/8/4K3 w - - 0 1",
         "4k3/8/3b4/8/8/8/8/4K3 w - - 0 1"},
        {"rook", "4k3/8/8/8/8/3R4/8/4K3 w - - 0 1",
         "4k3/8/3r4/8/8/8/8/4K3 w - - 0 1"},
        {"queen", "4k3/8/8/8/8/3Q4/8/4K3 w - - 0 1",
         "4k3/8/3q4/8/8/8/8/4K3 w - - 0 1"},
    };

    for (const PieceCase& piece : cases) {
        INFO("piece: " << piece.name);
        const chess::Position white = FromFen(piece.white_fen);
        const chess::Position black = FromFen(piece.black_fen);
        const chess::EvaluationBreakdown white_eval =
            chess::EvaluateDetailed(white);
        const chess::EvaluationBreakdown black_eval =
            chess::EvaluateDetailed(black);

        // Both positions have White to move. With the kings fixed, a white
        // piece and its black mirror contribute equal magnitudes and opposite
        // signs to the side-to-move score; tempo is unchanged.
        CHECK(white_eval.score ==
              -black_eval.score + 2 * chess::kTempoBonus);
        CHECK(white_eval.middlegame == -black_eval.middlegame);
        CHECK(white_eval.phase == black_eval.phase);
        CheckTaperFormula(white_eval);
        CheckTaperFormula(black_eval);
    }
}
