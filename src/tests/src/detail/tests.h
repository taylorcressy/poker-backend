#pragma once

// Pre-include all stdlib headers that poker_core.h pulls in, so they are
// compiled with the real 'private' keyword before we redefine it below.
#include <algorithm>
#include <cassert>
#include <iostream>
#include <optional>
#include <ranges>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Redefine 'private' as 'public' so this TU can access PokerGame internals.
// Access specifiers don't affect memory layout, so this is ABI-safe.
#define private public
#include "poker_core.h"
#undef private

// auth.h must be included OUTSIDE any namespace block.
// jwt-cpp transitively opens 'namespace std { ... }' internally; if this
// include were inside 'namespace pokergame::tests', those std symbols would
// be nested as pokergame::tests::std::... and break all of stdlib.
#include "auth.h"

namespace pokergame::tests {

class PokerTests {
public:
    PokerTests();
    void run();

private:
    // Game instance used across all tests: 5 seats, ante=5, min_chips=0.
    core::PokerGame g;

    // ---- test infrastructure ----
    void resetSeats(std::initializer_list<std::pair<core::SeatState, core::bet_t>> defs);
    void resetActions();
    void pushAction(core::BetType bt, std::optional<core::bet_t> amt);
    void setActions(std::initializer_list<std::pair<core::BetType, std::optional<core::bet_t>>> actions);

    // Runs g.handleBettingRound() and returns per-player chip contributions
    // as deltas (handleBettingRound returns void; we infer bets from chip changes).
    std::unordered_map<size_t, core::bet_t> runBettingRound();

    // ---- test suites ----
    void testTakeBet();
    void testSetNextStreetOrFinishRound();
    void testHandleBettingRound();
    void testProcessBetsOnRoundConclusion();
};

class AuthTests {
public:
    void run();

private:
    void testJwt();
};

} // namespace pokergame::tests
