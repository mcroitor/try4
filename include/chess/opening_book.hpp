#ifndef CHESS_OPENING_BOOK_HPP_
#define CHESS_OPENING_BOOK_HPP_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "chess/position.hpp"
#include "chess/types.hpp"

namespace chess {

// A single book move with its weight/learn data.
struct BookMove {
    Move move;
    std::uint32_t weight = 0;      // Play probability weight
    std::uint32_t learn = 0;       // Learn value (Polyglot)
};

// Abstract opening book interface.
class OpeningBook {
public:
    virtual ~OpeningBook() = default;

    // Find book moves for the given position.
    // Returns empty vector if position not in book or book exhausted.
    virtual std::vector<BookMove> FindMoves(const Position& position) const = 0;

    // Get book name/description for UCI 'id' output.
    virtual std::string Name() const = 0;

    // Whether the book is loaded and ready.
    virtual bool IsLoaded() const = 0;
};

// Factory functions.

// Load a Polyglot (.bin) opening book.
// Returns null on failure (file not found, corrupt, etc.).
std::unique_ptr<OpeningBook> LoadPolyglotBook(const std::string& path,
                                          std::string* error = nullptr);

// Load a tournament.txt format book (try4's text format).
// Format: <id><TAB><space-separated UCI moves>
// Returns null on failure.
std::unique_ptr<OpeningBook> LoadTournamentBook(const std::string& path,
                                            std::string* error = nullptr);

// Load a book by auto-detecting format from file extension.
// .bin -> Polyglot, .txt -> tournament.txt
std::unique_ptr<OpeningBook> LoadBookAuto(const std::string& path,
                                      std::string* error = nullptr);

// Standard Polyglot key, independent of the engine's internal Zobrist hash.
std::uint64_t PolyglotKey(const Position& position);

}  // namespace chess

#endif  // CHESS_OPENING_BOOK_HPP_
