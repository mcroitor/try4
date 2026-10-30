#include "chess/uci.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <istream>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "chess/notation.hpp"
#include "chess/opening_book.hpp"
#include "chess/position.hpp"
#include "chess/search.hpp"
#include "chess/time_manager.hpp"
#include "chess/version.hpp"

#include <cstdio>
#include <cstdlib>

namespace chess {
namespace {

// Match-forensics hook (TRY4_TIME_DEBUG=1): logs the adapter command flow to
// stderr. Without the variable nothing is written; process tests assert a
// clean stderr, so this must stay env-gated.
void UciDebug(const char* event) {
    if (std::getenv("TRY4_TIME_DEBUG") == nullptr) {
        return;
    }
    std::fprintf(stderr, "uci: %s\n", event);
}

using Clock = std::chrono::steady_clock;
using Ms = std::chrono::milliseconds;

// Task 058: the resignation diagnostic reuses the mate-distance formatting of
// EmitInfo; a negative mate score prints via the mate notation, anything else
// as plain centipawns.
std::string ResignDiagnostic(const SearchInfo& info, int threshold) {
    std::ostringstream oss;
    if (std::abs(info.scoreCp) >= kMateThreshold) {
        const int distance = (kMateScore - std::abs(info.scoreCp) + 1) / 2;
        oss << "score mate " << (info.scoreCp < 0 ? -distance : distance);
    } else {
        oss << "score cp " << info.scoreCp;
    }
    oss << " threshold " << threshold;
    return oss.str();
}

#ifdef TRY4_SMP_POC
// Task 048 PoC, variant A (private tables): helpers hold their own Searcher
// with the configured hash divided across workers; the same position, stop
// flag and budgets as the main worker; move ordering disabled so the helper's
// tree differs from the main worker's without changing any ordering policy.
// The only cross-thread channel is this snapshot (guarded by its own mutex);
// a helper's iteration may claim the snapshot only with a strictly deeper
// depth, or the same depth and a strictly better score, which keeps the
// aggregation order deterministic: the main worker's own iteration (the
// authoritative path) always beats an equal-depth-equal-score helper.
class HelperSnapshot {
public:
    bool TryPublish(int depth, int score_cp, std::uint64_t nodes,
                    const std::vector<Move>& pv) {
        const std::lock_guard<std::mutex> lock(mutex_);
        if (depth < depth_ || (depth == depth_ && score_cp <= score_)) {
            return false;
        }
        depth_ = depth;
        score_ = score_cp;
        nodes_ = nodes;
        pv_ = pv;
        return true;
    }

    bool HasResult() const {
        const std::lock_guard<std::mutex> lock(mutex_);
        return depth_ > 0;
    }

    int Depth() const {
        const std::lock_guard<std::mutex> lock(mutex_);
        return depth_;
    }

    int ScoreCp() const {
        const std::lock_guard<std::mutex> lock(mutex_);
        return score_;
    }

    std::uint64_t NodesAtClaim() const {
        const std::lock_guard<std::mutex> lock(mutex_);
        return nodes_;
    }

    SearchInfo ToInfo() const {
        SearchInfo info;
        const std::lock_guard<std::mutex> lock(mutex_);
        info.depth = depth_;
        info.scoreCp = score_;
        info.pv = pv_;
        if (!pv_.empty()) {
            info.bestMove = pv_.front();
        }
        return info;
    }

private:
    mutable std::mutex mutex_;
    int depth_ = 0;
    int score_ = 0;
    std::uint64_t nodes_ = 0;
    std::vector<Move> pv_;
};

SearchInfo HelperWorker(const Position& position, SearchLimits limits,
                        std::size_t hash_mb_per_worker,
                        std::atomic_bool& stop, HelperSnapshot* snapshot) {
    limits.useMoveOrdering = false;
    Searcher helper(std::max<std::size_t>(1, hash_mb_per_worker));
    SearchInfo last;
    const SearchProgressCallback on_progress =
        [&](const SearchInfo& progress) {
            last = progress;
            snapshot->TryPublish(progress.depth, progress.scoreCp,
                                 progress.nodes, progress.pv);
        };
    helper.Search(position, limits, stop, on_progress);
    return last.depth > 0 ? last : snapshot->ToInfo();
}
#endif

std::string Trim(std::string_view text) {
    std::size_t begin = 0;
    while (begin < text.size() &&
           std::isspace(static_cast<unsigned char>(text[begin])) != 0) {
        ++begin;
    }
    std::size_t end = text.size();
    while (end > begin &&
           std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
        --end;
    }
    return std::string(text.substr(begin, end - begin));
}

std::vector<std::string> Split(const std::string& text, char /*delimiter*/) {
    std::vector<std::string> parts;
    std::istringstream stream(text);
    std::string token;
    while (stream >> token) {
        parts.push_back(token);
    }
    return parts;
}

std::optional<std::int64_t> ParseInt(const std::string& text) {
    if (text.empty()) {
        return std::nullopt;
    }
    std::istringstream iss(text);
    std::int64_t value = 0;
    iss >> value;
    if (iss.fail() || !iss.eof()) {
        return std::nullopt;
    }
    return value;
}

}  // namespace

UciEngine::UciEngine(const UciOptions& options)
    : options_(options), startup_options_(options), searcher_(std::make_unique<Searcher>(options.hash_mb)) {
    position_.SetupInitial();
    if (!options_.book_path.empty()) {
        book_ = LoadBookAuto(options_.book_path, &book_error_);
    }
}

UciEngine::~UciEngine() {
    Stop();
    if (search_thread_.joinable()) {
        search_thread_.join();
    }
}

void UciEngine::Stop() {
    HandleStop();
    if (search_thread_.joinable()) {
        search_thread_.join();
    }
    searching_.store(false, std::memory_order_release);
}

void UciEngine::SetOption(const std::string& name, const std::string& value) {
    // Options may replace the book or resize the TT: join before mutation.
    Stop();
    if (name == "BookFile") {
        options_.book_path = value == "<empty>" ? "" : value;
        book_.reset();
        book_error_.clear();
        if (!options_.book_path.empty()) {
            book_ = LoadBookAuto(options_.book_path, &book_error_);
        }
    } else if (name == "OwnBook") {
        if (value == "true" || value == "false") {
            options_.book_enabled = value == "true";
        }
    } else if (name == "BookMaxPly") {
        if (const auto ply = ParseInt(value); ply && *ply >= 0 && *ply <= 10000) {
            options_.book_max_ply = static_cast<int>(*ply);
        }
    }
    if (name == "Hash") {
        if (auto mb = ParseInt(value); mb.has_value() && *mb >= 1 && *mb <= 131072) {
            options_.hash_mb = static_cast<std::size_t>(*mb);
            searcher_->ResizeTranspositionTable(options_.hash_mb);
        }
    } else if (name == "Move Overhead") {
        if (auto ms = ParseInt(value); ms.has_value() && *ms >= 0 && *ms <= 5000) {
            options_.move_overhead_ms = static_cast<int>(*ms);
        }
    } else if (name == "Resign") {
        if (auto cp = ParseInt(value); cp.has_value() && *cp >= -100000 && *cp <= 0) {
            options_.resign_cp = static_cast<int>(*cp);
        }
    } else if (name == "Threads") {
        if (auto t = ParseInt(value); t.has_value() && *t >= 1 && *t <= kMaxUciThreads) {
            options_.threads = static_cast<int>(*t);
        }
    }
}

int UciEngine::Run(std::istream& input, std::ostream& output) {
    std::string line;
    while (std::getline(input, line)) {
        // Store the output stream for the search thread.
        output_ = &output;
        const std::string trimmed = Trim(line);
        if (trimmed.empty()) {
            continue;
        }

        const std::size_t separator = trimmed.find_first_of(" \t");
        const std::string command =
            trimmed.substr(0, separator);
        const std::string arguments =
            separator == std::string::npos
                ? std::string()
                : Trim(trimmed.substr(separator + 1));

        if (command == "uci") {
            HandleUci(output);
        } else if (command == "isready") {
            HandleIsReady(output);
        } else if (command == "setoption") {
            HandleSetOption(arguments);
        } else if (command == "ucinewgame") {
            HandleUciNewGame(output);
        } else if (command == "position") {
            HandlePosition(arguments);
        } else if (command == "go") {
            HandleGo(output, arguments);
        } else if (command == "stop") {
            HandleStop();
        } else if (command == "ponderhit") {
            HandlePonderHit();
        } else if (command == "quit") {
            HandleQuit();
            return 0;
        }
        // Unknown commands are silently ignored per UCI spec.
    }
    Stop();  // Join while Run's output stream is still alive, including on EOF.
    return 0;
}

void UciEngine::HandleUci(std::ostream& out) {
    const std::lock_guard<std::mutex> lock(output_mutex_);
    out << "id name try4 " << kVersion << '\n';
    out << "id author try4\n";
    out << "option name Hash type spin default " << startup_options_.hash_mb << " min 1 max 131072\n";
    out << "option name Move Overhead type spin default " << startup_options_.move_overhead_ms << " min 0 max 5000\n";
    out << "option name Threads type spin default 1 min 1 max "
        << kMaxUciThreads << '\n';
    out << "option name OwnBook type check default " << (startup_options_.book_enabled ? "true" : "false") << '\n';
    out << "option name BookFile type string default " << (startup_options_.book_path.empty() ? "<empty>" : startup_options_.book_path) << '\n';
    out << "option name BookMaxPly type spin default " << startup_options_.book_max_ply << " min 0 max 10000\n";
    out << "option name Resign type spin default " << startup_options_.resign_cp
        << " min -100000 max 0 (0 disables resignation)\n";
    out << "uciok\n" << std::flush;
}

void UciEngine::HandleIsReady(std::ostream& out) {
    const std::lock_guard<std::mutex> lock(output_mutex_);
    out << "readyok\n" << std::flush;
}

void UciEngine::HandleSetOption(const std::string& args) {
    // Format: setoption name <name> [value <value>]
    std::istringstream stream(args);
    std::string token;
    if (!(stream >> token) || token != "name") { return; }
    std::string name;
    std::string value;
    while (stream >> token) {
        if (token == "value") { std::getline(stream, value); value = Trim(value); break; }
        if (!name.empty()) { name += ' '; }
        name += token;
    }
    SetOption(name, value);
    if (name == "BookFile" && output_ != nullptr) {
        const std::lock_guard<std::mutex> lock(output_mutex_);
        if (!book_error_.empty()) {
            *output_ << "info string Book load failed: " << book_error_ << '\n';
        } else if (book_) {
            *output_ << "info string Book loaded: " << book_->Name() << '\n';
        }
        output_->flush();
    }
}

void UciEngine::HandleUciNewGame(std::ostream& /*out*/) {
    Stop();
    position_.SetupInitial();
    searcher_->ClearTranspositionTable();
}

void UciEngine::HandlePosition(const std::string& args) {
    const std::vector<std::string> tokens = Split(args, ' ');
    if (tokens.empty()) {
        return;
    }

    Position candidate;
    std::size_t index = 0;
    if (tokens[0] == "startpos") {
        candidate.SetupInitial();
        index = 1;
    } else if (tokens[0] == "fen") {
        // Find the fen string (tokens 1-6) and optional moves.
        if (tokens.size() < 7) {
            return;
        }
        std::string fen = tokens[1] + " " + tokens[2] + " " + tokens[3] + " " +
                          tokens[4] + " " + tokens[5] + " " + tokens[6];
        std::string error;
        auto parsed = Position::FromFen(fen, &error);
        if (!parsed.has_value()) {
            return;
        }
        candidate = *parsed;
        index = 7;
    } else { return; }
    if (index < tokens.size()) {
        if (tokens[index++] != "moves") { return; }
        for (; index < tokens.size(); ++index) {
            const auto move = ParseUci(candidate, tokens[index]);
            if (!move || !candidate.MakeMove(*move)) { return; }
        }
    }
    position_ = std::move(candidate);
    UciDebug("position: applied");
}

void UciEngine::HandleGo(std::ostream& out, const std::string& args) {
    auto parsed = ParseGoCommand(args);
    if (!parsed.has_value()) {
        const std::lock_guard<std::mutex> lock(output_mutex_);
        out << "info string Unsupported or invalid go command\n" << std::flush;
        return;
    }

    // Stop any ongoing search.
    UciDebug("go: parsed");
    Stop();
    UciDebug("go: stop-join done");

    const auto tokens = Split(args, ' ');
    const bool analysis = std::find(tokens.begin(), tokens.end(), "infinite") != tokens.end() ||
                          std::find(tokens.begin(), tokens.end(), "ponder") != tokens.end();
    const std::int64_t ply = (static_cast<std::int64_t>(position_.FullmoveNumber()) - 1) * 2 +
                             (position_.SideToMove() == Color::Black ? 1 : 0);
    if (!analysis && options_.book_enabled && book_ &&
        (options_.book_max_ply == 0 || ply < options_.book_max_ply) &&
        position_.Result() == GameResult::Ongoing) {
        const auto moves = book_->FindMoves(position_);
        std::uint64_t total = 0;
        for (const auto& move : moves) {
            total += move.weight;
        }
        if (total > 0) {
            auto choice = std::uniform_int_distribution<std::uint64_t>(0, total - 1)(book_random_);
            for (const auto& move : moves) {
                if (choice < move.weight) {
                    const std::lock_guard<std::mutex> lock(output_mutex_);
                    out << "info string Book move " << move.move.ToUci() << '\n'
                        << "bestmove " << move.move.ToUci() << '\n' << std::flush;
                    return;
                }
                choice -= move.weight;
            }
        }
    }

    SearchLimits limits = parsed->limits;
    limits.evaluation = options_.evaluation;
    limits.forcing = options_.forcing;
    limits.pruning = PruningSettings{};
    limits.useMoveOrdering = true;
    limits.useQuiescence = true;
    limits.useTranspositionTable = true;
    limits.usePrincipalVariation = true;
    limits.useAspirationWindows = true;

    // Apply move overhead to time limits.
    if (limits.moveTimeMs.has_value()) {
        const std::int64_t overhead = options_.move_overhead_ms;
        if (*limits.moveTimeMs > overhead) {
            limits.moveTimeMs = *limits.moveTimeMs - overhead;
        }
    }

    // Copy position and limits for the search thread (thread-safe).
    search_position_copy_ = position_;
    current_limits_ = limits;
    stop_flag_.store(false, std::memory_order_release);
    searching_.store(true, std::memory_order_release);

    // Pass the output stream to the search worker.
    UciDebug("go: worker launch");
    search_thread_ = std::thread(&UciEngine::SearchWorker, this,
                                 std::cref(search_position_copy_),
                                 std::cref(*current_limits_),
                                 std::ref(*output_));

}

void UciEngine::HandleStop() {
    {
        const std::lock_guard<std::mutex> lock(stop_mutex_);
        stop_flag_.store(true);
    }
    stop_condition_.notify_all();
}

void UciEngine::HandlePonderHit() {
    // Not implemented for v1; ignore.
}

void UciEngine::HandleQuit() {
    Stop();
}

std::optional<GoCommand> UciEngine::ParseGoCommand(const std::string& args) {
    const std::vector<std::string> tokens = Split(args, ' ');
    GoCommand cmd;
    SearchLimits& limits = cmd.limits;

    for (std::size_t i = 0; i < tokens.size(); ++i) {
        const std::string& token = tokens[i];
        if (token == "infinite") {
            limits.infinite = true;
            continue;
        }
        if (token == "ponder" || token == "searchmoves" || token == "mate") {
            return std::nullopt;  // Explicitly unsupported, never silently ignored.
        }
        if (i + 1 >= tokens.size()) { return std::nullopt; }
        const auto number = ParseInt(tokens[i + 1]);
        if (!number || *number < 0 || *number > 2147483647LL ||
            ((token == "depth" || token == "movestogo") && *number == 0) ||
            (token == "depth" && *number > 127)) { return std::nullopt; }
        if (token == "depth") {
            if (i + 1 < tokens.size()) {
                if (auto d = ParseInt(tokens[i + 1]); d.has_value()) {
                    limits.depth = static_cast<int>(*d);
                }
                ++i;
            }
        } else if (token == "nodes") {
            if (i + 1 < tokens.size()) {
                if (auto n = ParseInt(tokens[i + 1]); n.has_value()) {
                    limits.nodeLimit = static_cast<std::uint64_t>(*n);
                }
                ++i;
            }
        } else if (token == "movetime") {
            if (i + 1 < tokens.size()) {
                if (auto ms = ParseInt(tokens[i + 1]); ms.has_value()) {
                    limits.moveTimeMs = *ms;
                }
                ++i;
            }
        } else if (token == "infinite") {
            // No limits; search until stop.
        } else if (token == "wtime") {
            if (i + 1 < tokens.size()) {
                if (auto ms = ParseInt(tokens[i + 1]); ms.has_value()) {
                    limits.whiteTimeMs = *ms;
                }
                ++i;
            }
        } else if (token == "btime") {
            if (i + 1 < tokens.size()) {
                if (auto ms = ParseInt(tokens[i + 1]); ms.has_value()) {
                    limits.blackTimeMs = *ms;
                }
                ++i;
            }
        } else if (token == "winc") {
            if (i + 1 < tokens.size()) {
                if (auto ms = ParseInt(tokens[i + 1]); ms.has_value()) {
                    limits.whiteIncrementMs = *ms;
                }
                ++i;
            }
        } else if (token == "binc") {
            if (i + 1 < tokens.size()) {
                if (auto ms = ParseInt(tokens[i + 1]); ms.has_value()) {
                    limits.blackIncrementMs = *ms;
                }
                ++i;
            }
        } else if (token == "movestogo") {
            if (i + 1 < tokens.size()) {
                if (auto m = ParseInt(tokens[i + 1]); m.has_value()) {
                    limits.movesToGo = static_cast<int>(*m);
                }
                ++i;
            }
        } else { return std::nullopt; }
    }
    if (limits.infinite) {
        limits = SearchLimits{};
        limits.infinite = true;
    }
    return cmd;
}

void UciEngine::SearchWorker(const Position& position, const SearchLimits& limits,
                             std::ostream& out) {
    const auto start_time = Clock::now();

    std::atomic_bool& stop = stop_flag_;
    UciDebug("worker: search enter");
#ifdef TRY4_SMP_POC
    // Task 048 PoC, variant A (private tables): helpers beside the main
    // worker. Each helper owns a Searcher with the configured hash divided
    // across workers, the same position, budgets and broadcast stop flag;
    // ordering stays off in the helper so its tree differs from the main
    // worker's. Helpers join before any bestmove; helper nodes aggregate.
    const int workers = std::min(kMaxUciThreads, std::max(1, options_.threads));
    const int helper_count = std::max(0, workers - 1);
    HelperSnapshot snapshot;
    std::vector<std::thread> helper_threads;
    std::array<std::optional<SearchInfo>, kMaxUciThreads - 1> helper_results{};
    if (helper_count > 0) {
        const std::size_t hash_mb_per_worker =
            std::max<std::size_t>(1, options_.hash_mb /
                                         static_cast<std::size_t>(workers));
        helper_threads.reserve(static_cast<std::size_t>(helper_count));
        for (int index = 0; index < helper_count; ++index) {
            helper_threads.emplace_back([&, index]() {
                SearchLimits helper_limits = limits;
                helper_limits.useMoveOrdering = false;
                helper_results[static_cast<std::size_t>(index)] =
                    HelperWorker(position, helper_limits, hash_mb_per_worker,
                                 stop, &snapshot);
            });
        }
    }
#endif

    const SearchProgressCallback on_progress = [&](const SearchInfo& progress) {
        const auto elapsed_ms =
            std::chrono::duration_cast<Ms>(Clock::now() - start_time).count();
        EmitInfo(out, progress, progress.depth, progress.nodes, elapsed_ms);
    };
    SearchInfo info = searcher_->Search(position, limits, stop, on_progress);
    UciDebug("worker: search returned");

#ifdef TRY4_SMP_POC
    std::uint64_t aggregate_nodes = info.nodes;
    for (std::thread& thread : helper_threads) {
        thread.join();
    }
    for (const auto& helper_result : helper_results) {
        if (helper_result.has_value()) {
            aggregate_nodes += helper_result->nodes;
        }
    }
    // A helper may win only with a strictly deeper completed iteration, or
    // the same depth with a strictly better score; equal results keep the
    // main worker's iteration (deterministic). The helper's move is checked
    // against the root before it can be reported.
    if (snapshot.Depth() > info.depth ||
        (snapshot.Depth() == info.depth && snapshot.ScoreCp() > info.scoreCp)) {
        const SearchInfo candidate = snapshot.ToInfo();
        if (candidate.bestMove.has_value() &&
            position.IsLegal(*candidate.bestMove)) {
            info = candidate;
        }
    }
    info.nodes = aggregate_nodes;
    // PoC protocol: one final info line with the aggregated node total, so
    // drivers can measure aggregate NPS without parsing per-worker streams.
    if (helper_count > 0) {
        EmitInfo(out, info, info.depth, info.nodes,
                 std::chrono::duration_cast<Ms>(Clock::now() - start_time)
                     .count());
    }
#endif

    if (limits.infinite) {
        std::unique_lock<std::mutex> lock(stop_mutex_);
        stop_condition_.wait(lock, [this] { return stop_flag_.load(); });
    }

    const auto end_time = Clock::now();
    const std::int64_t elapsed_ms =
        std::chrono::duration_cast<Ms>(end_time - start_time).count();

    // A normal search already streamed every completed iteration. Emit one
    // final line only when no iteration finished (terminal root, immediate
    // stop, or a zero budget) so the protocol still has a score/PV line before
    // bestmove.
    if (info.depth == 0) {
        EmitInfo(out, info, info.depth, info.nodes, elapsed_ms);
    }

    // Task 058: resign when the completed score is below the configured
    // threshold, except for infinite analysis, user-cancelled searches, book
    // moves (handled before the search) and terminal roots. The stop flag
    // doubles as the cancel detector: only HandleStop and HandleQuit set it
    // during a search, deadline abortions keep it false.
    const bool resign = !limits.infinite && !stop_flag_.load() &&
                        info.bestMove.has_value() &&
                        ShouldResign(info, options_.resign_cp);
    if (resign) {
        const std::lock_guard<std::mutex> lock(output_mutex_);
        out << "info string resign "
            << ResignDiagnostic(info, options_.resign_cp) << '\n';
        out.flush();
    }
    EmitBestMove(out, info, resign);
    UciDebug("worker: bestmove emitted");

    searching_.store(false, std::memory_order_release);
}

void UciEngine::EmitInfo(std::ostream& out, const SearchInfo& info, int depth,
                         std::uint64_t nodes, std::int64_t elapsed_ms) {
    const std::lock_guard<std::mutex> lock(output_mutex_);
    std::ostringstream oss;
    oss << "info depth " << depth;
    if (std::abs(info.scoreCp) >= kMateThreshold) {
        const int distance = (kMateScore - std::abs(info.scoreCp) + 1) / 2;
        oss << " score mate " << (info.scoreCp < 0 ? -distance : distance);
    } else {
        oss << " score cp " << info.scoreCp;
    }
    oss << " nodes " << nodes
        << " nps " << (elapsed_ms > 0 ? (nodes * 1000 / static_cast<std::uint64_t>(elapsed_ms)) : 0)
        << " time " << elapsed_ms;

    if (!info.pv.empty()) {
        oss << " pv";
        for (const Move& move : info.pv) {
            oss << ' ' << move.ToUci();
        }
    }
    out << oss.str() << '\n';
    out.flush();
}

void UciEngine::EmitBestMove(std::ostream& out, const SearchInfo& info,
                             bool resign) {
    const std::lock_guard<std::mutex> lock(output_mutex_);
    if (info.bestMove.has_value() && !resign) {
        out << "bestmove " << info.bestMove->ToUci() << '\n';
    } else {
        // No legal moves, or task 058 resignation: `bestmove 0000` is the
        // protocol's portable no-move signal.
        out << "bestmove 0000\n";
    }
    out.flush();
}

}  // namespace chess
