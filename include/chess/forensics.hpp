#ifndef CHESS_FORENSICS_HPP_
#define CHESS_FORENSICS_HPP_

#include <cstdlib>

namespace chess {
namespace detail {

// True when TRY4_TIME_DEBUG is in the environment; gates the match-forensics
// logging of time budgets and UCI command flow to stderr. Without the
// variable nothing is written anywhere: the process tests assert a clean
// stderr stream, so this must stay strictly opt-in.
//
// MSVC (C4996, /WX in the CI) deprecates getenv; MinGW does not. The pragma
// is confined here and guarded so both toolchains build warning-clean.
inline bool ForensicsEnabled() {
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4996)
    const bool enabled = std::getenv("TRY4_TIME_DEBUG") != nullptr;
#pragma warning(pop)
    return enabled;
#else
    return std::getenv("TRY4_TIME_DEBUG") != nullptr;
#endif
}

}  // namespace detail
}  // namespace chess

#endif  // CHESS_FORENSICS_HPP_