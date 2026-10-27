#ifndef TRY4_TEST_SUPPORT_HPP_
#define TRY4_TEST_SUPPORT_HPP_

#include <string_view>

#include "chess/types.hpp"

namespace try4_test {

inline chess::Square Sq(std::string_view text) {
    return chess::Square::Parse(text).value();
}

inline chess::Move Move(std::string_view from, std::string_view to,
                        chess::MoveFlag flags = chess::MoveFlag::None,
                        chess::PieceType promotion = chess::PieceType::None) {
    return chess::Move(Sq(from), Sq(to), flags, promotion);
}

}  // namespace try4_test

#endif  // TRY4_TEST_SUPPORT_HPP_
