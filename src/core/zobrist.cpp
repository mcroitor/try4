#include "chess/zobrist.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>

namespace chess::zobrist {
namespace {

constexpr std::uint64_t kSeed = 0x9E3779B97F4A7C15ULL;

std::uint64_t SplitMix64(std::uint64_t* state) {
    *state += 0x9E3779B97F4A7C15ULL;
    std::uint64_t value = *state;
    value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ULL;
    value = (value ^ (value >> 27)) * 0x94D049BB133111EBULL;
    return value ^ (value >> 31);
}

struct Keys {
    std::array<std::array<std::uint64_t, 64>, 12> piece{};
    std::array<std::uint64_t, 16> castling{};
    std::array<std::uint64_t, 8> en_passant{};
    std::uint64_t side = 0;
};

Keys MakeKeys() {
    Keys keys;
    std::uint64_t state = kSeed;
    for (auto& piece_type : keys.piece) {
        for (std::uint64_t& key : piece_type) {
            key = SplitMix64(&state);
        }
    }
    for (std::uint64_t& key : keys.castling) {
        key = SplitMix64(&state);
    }
    for (std::uint64_t& key : keys.en_passant) {
        key = SplitMix64(&state);
    }
    keys.side = SplitMix64(&state);
    return keys;
}

const Keys& GetKeys() {
    static const Keys keys = MakeKeys();
    return keys;
}

std::size_t PieceIndex(Color color, PieceType type) {
    const std::size_t color_offset =
        color == Color::Black ? static_cast<std::size_t>(6) : 0;
    const std::size_t type_index = static_cast<std::size_t>(type);
    assert(type_index >= 1 && type_index <= 6);
    return color_offset + type_index - 1;
}

}  // namespace

std::uint64_t PieceKey(Color color, PieceType type, Square square) {
    if (type == PieceType::None || !square.IsValid()) {
        return 0;
    }
    return GetKeys().piece[PieceIndex(color, type)][square.Index()];
}

std::uint64_t SideKey() {
    return GetKeys().side;
}

std::uint64_t CastlingKey(CastlingRights rights) {
    return GetKeys().castling[rights & 0x0F];
}

std::uint64_t EnPassantKey(int file) {
    if (file < 0 || file >= 8) {
        return 0;
    }
    return GetKeys().en_passant[static_cast<std::size_t>(file)];
}

}  // namespace chess::zobrist
