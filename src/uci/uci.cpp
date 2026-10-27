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

namespace chess {
namespace {

using Clock = std::chrono::steady_clock;
using Ms = std::chrono::milliseconds;

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
    } else if (name == "Threads") {
        if (auto t = ParseInt(value); t.has_value() && *t == 1) {
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
    out << "option name Threads type spin default 1 min 1 max 1\n";
    out << "option name OwnBook type check default " << (startup_options_.book_enabled ? "true" : "false") << '\n';
    out << "option name BookFile type string default " << (startup_options_.book_path.empty() ? "<empty>" : startup_options_.book_path) << '\n';
    out << "option name BookMaxPly type spin default " << startup_options_.book_max_ply << " min 0 max 10000\n";
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
}

void UciEngine::HandleGo(std::ostream& out, const std::string& args) {
    auto parsed = ParseGoCommand(args);
    if (!parsed.has_value()) {
        const std::lock_guard<std::mutex> lock(output_mutex_);
        out << "info string Unsupported or invalid go command\n" << std::flush;
        return;
    }

    // Stop any ongoing search.
    Stop();

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
    limits.useMoveOrdering = true;
    limits.useQuiescence = true;
    limits.useTranspositionTable = true;

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
    const SearchProgressCallback on_progress = [&](const SearchInfo& progress) {
        const auto elapsed_ms =
            std::chrono::duration_cast<Ms>(Clock::now() - start_time).count();
        EmitInfo(out, progress, progress.depth, progress.nodes, elapsed_ms);
    };
    const SearchInfo info = searcher_->Search(position, limits, stop, on_progress);
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
    EmitBestMove(out, info);

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

void UciEngine::EmitBestMove(std::ostream& out, const SearchInfo& info) {
    const std::lock_guard<std::mutex> lock(output_mutex_);
    if (info.bestMove.has_value()) {
        out << "bestmove " << info.bestMove->ToUci() << '\n';
    } else {
        // No legal moves (should not happen in normal play).
        out << "bestmove 0000\n";
    }
    out.flush();
}

}  // namespace chess
