#include "chess/opening_book.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <filesystem>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <utility>

#include "chess/notation.hpp"

namespace chess {
namespace {

#include "polyglot_random.inc"

struct Entry {
    std::string uci;
    std::uint32_t weight;
    std::uint32_t learn;
};

class IndexedBook : public OpeningBook {
public:
    explicit IndexedBook(std::string name) : name_(std::move(name)) {}

    void Add(std::uint64_t key, std::string uci, std::uint32_t weight,
             std::uint32_t learn = 0) {
        auto& entries = entries_[key];
        for (auto& entry : entries) {
            if (entry.uci == uci) {
                const auto sum = static_cast<std::uint64_t>(entry.weight) + weight;
                entry.weight = static_cast<std::uint32_t>(std::min<std::uint64_t>(
                    sum, std::numeric_limits<std::uint32_t>::max()));
                return;
            }
        }
        entries.push_back({std::move(uci), weight, learn});
    }

    std::vector<BookMove> FindMoves(const Position& position) const override {
        std::vector<BookMove> result;
        const auto found = entries_.find(PolyglotKey(position));
        if (found == entries_.end()) {
            return result;
        }
        // Match against canonical legal moves to recover capture, double-push,
        // castling and en-passant flags. Never trust moves from a book file.
        const auto legal = position.LegalMoves();
        for (const auto& entry : found->second) {
            if (entry.weight == 0) {
                continue;  // Polyglot uses zero weight for deleted entries.
            }
            for (const auto& move : legal) {
                std::string castle;
                if (move.HasFlag(MoveFlag::CastleKing)) {
                    castle = move.from.ToString() + "h" + move.from.ToString().substr(1);
                } else if (move.HasFlag(MoveFlag::CastleQueen)) {
                    castle = move.from.ToString() + "a" + move.from.ToString().substr(1);
                }
                if (move.ToUci() == entry.uci || (!castle.empty() && castle == entry.uci)) {
                    result.push_back({move, entry.weight, entry.learn});
                    break;
                }
            }
        }
        return result;
    }

    std::string Name() const override { return name_; }
    bool IsLoaded() const override { return true; }

private:
    std::string name_;
    std::unordered_map<std::uint64_t, std::vector<Entry>> entries_;
};

std::unique_ptr<OpeningBook> Fail(std::string* error, const std::string& message) {
    if (error != nullptr) {
        *error = message;
    }
    return nullptr;
}

std::uint64_t ReadBigEndian(const unsigned char* bytes, int count) {
    std::uint64_t value = 0;
    for (int i = 0; i < count; ++i) {
        value = (value << 8) | bytes[i];
    }
    return value;
}

std::string DecodeMove(std::uint16_t encoded) {
    if ((encoded & 0x8000) != 0 || ((encoded >> 12) & 7) > 4) {
        return {};
    }
    const Square from(static_cast<std::uint8_t>((encoded >> 6) & 63));
    const Square to(static_cast<std::uint8_t>(encoded & 63));
    std::string uci = from.ToString() + to.ToString();
    const int promotion = (encoded >> 12) & 7;
    if (promotion != 0) {
        uci += " nbrq"[promotion];
    }
    return uci;
}

}  // namespace

std::uint64_t PolyglotKey(const Position& position) {
    std::uint64_t key = 0;
    for (int i = 0; i < 64; ++i) {
        const auto piece = position.PieceAt(Square(static_cast<std::uint8_t>(i)));
        if (!piece.IsEmpty()) {
            const int index = (static_cast<int>(piece.Type()) - 1) * 2 +
                              (piece.Color() == Color::White ? 1 : 0);
            key ^= kPolyglotRandom[index * 64 + i];
        }
    }
    for (int i = 0; i < 4; ++i) {
        if ((position.CastlingRights() & (1U << i)) != 0) {
            key ^= kPolyglotRandom[768 + i];
        }
    }
    if (const auto ep = position.EnPassant(); ep.has_value()) {
        const int rank = ep->Rank() + (position.SideToMove() == Color::White ? -1 : 1);
        for (const int offset : {-1, 1}) {
            const int file = ep->File() + offset;
            if (file >= 0 && file < 8 && rank >= 0 && rank < 8 &&
                position.PieceAt(Square(static_cast<std::uint8_t>(rank * 8 + file))) ==
                    Piece(position.SideToMove(), PieceType::Pawn)) {
                // Pseudo-legal capture suffices, even if the pawn is pinned.
                key ^= kPolyglotRandom[772 + ep->File()];
                break;
            }
        }
    }
    if (position.SideToMove() == Color::White) {
        key ^= kPolyglotRandom[780];
    }
    return key;
}

std::unique_ptr<OpeningBook> LoadPolyglotBook(const std::string& path,
                                          std::string* error) {
    if (error != nullptr) {
        error->clear();
    }
    std::ifstream file(std::filesystem::u8path(path), std::ios::binary);
    if (!file) {
        return Fail(error, "cannot open book: " + path);
    }
    auto book = std::make_unique<IndexedBook>("Polyglot: " + path);
    std::array<unsigned char, 16> bytes{};
    std::size_t count = 0;
    while (file.read(reinterpret_cast<char*>(bytes.data()), bytes.size())) {
        const auto key = ReadBigEndian(bytes.data(), 8);
        const auto move = static_cast<std::uint16_t>(ReadBigEndian(bytes.data() + 8, 2));
        const auto uci = DecodeMove(move);
        if (!uci.empty()) {
            book->Add(key, uci,
                      static_cast<std::uint32_t>(ReadBigEndian(bytes.data() + 10, 2)),
                      static_cast<std::uint32_t>(ReadBigEndian(bytes.data() + 12, 4)));
        }
        ++count;
    }
    if (!file.eof() || file.gcount() != 0 || count == 0) {
        return Fail(error, "empty, unreadable or truncated Polyglot book: " + path);
    }
    return book;
}

std::unique_ptr<OpeningBook> LoadTournamentBook(const std::string& path,
                                            std::string* error) {
    if (error != nullptr) {
        error->clear();
    }
    std::ifstream file(std::filesystem::u8path(path));
    if (!file) {
        return Fail(error, "cannot open book: " + path);
    }
    auto book = std::make_unique<IndexedBook>("Tournament: " + path);
    std::string line;
    std::size_t number = 0;
    std::size_t count = 0;
    while (std::getline(file, line)) {
        ++number;
        std::istringstream stream(line);
        stream >> std::ws;
        if (stream.peek() == '#' || stream.eof()) {
            continue;
        }
        std::string id;
        stream >> id;
        Position position;
        position.SetupInitial();
        std::string text;
        bool has_move = false;
        while (stream >> text) {
            const auto move = ParseUci(position, text);
            if (!move.has_value()) {
                return Fail(error, "invalid move " + text + " at line " +
                            std::to_string(number) + " (" + id + ")");
            }
            book->Add(PolyglotKey(position), move->ToUci(), 1);
            if (!position.MakeMove(*move)) {
                return Fail(error, "illegal move at line " + std::to_string(number));
            }
            has_move = true;
        }
        if (!has_move) {
            return Fail(error, "missing moves at line " + std::to_string(number));
        }
        ++count;
    }
    if (!file.eof() || count == 0) {
        return Fail(error, "empty or unreadable tournament book: " + path);
    }
    return book;
}

std::unique_ptr<OpeningBook> LoadBookAuto(const std::string& path, std::string* error) {
    const auto dot = path.find_last_of('.');
    std::string extension = dot == std::string::npos ? "" : path.substr(dot);
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (extension == ".bin") {
        return LoadPolyglotBook(path, error);
    }
    if (extension == ".txt") {
        return LoadTournamentBook(path, error);
    }
    return Fail(error, "unsupported book extension (use .bin or .txt): " + path);
}

}  // namespace chess
