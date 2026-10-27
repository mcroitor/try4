#ifndef CHESS_NOTATION_HPP_
#define CHESS_NOTATION_HPP_

#include <optional>
#include <string>
#include <string_view>

#include "chess/position.hpp"

namespace chess {

// Parses coordinate notation and returns the corresponding legal move.
// Castling is written as the king move; en passant uses the target square.
std::optional<Move> ParseUci(const Position& position, std::string_view text);

// Formats a legal move in SAN, including disambiguation, capture, promotion,
// check, and mate markers.
std::string ToSan(const Position& position, const Move& move);

// Parses SAN by matching against the legal moves of the position. A trailing
// '+' or '#' is optional. Returns std::nullopt for illegal or ambiguous input.
std::optional<Move> ParseSan(const Position& position, std::string_view text);

}  // namespace chess

#endif  // CHESS_NOTATION_HPP_
