#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "chess/notation.hpp"
#include "chess/opening_book.hpp"
#include "chess/uci.hpp"

namespace {

class BookFile {
public:
    explicit BookFile(const std::string& extension) {
        static int counter = 0;
        path = std::filesystem::temp_directory_path() /
               ("try4 book " + std::to_string(
                   std::chrono::steady_clock::now().time_since_epoch().count()) +
                "-" + std::to_string(++counter) + extension);
    }
    ~BookFile() {
        std::error_code error;
        std::filesystem::remove(path, error);
    }
    void Text(const std::string& text) const { std::ofstream(path) << text; }
    std::filesystem::path path;
};

void WriteNumber(std::ostream& out, std::uint64_t value, int bytes) {
    for (int i = bytes - 1; i >= 0; --i) {
        out.put(static_cast<char>((value >> (i * 8)) & 255));
    }
}

void Entry(std::ostream& out, std::uint64_t key, const std::string& uci,
           int weight = 10, int promotion = 0) {
    const auto from = chess::Square::Parse(uci.substr(0, 2));
    const auto to = chess::Square::Parse(uci.substr(2, 2));
    REQUIRE(from);
    REQUIRE(to);
    WriteNumber(out, key, 8);
    WriteNumber(out, (promotion << 12) | (from->Index() << 6) | to->Index(), 2);
    WriteNumber(out, static_cast<std::uint64_t>(weight), 2);
    WriteNumber(out, 0x12345678, 4);
}

std::string Session(const std::string& script) {
    std::istringstream input(script);
    std::ostringstream output;
    chess::UciEngine engine;
    engine.Run(input, output);
    return output.str();
}

}  // namespace

TEST_CASE("Polyglot hash matches published reference sequence", "[book]") {
    chess::Position position;
    position.SetupInitial();
    REQUIRE(chess::PolyglotKey(position) == 0x463b96181691fc9cULL);
    const std::pair<const char*, std::uint64_t> sequence[] = {
        {"e2e4", 0x823c9b50fd114196ULL},
        {"d7d5", 0x0756b94461c50fb0ULL},
        {"e4e5", 0x662fafb965db29d4ULL},
        {"f7f5", 0x22a48b5a8e47ff78ULL},
        {"e1e2", 0x652a607ca3f242c1ULL},
    };
    for (const auto& item : sequence) {
        const auto move = chess::ParseUci(position, item.first);
        REQUIRE(move);
        REQUIRE(position.MakeMove(*move));
        REQUIRE(chess::PolyglotKey(position) == item.second);
    }
}

TEST_CASE("Polyglot en passant hashes pseudo-legal captures including pinned pawns", "[book]") {
    const auto position = chess::Position::FromFen("k3r3/8/8/3pP3/8/8/8/4K3 w - d6 0 2");
    REQUIRE(position);
    auto without_ep = *position;
    without_ep.SetEnPassant(std::nullopt);
    REQUIRE(chess::PolyglotKey(*position) != chess::PolyglotKey(without_ep));
    REQUIRE_FALSE(chess::ParseUci(*position, "e5d6"));
}

TEST_CASE("Polyglot binary decoding recovers all special move flags", "[book]") {
    const std::pair<const char*, const char*> cases[] = {
        {"r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1", "e1h1"},
        {"r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1", "e1a1"},
        {"r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1", "e8h8"},
        {"r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1", "e8a8"},
        {"7k/P7/8/8/8/8/8/7K w - - 0 1", "a7a8q"},
        {"7k/P7/8/8/8/8/8/7K w - - 0 1", "a7a8n"},
        {"7k/P7/8/8/8/8/8/7K w - - 0 1", "a7a8b"},
        {"7k/P7/8/8/8/8/8/7K w - - 0 1", "a7a8r"},
        {"7k/8/8/3pP3/8/8/8/7K w - d6 0 2", "e5d6"},
        {"7k/8/8/8/8/8/4P3/7K w - - 0 1", "e2e4"},
        {"7k/8/8/8/8/8/8/4R2K w - - 0 1", "e1a1"},
    };
    for (const auto& item : cases) {
        INFO(item.first << " " << item.second);
        const auto position = chess::Position::FromFen(item.first);
        REQUIRE(position);
        BookFile file(".bin");
        {
            std::ofstream out(file.path, std::ios::binary);
            const std::string move = item.second;
            Entry(out, chess::PolyglotKey(*position), move, 10,
                  move.size() == 5 ? static_cast<int>(std::string(" nbrq").find(move.back())) : 0);
        }
        const auto book = chess::LoadBookAuto(file.path.string());
        REQUIRE(book);
        const auto moves = book->FindMoves(*position);
        REQUIRE(moves.size() == 1);
        REQUIRE(position->IsLegal(moves.front().move));
        REQUIRE(moves.front().learn == 0x12345678);
        auto next = *position;
        REQUIRE(next.MakeMove(moves.front().move));
    }
}

TEST_CASE("Polyglot ignores illegal and zero weight entries and rejects truncation", "[book]") {
    BookFile file(".bin");
    {
        std::ofstream out(file.path, std::ios::binary);
        Entry(out, 0x463b96181691fc9cULL, "e2e5", 100);
        Entry(out, 0x463b96181691fc9cULL, "d2d4", 0);
        Entry(out, 0x463b96181691fc9cULL, "e2e4", 20);
    }
    chess::Position position;
    position.SetupInitial();
    const auto book = chess::LoadBookAuto(file.path.string());
    REQUIRE(book);
    const auto moves = book->FindMoves(position);
    REQUIRE(moves.size() == 1);
    REQUIRE(moves.front().move.ToUci() == "e2e4");
    REQUIRE(moves.front().weight == 20);
    std::ofstream(file.path, std::ios::binary | std::ios::app).put('x');
    std::string error;
    REQUIRE_FALSE(chess::LoadBookAuto(file.path.string(), &error));
    REQUIRE_FALSE(error.empty());
}

TEST_CASE("Text book aggregates variants and matches transposed FEN positions", "[book]") {
    BookFile file(".txt");
    file.Text("  # comment\n\nline-1\tg1f3 g8f6 d2d4 d7d5 c2c4\n"
              "line-2\tg1f3 d7d5 d2d4 g8f6 c2c4\n"
              "line-3\td2d4 d7d5\n");
    const auto book = chess::LoadBookAuto(file.path.string());
    REQUIRE(book);
    chess::Position position;
    position.SetupInitial();
    const auto moves = book->FindMoves(position);
    REQUIRE(moves.size() == 2);
    REQUIRE(moves.front().weight == 2);
    const auto transposed = chess::Position::FromFen(
        "rnbqkb1r/ppp1pppp/5n2/3p4/3P4/5N2/PPP1PPPP/RNBQKB1R w KQkq - 7 9");
    REQUIRE(transposed);
    const auto next = book->FindMoves(*transposed);
    REQUIRE(next.size() == 1);
    REQUIRE(next.front().move.ToUci() == "c2c4");
    REQUIRE(next.front().weight == 2);
}

TEST_CASE("Malformed text book fails instead of accepting a partial line", "[book]") {
    BookFile file(".txt");
    for (const auto* text : {"", "# only comment\n", "tag\n", "tag\te2e4 e7e4\n"}) {
        file.Text(text);
        std::string error;
        REQUIRE_FALSE(chess::LoadBookAuto(file.path.string(), &error));
        REQUIRE_FALSE(error.empty());
    }
}

TEST_CASE("Repository tournament book loads and probes", "[book]") {
    const auto book = chess::LoadTournamentBook(
        std::string(TRY4_SOURCE_DIR) + "/docs/selfplay/tournament.txt");
    REQUIRE(book);
    chess::Position position;
    position.SetupInitial();
    REQUIRE_FALSE(book->FindMoves(position).empty());
}

TEST_CASE("UCI selects book moves then falls back when disabled exhausted or limited", "[book][uci]") {
    BookFile file(".txt");
    file.Text("only\te2e4 e7e5\n");
    const std::string setup = "setoption name BookFile value " + file.path.string() + "\n";
    auto out = Session(setup + "position startpos\ngo depth 1\nquit\n");
    REQUIRE(out.find("Book move e2e4\nbestmove e2e4\n") != std::string::npos);
    REQUIRE(out.find("info depth") == std::string::npos);
    out = Session(setup + "position startpos moves e2e4\ngo depth 1\nquit\n");
    REQUIRE(out.find("bestmove e7e5") != std::string::npos);
    out = Session(setup + "setoption name BookMaxPly value 0\n"
                  "position fen rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 1\n"
                  "go depth 1\nquit\n");
    REQUIRE(out.find("Book move") == std::string::npos);  // Line is exhausted.
    out = Session(setup + "setoption name BookMaxPly value 0\n"
                  "position fen rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 50\n"
                  "go depth 1\nquit\n");
    REQUIRE(out.find("bestmove e7e5") != std::string::npos);
    for (const auto& script : {
        "setoption name OwnBook value false\nposition startpos\n",
        "position startpos moves e2e4 e7e5\n",
        "setoption name BookMaxPly value 1\nposition startpos moves e2e4\n",
        "setoption name BookFile value <empty>\nposition startpos\n"}) {
        out = Session(setup + script + "go depth 1\nquit\n");
        REQUIRE(out.find("Book move") == std::string::npos);
        REQUIRE(out.find("info depth") != std::string::npos);
        REQUIRE(out.find("bestmove ") != std::string::npos);
    }
    out = Session(setup + "go infinite\nstop\nquit\n");
    REQUIRE(out.find("Book move") == std::string::npos);
    out = Session(setup + "setoption name BookFile value missing.bin\ngo depth 1\nquit\n");
    REQUIRE(out.find("Book load failed") != std::string::npos);
    REQUIRE(out.find("Book move") == std::string::npos);
    REQUIRE(out.find("bestmove ") != std::string::npos);
}

TEST_CASE("UCI probes a real Polyglot record by its standard key", "[book][uci]") {
    BookFile file(".bin");
    {
        std::ofstream out(file.path, std::ios::binary);
        Entry(out, 0x463b96181691fc9cULL, "d2d4");
    }
    const auto out = Session("setoption name BookFile value " + file.path.string() +
                             "\nposition startpos\ngo depth 1\nquit\n");
    REQUIRE(out.find("Book move d2d4\nbestmove d2d4\n") != std::string::npos);
}
