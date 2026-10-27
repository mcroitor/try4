#include <iostream>
#include <string_view>

#include "chess/uci.hpp"
#include "chess/configuration.hpp"
#include "chess/version.hpp"

int main(int argc, char** argv) {
    if (argc == 2 && std::string_view(argv[1]) == "--version") {
        std::cout << "try4 " << chess::kVersion << '\n';
        return 0;
    }
    try {
        std::string error;
        const auto options = chess::StartupConfiguration(argc, argv, &error);
        if (!options) { std::cerr << error << '\n'; return 1; }
        chess::UciEngine engine(*options);
        return engine.Run(std::cin, std::cout);
    } catch (const std::exception& error) {
        std::cerr << "startup failure: " << error.what() << '\n';
        return 1;
    }
}
