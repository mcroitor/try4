#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "chess/notation.hpp"
#include "chess/position.hpp"
#include "chess/search.hpp"
#include "chess/configuration.hpp"
#include "chess/version.hpp"

#ifndef TRY4_SELFPLAY_COMPILER
#define TRY4_SELFPLAY_COMPILER "unknown"
#endif

#ifndef TRY4_SELFPLAY_BUILD_TYPE
#define TRY4_SELFPLAY_BUILD_TYPE "unknown"
#endif

namespace {

using namespace chess;

constexpr double kAlpha = 0.05;
constexpr double kBeta = 0.05;
constexpr double kSprtElo1 = 20.0;

struct Options {
    std::string config_path;
    UciOptions engine_settings;
    bool tt_mb_set = false;
    std::string openings_path = "docs/selfplay/openings.txt";
    std::string output_dir = "docs/selfplay/results/baseline";
    std::string label = "baseline";
    std::string hardware = "unspecified";
    std::uint64_t nodes = 5000;
    std::optional<std::int64_t> movetime_ms;
    int max_plies = 160;
    std::size_t tt_mb = 16;
    std::uint64_t seed = 1;
    std::size_t limit_openings = 0;
    bool validate_openings = false;
    bool nodes_set = false;
    bool movetime_set = false;
};

struct Opening {
    std::string id;
    std::vector<std::string> moves;
};

struct GameRecord {
    int id = 0;
    std::string opening;
    std::string white;
    std::string black;
    std::string result = "1/2-1/2";
    std::string termination = "unknown";
    int plies = 0;
    std::uint64_t white_nodes = 0;
    std::uint64_t black_nodes = 0;
    std::vector<std::string> uci_moves;
    std::vector<std::string> san_moves;
};

struct Split {
    int wins = 0;
    int losses = 0;
    int draws = 0;
};

struct Summary {
    int games = 0;
    int wins = 0;
    int losses = 0;
    int draws = 0;
    Split as_white;
    Split as_black;
    std::uint64_t full_nodes = 0;
    std::uint64_t material_nodes = 0;
    double score = 0.0;
    double elo = 0.0;
    double margin95 = 0.0;
    double sprt_llr = 0.0;
    double sprt_lower = 0.0;
    double sprt_upper = 0.0;
    std::string sprt_decision = "continue";
    bool elo_clipped = false;
    std::string terminations;
};

std::string Trim(std::string_view text) {
    const std::size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) {
        return {};
    }
    const std::size_t last = text.find_last_not_of(" \t\r\n");
    return std::string(text.substr(first, last - first + 1));
}

std::vector<Opening> LoadOpenings(const std::string& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("cannot open opening file: " + path);
    }

    std::vector<Opening> openings;
    std::string line;
    int line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        const std::string trimmed = Trim(line);
        if (trimmed.empty() || trimmed.front() == '#') {
            continue;
        }
        std::istringstream stream(trimmed);
        Opening opening;
        if (!(stream >> opening.id)) {
            throw std::runtime_error("missing opening id on line " +
                                     std::to_string(line_number));
        }
        std::string move;
        while (stream >> move) {
            opening.moves.push_back(move);
        }
        if (opening.moves.empty()) {
            throw std::runtime_error("opening has no moves: " + opening.id);
        }
        openings.push_back(std::move(opening));
    }
    if (openings.empty()) {
        throw std::runtime_error("opening file contains no openings: " + path);
    }
    return openings;
}

Position OpeningPosition(const Opening& opening,
                         std::vector<std::string>* san_moves = nullptr) {
    Position position;
    position.SetupInitial();
    for (const std::string& text : opening.moves) {
        const std::optional<Move> move = ParseUci(position, text);
        if (!move.has_value()) {
            throw std::runtime_error("illegal opening move " + text +
                                     " in " + opening.id);
        }
        if (san_moves != nullptr) {
            san_moves->push_back(ToSan(position, *move));
        }
        if (!position.MakeMove(*move)) {
            throw std::runtime_error("failed to apply opening move " + text +
                                     " in " + opening.id);
        }
    }
    return position;
}

std::string ResultForWinner(Color winner) {
    return winner == Color::White ? "1-0" : "0-1";
}

std::string TerminationName(GameResult result) {
    switch (result) {
        case GameResult::Checkmate:
            return "checkmate";
        case GameResult::Stalemate:
            return "stalemate";
        case GameResult::DrawFiftyMove:
            return "fifty-move";
        case GameResult::DrawRepetition:
            return "threefold-repetition";
        case GameResult::DrawInsufficientMaterial:
            return "insufficient-material";
        case GameResult::Ongoing:
            return "ongoing";
    }
    return "unknown";
}

GameRecord PlayGame(const Opening& opening, int game_id, bool full_is_white,
                    const Options& options, std::size_t tt_mb) {
    GameRecord game;
    game.id = game_id;
    game.opening = opening.id;
    game.white = full_is_white ? "full" : "material";
    game.black = full_is_white ? "material" : "full";

    Position position = OpeningPosition(opening, &game.san_moves);
    for (const std::string& move : opening.moves) {
        game.uci_moves.push_back(move);
    }
    game.plies = static_cast<int>(game.uci_moves.size());

    Searcher white_searcher(tt_mb);
    Searcher black_searcher(tt_mb);

    SearchLimits limits;
    limits.evaluation = options.engine_settings.evaluation;
    limits.forcing = options.engine_settings.forcing;
    limits.depth = 64;
    if (options.movetime_ms.has_value()) {
        limits.moveTimeMs = options.movetime_ms;
    } else {
        limits.nodeLimit = options.nodes;
    }
    limits.useMoveOrdering = true;
    limits.useQuiescence = true;
    limits.useTranspositionTable = true;

    while (true) {
        const GameResult result = position.Result();
        if (result == GameResult::Checkmate) {
            const Color winner = position.SideToMove() == Color::White
                                     ? Color::Black
                                     : Color::White;
            game.result = ResultForWinner(winner);
            game.termination = "checkmate";
            break;
        }
        if (result != GameResult::Ongoing) {
            game.result = "1/2-1/2";
            game.termination = TerminationName(result);
            break;
        }
        if (game.plies >= options.max_plies) {
            game.result = "1/2-1/2";
            game.termination = "max-plies";
            break;
        }

        const std::vector<Move> legal = position.LegalMoves();
        if (legal.empty()) {
            game.result = "1/2-1/2";
            game.termination = "no-legal-moves";
            break;
        }

        const bool white_to_move = position.SideToMove() == Color::White;
        Searcher& searcher = white_to_move ? white_searcher : black_searcher;
        limits.evaluationMode = (white_to_move == full_is_white)
                                    ? EvaluationMode::Full
                                    : EvaluationMode::Material;

        std::atomic_bool stop{false};
        const SearchInfo info = searcher.Search(position, limits, stop);
        if (!info.bestMove.has_value() || !position.IsLegal(*info.bestMove)) {
            throw std::runtime_error("search returned no legal move in game " +
                                     std::to_string(game_id));
        }

        const Move move = *info.bestMove;
        game.san_moves.push_back(ToSan(position, move));
        game.uci_moves.push_back(move.ToUci());
        if (white_to_move) {
            game.white_nodes += info.nodes;
        } else {
            game.black_nodes += info.nodes;
        }
        if (!position.MakeMove(move)) {
            throw std::runtime_error("failed to apply searched move in game " +
                                     std::to_string(game_id));
        }
        ++game.plies;
    }

    return game;
}

double GameScoreForFull(const GameRecord& game) {
    const bool full_is_white = game.white == "full";
    if (game.result == "1-0") {
        return full_is_white ? 1.0 : 0.0;
    }
    if (game.result == "0-1") {
        return full_is_white ? 0.0 : 1.0;
    }
    return 0.5;
}

void AddSplit(Split* split, const GameRecord& game) {
    const double score = GameScoreForFull(game);
    if (score == 1.0) {
        ++split->wins;
    } else if (score == 0.0) {
        ++split->losses;
    } else {
        ++split->draws;
    }
}

std::string JoinMoves(const std::vector<std::string>& moves) {
    std::ostringstream stream;
    for (std::size_t i = 0; i < moves.size(); ++i) {
        if (i != 0) {
            stream << ' ';
        }
        stream << moves[i];
    }
    return stream.str();
}

std::string EscapeJson(const std::string& text) {
    std::string escaped;
    for (char character : text) {
        switch (character) {
            case '"':
                escaped += "\\\"";
                break;
            case '\\':
                escaped += "\\\\";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case '\r':
                escaped += "\\r";
                break;
            case '\t':
                escaped += "\\t";
                break;
            default:
                escaped += character;
                break;
        }
    }
    return escaped;
}

std::string TerminationCounts(const std::vector<GameRecord>& games) {
    std::vector<std::pair<std::string, int>> counts;
    for (const GameRecord& game : games) {
        auto found = std::find_if(
            counts.begin(), counts.end(),
            [&game](const auto& item) { return item.first == game.termination; });
        if (found == counts.end()) {
            counts.emplace_back(game.termination, 1);
        } else {
            ++found->second;
        }
    }
    std::ostringstream stream;
    for (std::size_t i = 0; i < counts.size(); ++i) {
        if (i != 0) {
            stream << ", ";
        }
        stream << counts[i].first << "=" << counts[i].second;
    }
    return stream.str();
}

Summary Summarize(const std::vector<GameRecord>& games) {
    Summary summary;
    summary.games = static_cast<int>(games.size());
    double total_score = 0.0;
    for (const GameRecord& game : games) {
        const bool full_is_white = game.white == "full";
        const double score = GameScoreForFull(game);
        total_score += score;
        if (score == 1.0) {
            ++summary.wins;
        } else if (score == 0.0) {
            ++summary.losses;
        } else {
            ++summary.draws;
        }
        if (full_is_white) {
            AddSplit(&summary.as_white, game);
        } else {
            AddSplit(&summary.as_black, game);
        }
        if (full_is_white) {
            summary.full_nodes += game.white_nodes;
            summary.material_nodes += game.black_nodes;
        } else {
            summary.full_nodes += game.black_nodes;
            summary.material_nodes += game.white_nodes;
        }
    }

    summary.score = summary.games == 0
                        ? 0.5
                        : total_score / static_cast<double>(summary.games);
    summary.terminations = TerminationCounts(games);

    double variance = 0.0;
    for (const GameRecord& game : games) {
        const double difference = GameScoreForFull(game) - summary.score;
        variance += difference * difference;
    }
    if (summary.games > 1) {
        variance /= static_cast<double>(summary.games - 1);
    } else {
        variance = 0.0;
    }
    const double standard_error =
        summary.games == 0
            ? 0.0
            : std::sqrt(variance / static_cast<double>(summary.games));

    double elo_score = summary.score;
    if (elo_score <= 0.0 || elo_score >= 1.0) {
        summary.elo_clipped = true;
        const double n = static_cast<double>(std::max(1, summary.games));
        elo_score = std::clamp(elo_score, 0.5 / n, 1.0 - 0.5 / n);
    }
    summary.elo = -400.0 * std::log10(1.0 / elo_score - 1.0);
    const double slope =
        400.0 / (std::log(10.0) * elo_score * (1.0 - elo_score));
    summary.margin95 =
        1.96 * slope * standard_error;

    const double p0 = 0.5;
    const double p1 = 1.0 / (1.0 + std::pow(10.0, -kSprtElo1 / 400.0));
    double llr = 0.0;
    for (const GameRecord& game : games) {
        const double x = GameScoreForFull(game);
        llr += ((x - p0) * (x - p0) - (x - p1) * (x - p1)) / (2.0 * 0.25);
    }
    summary.sprt_llr = llr;
    summary.sprt_lower = std::log(kBeta / (1.0 - kAlpha));
    summary.sprt_upper = std::log((1.0 - kBeta) / kAlpha);
    if (llr >= summary.sprt_upper) {
        summary.sprt_decision = "accept_full";
    } else if (llr <= summary.sprt_lower) {
        summary.sprt_decision = "accept_material";
    }
    return summary;
}

void WriteConfig(const std::filesystem::path& path, const Options& options) {
    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error("cannot write " + path.string());
    }
    output << "engine_version=" << kVersion << '\n'
           << "config_path=" << options.config_path << '\n'
           << "label=" << options.label << '\n'
           << "candidate=full\n"
           << "baseline=material\n"
           << "openings_path=" << options.openings_path << '\n'
           << "nodes_per_move="
           << (options.movetime_ms.has_value()
                   ? "time-based"
                   : std::to_string(options.nodes))
           << '\n'
           << "movetime_ms="
           << (options.movetime_ms.has_value()
                   ? std::to_string(*options.movetime_ms)
                   : "none")
           << '\n'
           << "max_plies=" << options.max_plies << '\n'
           << "tt_mb=" << options.tt_mb << '\n'
           << "seed=" << options.seed << '\n'
           << "hardware=" << options.hardware << '\n'
           << "compiler=" << TRY4_SELFPLAY_COMPILER << '\n'
           << "build_type=" << TRY4_SELFPLAY_BUILD_TYPE << '\n';
    std::ofstream snapshot(path.parent_path() / "engine-settings.ini");
    if (!snapshot) { throw std::runtime_error("cannot write engine settings snapshot"); }
    auto settings = options.engine_settings;
    settings.hash_mb = options.tt_mb;
    snapshot << ConfigurationSnapshot(settings);
}

void WriteCsv(const std::filesystem::path& path,
              const std::vector<GameRecord>& games) {
    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error("cannot write " + path.string());
    }
    output << "game,opening,white,black,result,termination,plies,"
              "white_nodes,black_nodes,moves\n";
    for (const GameRecord& game : games) {
        output << game.id << ',' << game.opening << ',' << game.white << ','
               << game.black << ',' << game.result << ',' << game.termination
               << ',' << game.plies << ',' << game.white_nodes << ','
               << game.black_nodes << ',' << JoinMoves(game.uci_moves) << '\n';
    }
}

std::string PgnMoveText(const GameRecord& game) {
    std::ostringstream output;
    for (std::size_t i = 0; i < game.san_moves.size(); ++i) {
        if (i % 2 == 0) {
            if (i != 0) {
                output << ' ';
            }
            output << (i / 2 + 1) << ". ";
        } else {
            output << ' ';
        }
        output << game.san_moves[i];
    }
    output << ' ' << game.result;
    return output.str();
}

void WritePgn(const std::filesystem::path& path,
              const std::vector<GameRecord>& games) {
    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error("cannot write " + path.string());
    }
    for (const GameRecord& game : games) {
        output << "[Event \"try4 self-play\"]\n"
               << "[Site \"local\"]\n"
               << "[Date \"????.??.??\"]\n"
               << "[Round \"" << game.id << "\"]\n"
               << "[White \"" << game.white << "\"]\n"
               << "[Black \"" << game.black << "\"]\n"
               << "[Result \"" << game.result << "\"]\n"
               << "[Opening \"" << game.opening << "\"]\n"
               << "[Termination \"" << game.termination << "\"]\n\n"
               << PgnMoveText(game) << "\n\n";
    }
}

void WriteSummaryText(const std::filesystem::path& path, const Options& options,
                      const Summary& summary) {
    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error("cannot write " + path.string());
    }
    output << "try4 self-play " << options.label << '\n'
           << "candidate: full evaluation\n"
           << "baseline: material only\n"
           << "games: " << summary.games << '\n'
           << "full W/D/L: " << summary.wins << '/' << summary.draws << '/'
           << summary.losses << '\n'
           << "full score: " << std::fixed << std::setprecision(4)
           << summary.score << '\n'
           << "full as White W/D/L: " << summary.as_white.wins << '/'
           << summary.as_white.draws << '/' << summary.as_white.losses << '\n'
           << "full as Black W/D/L: " << summary.as_black.wins << '/'
           << summary.as_black.draws << '/' << summary.as_black.losses << '\n'
           << "elo difference: " << std::showpos << std::fixed
           << std::setprecision(2) << summary.elo << std::noshowpos << '\n'
           << "95% margin: +/- " << std::fixed << std::setprecision(2)
           << summary.margin95 << '\n'
           << "terminations: " << summary.terminations << '\n'
           << "full nodes: " << summary.full_nodes << '\n'
           << "material nodes: " << summary.material_nodes << '\n'
           << "sprt (H1 +" << std::setprecision(0) << kSprtElo1
           << " Elo): llr=" << std::setprecision(3) << summary.sprt_llr
           << " bounds=[" << summary.sprt_lower << ", "
           << summary.sprt_upper << "] decision=" << summary.sprt_decision
           << '\n';
    if (summary.elo_clipped) {
        output << "note: score was 0% or 100%; Elo estimate uses a small-sample "
                  "clamp\n";
    }
}

void WriteSummaryJson(const std::filesystem::path& path, const Options& options,
                      const Summary& summary) {
    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error("cannot write " + path.string());
    }
    output << std::fixed << std::setprecision(4)
           << "{\n"
           << "  \"label\": \"" << EscapeJson(options.label) << "\",\n"
           << "  \"candidate\": \"full\",\n"
           << "  \"baseline\": \"material\",\n"
           << "  \"games\": " << summary.games << ",\n"
           << "  \"wins\": " << summary.wins << ",\n"
           << "  \"losses\": " << summary.losses << ",\n"
           << "  \"draws\": " << summary.draws << ",\n"
           << "  \"score\": " << summary.score << ",\n"
           << "  \"elo\": " << summary.elo << ",\n"
           << "  \"elo_margin_95\": " << summary.margin95 << ",\n"
           << "  \"elo_clipped\": " << (summary.elo_clipped ? "true" : "false")
           << ",\n"
           << "  \"as_white\": {\"wins\": " << summary.as_white.wins
           << ", \"losses\": " << summary.as_white.losses
           << ", \"draws\": " << summary.as_white.draws << "},\n"
           << "  \"as_black\": {\"wins\": " << summary.as_black.wins
           << ", \"losses\": " << summary.as_black.losses
           << ", \"draws\": " << summary.as_black.draws << "},\n"
           << "  \"terminations\": \"" << EscapeJson(summary.terminations)
           << "\",\n"
           << "  \"sprt\": {\"elo1\": " << std::setprecision(1)
           << kSprtElo1 << ", \"llr\": " << std::setprecision(4)
           << summary.sprt_llr << ", \"lower\": " << summary.sprt_lower
           << ", \"upper\": " << summary.sprt_upper << ", \"decision\": \""
           << summary.sprt_decision << "\"}\n"
           << "}\n";
}

void PrintHelp() {
    std::cout
        << "usage: try4_selfplay [options]\n"
        << "  --config PATH         explicit INI evaluation/forcing/hash settings\n"
        << "  --openings PATH       opening file (default docs/selfplay/openings.txt)\n"
        << "  --output DIR          result directory (default docs/selfplay/results/baseline)\n"
        << "  --label NAME          label written to the summary\n"
        << "  --hardware TEXT       hardware description for config.txt\n"
        << "  --nodes N             fixed nodes per move (default 5000)\n"
        << "  --movetime MS         fixed movetime per move; overrides --nodes\n"
        << "  --max-plies N         adjudicate as a draw after N plies (default 160)\n"
        << "  --tt-mb N             transposition table size in MB (default 16)\n"
        << "  --seed N              recorded seed (default 1)\n"
        << "  --limit-openings N    play only the first N openings\n"
        << "  --validate-openings   validate the opening file and exit\n"
        << "  --help                show this help\n";
}

Options ParseOptions(int argc, char** argv) {
    Options options;
    auto require_value = [&](int index) -> std::string {
        if (index + 1 >= argc) {
            throw std::runtime_error("missing value for " + std::string(argv[index]));
        }
        return argv[index + 1];
    };

    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--help") {
            PrintHelp();
            std::exit(0);
        }
        if (argument == "--validate-openings") {
            options.validate_openings = true;
            continue;
        }
        if (argument == "--config") {
            options.config_path = require_value(i);
            ++i;
        } else if (argument == "--openings") {
            options.openings_path = require_value(i);
            ++i;
        } else if (argument == "--output") {
            options.output_dir = require_value(i);
            ++i;
        } else if (argument == "--label") {
            options.label = require_value(i);
            ++i;
        } else if (argument == "--hardware") {
            options.hardware = require_value(i);
            ++i;
        } else if (argument == "--nodes") {
            options.nodes = std::stoull(require_value(i));
            options.nodes_set = true;
            ++i;
        } else if (argument == "--movetime") {
            options.movetime_ms = std::stoll(require_value(i));
            options.movetime_set = true;
            ++i;
        } else if (argument == "--max-plies") {
            options.max_plies = std::stoi(require_value(i));
            ++i;
        } else if (argument == "--tt-mb") {
            options.tt_mb = std::stoull(require_value(i));
            options.tt_mb_set = true;
            ++i;
        } else if (argument == "--seed") {
            options.seed = std::stoull(require_value(i));
            ++i;
        } else if (argument == "--limit-openings") {
            options.limit_openings = std::stoull(require_value(i));
            ++i;
        } else {
            throw std::runtime_error("unknown option: " + argument);
        }
    }

    if (!options.config_path.empty()) {
        std::string error;
        const auto settings = LoadConfiguration(std::filesystem::u8path(options.config_path), &error);
        if (!settings) { throw std::runtime_error(error); }
        options.engine_settings = *settings;
        if (!options.tt_mb_set) { options.tt_mb = settings->hash_mb; }
    }
    if (options.nodes_set && options.movetime_set) {
        throw std::runtime_error("--nodes and --movetime are mutually exclusive");
    }
    if (options.max_plies <= 0) {
        throw std::runtime_error("--max-plies must be positive");
    }
    if (options.tt_mb == 0) {
        throw std::runtime_error("--tt-mb must be positive");
    }
    if (options.movetime_ms.has_value() && *options.movetime_ms <= 0) {
        throw std::runtime_error("--movetime must be positive");
    }
    return options;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const Options options = ParseOptions(argc, argv);
        std::vector<Opening> openings = LoadOpenings(options.openings_path);

        for (const Opening& opening : openings) {
            (void)OpeningPosition(opening);
        }
        if (options.validate_openings) {
            std::cout << "validated " << openings.size() << " openings\n";
            return 0;
        }

        if (options.limit_openings != 0) {
            openings.resize(std::min(openings.size(), options.limit_openings));
        }

        std::vector<GameRecord> games;
        games.reserve(openings.size() * 2);
        int game_id = 0;
        for (const Opening& opening : openings) {
            ++game_id;
            games.push_back(PlayGame(opening, game_id, true, options, options.tt_mb));
            ++game_id;
            games.push_back(PlayGame(opening, game_id, false, options, options.tt_mb));
        }

        const Summary summary = Summarize(games);
        std::filesystem::create_directories(options.output_dir);
        const std::filesystem::path output(options.output_dir);
        WriteConfig(output / "config.txt", options);
        WriteCsv(output / "games.csv", games);
        WritePgn(output / "games.pgn", games);
        WriteSummaryText(output / "summary.txt", options, summary);
        WriteSummaryJson(output / "summary.json", options, summary);

        std::cout << "games: " << summary.games
                  << ", full W/D/L: " << summary.wins << '/'
                  << summary.draws << '/' << summary.losses
                  << ", score: " << std::fixed << std::setprecision(4)
                  << summary.score << ", elo: " << std::showpos
                  << std::setprecision(2) << summary.elo << std::noshowpos
                  << " +/- " << std::setprecision(2) << summary.margin95
                  << '\n';
        std::cout << "results written to " << options.output_dir << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
