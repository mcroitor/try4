#ifndef CHESS_CONFIGURATION_HPP_
#define CHESS_CONFIGURATION_HPP_

#include <filesystem>
#include <istream>
#include <optional>
#include <string>

#include "chess/uci.hpp"

namespace chess {

// Transactional parser. Omitted fields inherit defaults; no engine side effects.
std::optional<UciOptions> ParseConfiguration(std::istream& input,
    const std::string& source, std::string* error);
std::optional<UciOptions> LoadConfiguration(const std::filesystem::path& path,
                                         std::string* error);
// --config selects an explicit file; otherwise try4.ini beside the executable
// is optional. --version is handled by the executable before this function.
std::optional<UciOptions> StartupConfiguration(int argc, char** argv, std::string* error);
std::string ConfigurationSnapshot(const UciOptions& options);

}  // namespace chess
#endif  // CHESS_CONFIGURATION_HPP_
