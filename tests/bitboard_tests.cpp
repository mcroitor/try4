#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <optional>
#include <vector>

#include "chess/bitboard.hpp"
#include "test_support.hpp"

using chess::bitboard::Any;
using chess::bitboard::Board;
using chess::bitboard::Count;
using chess::bitboard::HighestSquare;
using chess::bitboard::IsSet;
using chess::bitboard::LowestSquare;
using chess::bitboard::Move;
using chess::bitboard::NextSquare;
using chess::bitboard::SquareMask;
using chess::bitboard::Without;
using chess::bitboard::With;
using chess::Square;

TEST_CASE("bitboard square masks", "[bitboard]") {
    REQUIRE(SquareMask(Square(0)) == 1);
    REQUIRE(SquareMask(chess::Square(1)) == 2);
    REQUIRE(SquareMask(chess::Square(7)) == 0x80);
    REQUIRE(SquareMask(chess::Square(63)) == 0x8000000000000000ULL);

    REQUIRE(SquareMask(Square()) == 0);
    REQUIRE(SquareMask(Square(64)) == 0);
}

TEST_CASE("bitboard set and clear semantics", "[bitboard]") {
    const Board a1 = SquareMask(try4_test::Sq("a1"));
    const Board h8 = SquareMask(try4_test::Sq("h8"));
    const Board c4 = SquareMask(try4_test::Sq("c4"));

    REQUIRE(IsSet(a1, try4_test::Sq("a1")));
    REQUIRE_FALSE(IsSet(a1, try4_test::Sq("a2")));
    REQUIRE_FALSE(IsSet(a1, Square()));

    REQUIRE(With(h8, try4_test::Sq("c4")) == (h8 | c4));
    REQUIRE(Without(h8, try4_test::Sq("h8")) == 0);
    REQUIRE(Without(h8, try4_test::Sq("c4")) == h8);
    REQUIRE(Without(0, try4_test::Sq("a1")) == 0);
}

TEST_CASE("bitboard move preserves other squares", "[bitboard]") {
    const Board board = With(With(0, try4_test::Sq("a1")),
                            try4_test::Sq("h8"));
    const Board moved = Move(board, try4_test::Sq("a1"), try4_test::Sq("b1"));

    REQUIRE_FALSE(IsSet(moved, try4_test::Sq("a1")));
    REQUIRE(IsSet(moved, try4_test::Sq("b1")));
    REQUIRE(IsSet(moved, try4_test::Sq("h8")));
    REQUIRE(Count(moved) == 2);
}

TEST_CASE("bitboard counting and iteration", "[bitboard]") {
    Board board = With(With(With(0, try4_test::Sq("a1")),
                            try4_test::Sq("h8")),
                    try4_test::Sq("f2"));
    REQUIRE(Count(board) == 3);

    REQUIRE(LowestSquare(board).value() == try4_test::Sq("a1"));
    REQUIRE(HighestSquare(board).value() == try4_test::Sq("h8"));

    std::vector<std::string> squares;
    while (auto square = NextSquare(board)) {
        squares.push_back(square->ToString());
    }
    REQUIRE_FALSE(Any(board));
    REQUIRE(squares == std::vector<std::string>{"a1", "f2", "h8"});
}

TEST_CASE("bitboard helpers never corrupt the board on invalid input", "[bitboard]") {
    const Board board = With(0, try4_test::Sq("a1"));
    Square invalid = Square();
    REQUIRE(IsSet(board, invalid) == false);
    REQUIRE(Without(board, invalid) == board);
    REQUIRE(With(board, invalid) == board);
    REQUIRE(Move(board, invalid, try4_test::Sq("a2")) == board);
    REQUIRE_FALSE(NextSquare(invalid).has_value());
}