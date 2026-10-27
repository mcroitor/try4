#include <iostream>
#include <string_view>

#include "chess/cli.hpp"
#include "chess/version.hpp"

int main(int argc, char** argv) {
    if (argc == 2 && std::string_view(argv[1]) == "--version") {
        std::cout << "try4 " << chess::kVersion << '\n';
        return 0;
    }
    chess::Cli cli(std::cin, std::cout);
    return cli.Run();
}
