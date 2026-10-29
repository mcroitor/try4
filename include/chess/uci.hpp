#ifndef CHESS_UCI_HPP_
#define CHESS_UCI_HPP_

#include <atomic>
#include <cstddef>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <istream>
#include <memory>
#include <mutex>
#include <optional>
#include <ostream>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "chess/opening_book.hpp"
#include "chess/position.hpp"
#include "chess/search.hpp"

namespace chess {

// UCI option definitions for the engine.
struct UciOptions {
    std::size_t hash_mb = 16;
    int move_overhead_ms = 20;
    // Threads fixed at 1 for v1; field present for future compatibility.
    int threads = 1;

    // Task 058: resign threshold in centipawns, from the side to move's
    // perspective. 0 or any positive value disables resignation. The effective
    // range is -100000..0; a negative mate score resigns under the threshold.
    int resign_cp = -600;

    // Opening book options.
    std::string book_path;        // Path to book file (.bin or .txt)
    bool book_enabled = true;     // Whether to use book
    int book_max_ply = 40;        // Maximum ply to use book (0 = unlimited)
    EvaluationParameters evaluation;
    ForcingSearchSettings forcing;
};

// Result of parsing a `go` command.
struct GoCommand {
    SearchLimits limits;
};

// Task 058: the score-side resignation gate. The caller must satisfy the
// context exclusions (a completed search, not a cancelled or infinite search,
// a searched move rather than a book move); this helper holds only the pure
// score comparison with the disable-at-zero rule and the documented range.
inline bool ShouldResign(const SearchInfo& info, int resign_cp) {
    return resign_cp >= -100000 && resign_cp < 0 && info.depth > 0 &&
           info.scoreCp < resign_cp;
}

// UCI engine adapter. Handles the UCI protocol handshake and command loop.
// The engine runs on a background thread; commands are processed on the
// calling thread (typically stdin).
class UciEngine {
public:
    explicit UciEngine(const UciOptions& options = {});
    ~UciEngine();

    // Non-copyable, movable.
    UciEngine(const UciEngine&) = delete;
    UciEngine& operator=(const UciEngine&) = delete;
    UciEngine(UciEngine&&) = delete;
    UciEngine& operator=(UciEngine&&) = delete;

    // Starts the command loop reading from `input` and writing to `output`.
    // Returns when `quit` is received or the stream ends.
    int Run(std::istream& input, std::ostream& output);

    // Stops any ongoing search and waits for the search thread to finish.
    void Stop();

    // Sets options at runtime (called from `setoption` command).
    void SetOption(const std::string& name, const std::string& value);

private:
    // Command handlers.
    void HandleUci(std::ostream& out);
    void HandleIsReady(std::ostream& out);
    void HandleSetOption(const std::string& args);
    void HandleUciNewGame(std::ostream& out);
    void HandlePosition(const std::string& args);
    void HandleGo(std::ostream& out, const std::string& args);
    void HandleStop();
    void HandlePonderHit();
    void HandleQuit();

    // Parsing helpers.
    static std::optional<GoCommand> ParseGoCommand(const std::string& args);

    // Search worker.
    void SearchWorker(const Position& position, const SearchLimits& limits,
                      std::ostream& out);
    void EmitInfo(std::ostream& out, const SearchInfo& info, int depth,
                  std::uint64_t nodes, std::int64_t elapsed_ms);
    void EmitBestMove(std::ostream& out, const SearchInfo& info,
                      bool resign = false);

    // State.
    UciOptions options_;
    UciOptions startup_options_;
    Position position_;
    std::unique_ptr<Searcher> searcher_;
    std::unique_ptr<OpeningBook> book_;
    std::string book_error_;
    std::mt19937_64 book_random_{std::random_device{}()};
    std::mutex output_mutex_;
    std::mutex stop_mutex_;
    std::condition_variable stop_condition_;
    std::thread search_thread_;
    std::atomic_bool stop_flag_{false};
    std::atomic_bool searching_{false};
    std::optional<SearchLimits> current_limits_;
    Position search_position_copy_;
    std::ostream* output_{nullptr};
};

}  // namespace chess

#endif  // CHESS_UCI_HPP_
