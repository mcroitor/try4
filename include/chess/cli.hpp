#ifndef CHESS_CLI_HPP_
#define CHESS_CLI_HPP_

#include <iosfwd>
#include <string>

#include "chess/position.hpp"

namespace chess {

// Small line-oriented debug shell. It is deliberately I/O-injectable so the
// same command handling can be exercised by tests.
class Cli {
public:
    Cli(std::istream& input, std::ostream& output);

    // Processes commands until EOF or `quit`. Returns 0 on normal completion.
    int Run();

private:
    bool ProcessLine(const std::string& line);
    void PrintMoves() const;
    void PrintResult() const;

    std::istream& input_;
    std::ostream& output_;
    Position position_;
};

}  // namespace chess

#endif  // CHESS_CLI_HPP_
