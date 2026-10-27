#include <chrono>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <iostream>
#include <string>

#include "chess/perft.hpp"
#include "chess/position.hpp"

namespace {

void PrintUsage(const char* program) {
    std::cerr << "usage: " << program << " <fen> <depth>\n";
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        PrintUsage(argv[0]);
        return 2;
    }

    std::string error;
    const auto parsed = chess::Position::FromFen(argv[1], &error);
    if (!parsed.has_value()) {
        std::cerr << "invalid FEN: " << error << '\n';
        return 2;
    }

    int depth = 0;
    try {
        std::size_t consumed = 0;
        const std::string depth_text = argv[2];
        depth = std::stoi(depth_text, &consumed);
        if (consumed != depth_text.size() || depth < 0) {
            throw std::invalid_argument("depth");
        }
    } catch (const std::exception&) {
        std::cerr << "invalid depth: " << argv[2] << '\n';
        return 2;
    }

    chess::Position position = *parsed;
    const std::string before = position.ToFen();

    const auto start = std::chrono::steady_clock::now();
    const std::uint64_t nodes = chess::Perft(position, depth);
    const auto finish = std::chrono::steady_clock::now();
    const auto elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(finish - start);
    const double seconds = static_cast<double>(elapsed.count()) / 1000.0;

    std::cout << "position=" << before << '\n';
    std::cout << "depth=" << depth << '\n';
    std::cout << "nodes=" << nodes << '\n';
    std::cout << "time_ms=" << elapsed.count() << '\n';
    if (seconds > 0.0) {
        std::cout << "nps=" << std::fixed << std::setprecision(0)
                  << static_cast<double>(nodes) / seconds << '\n';
    }
    return 0;
}
