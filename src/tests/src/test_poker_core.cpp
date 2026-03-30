#include "detail/tests.h"

namespace pokergame::tests {

using namespace core;
using namespace core::types;

// ---------------------------------------------------------------------------
// Infrastructure
// ---------------------------------------------------------------------------
class TestNotifier: public events::INotifier {
public:
    TestNotifier() = default;

    void sendMessageToPlayer(const std::string &room_id, const std::string &player_id, events::Notification *) override {

    }

    void sendMessageToTable(const std::string &room_id, events::Notification *) override {

    }
};


PokerTests::PokerTests() : g(PokerConfiguration{5, 5, 0}, std::make_shared<TestNotifier>(TestNotifier{}), "1") {}

void PokerTests::resetSeats(std::initializer_list<std::pair<SeatState, bet_t>> defs) {
    for (auto &seat : g.seats) {
        seat.seat_state = SeatState::Empty;
        seat.chips = 0;
        seat.hand.clear();
    }
    g.pots.clear();
    g.max_pot_sizes.clear();
    size_t idx = 0;
    for (auto [st, chips] : defs) {
        g.seats[idx].seat_state = st;
        g.seats[idx].chips = chips;
        idx++;
    }
}

void PokerTests::resetActions() {
    g.getPlayerAction(static_cast<size_t>(-1), {}, 0);
}

void PokerTests::pushAction(BetType bt, std::optional<bet_t> amt) {
    bet_t enc = amt.has_value() ? *amt : static_cast<bet_t>(-1);
    g.getPlayerAction(static_cast<size_t>(-2), {bt}, enc);
}

void PokerTests::setActions(std::initializer_list<std::pair<BetType, std::optional<bet_t>>> actions) {
    resetActions();
    for (auto [bt, amt] : actions) pushAction(bt, amt);
}

std::unordered_map<size_t, bet_t> PokerTests::runBettingRound() {
    std::vector<bet_t> initial(g.seats.size());
    for (size_t i = 0; i < g.seats.size(); i++) initial[i] = g.seats[i].chips;
    g.handleBettingRound();
    std::unordered_map<size_t, bet_t> bets;
    for (size_t i = 0; i < g.seats.size(); i++) {
        if (initial[i] > g.seats[i].chips)
            bets[i] = initial[i] - g.seats[i].chips;
    }
    return bets;
}

void PokerTests::run() {
    testTakeBet();
    testSetNextStreetOrFinishRound();
    testHandleBettingRound();
    testProcessBetsOnRoundConclusion();
    resetActions(); // leave sentinel queue clean
}

// ---------------------------------------------------------------------------
// takeBet tests   (config.ante = 5 → SB = 5, BB = 10)
// ---------------------------------------------------------------------------

void PokerTests::testTakeBet() {
    std::cout << "=== takeBet tests ===\n";

    // TB1: Check – nullopt returned, chips unchanged
    {
        resetSeats({{SeatState::InHand, 100}});
        auto r = g.takeBet(0, BetType::Check, std::nullopt);
        assert(!r.has_value());
        assert(g.seats[0].chips == 100);
        assert(g.seats[0].seat_state == SeatState::InHand);
    }

    // TB2: SmallBlind – deducts ante (5)
    {
        resetSeats({{SeatState::InHand, 100}});
        auto r = g.takeBet(0, BetType::SmallBlind, std::nullopt);
        assert(r.has_value() && *r == 5);
        assert(g.seats[0].chips == 95);
        assert(g.seats[0].seat_state == SeatState::InHand);
    }

    // TB3: BigBlind – deducts ante*2 (10)
    {
        resetSeats({{SeatState::InHand, 100}});
        auto r = g.takeBet(0, BetType::BigBlind, std::nullopt);
        assert(r.has_value() && *r == 10);
        assert(g.seats[0].chips == 90);
        assert(g.seats[0].seat_state == SeatState::InHand);
    }

    // TB4: Fold – nullopt returned, seat becomes Folded, chips untouched
    {
        resetSeats({{SeatState::InHand, 100}});
        auto r = g.takeBet(0, BetType::Fold, std::nullopt);
        assert(!r.has_value());
        assert(g.seats[0].chips == 100);
        assert(g.seats[0].seat_state == SeatState::Folded);
    }

    // TB5: Call – deducts exact amount requested
    {
        resetSeats({{SeatState::InHand, 100}});
        auto r = g.takeBet(0, BetType::Call, bet_t{30});
        assert(r.has_value() && *r == 30);
        assert(g.seats[0].chips == 70);
        assert(g.seats[0].seat_state == SeatState::InHand);
    }

    // TB6: Bet – deducts requested bet amount
    {
        resetSeats({{SeatState::InHand, 100}});
        auto r = g.takeBet(0, BetType::Bet, bet_t{25});
        assert(r.has_value() && *r == 25);
        assert(g.seats[0].chips == 75);
        assert(g.seats[0].seat_state == SeatState::InHand);
    }

    // TB7: Raise – deducts requested raise amount
    {
        resetSeats({{SeatState::InHand, 100}});
        auto r = g.takeBet(0, BetType::Raise, bet_t{40});
        assert(r.has_value() && *r == 40);
        assert(g.seats[0].chips == 60);
        assert(g.seats[0].seat_state == SeatState::InHand);
    }

    // TB8: SmallBlind all-in – chips(3) < ante(5): takes all chips, state → AllIn
    {
        resetSeats({{SeatState::InHand, 3}});
        auto r = g.takeBet(0, BetType::SmallBlind, std::nullopt);
        assert(r.has_value() && *r == 3);
        assert(g.seats[0].chips == 0);
        assert(g.seats[0].seat_state == SeatState::AllIn);
    }

    // TB9: Call all-in – chips(15) < amount(20): takes all chips, state → AllIn
    {
        resetSeats({{SeatState::InHand, 15}});
        auto r = g.takeBet(0, BetType::Call, bet_t{20});
        assert(r.has_value() && *r == 15);
        assert(g.seats[0].chips == 0);
        assert(g.seats[0].seat_state == SeatState::AllIn);
    }

    // TB10: BigBlind with chips == ante*2 – takes full amount, chips reach 0,
    //       state stays InHand (condition is chips < amount, strictly less-than)
    {
        resetSeats({{SeatState::InHand, 10}});
        auto r = g.takeBet(0, BetType::BigBlind, std::nullopt);
        assert(r.has_value() && *r == 10);
        assert(g.seats[0].chips == 0);
        assert(g.seats[0].seat_state == SeatState::InHand);
    }

    // TB11: All-in BigBlind – chips(7) < BB(10): takes all chips, state → AllIn
    {
        resetSeats({{SeatState::InHand, 7}});
        auto r = g.takeBet(0, BetType::BigBlind, std::nullopt);
        assert(r.has_value() && *r == 7);
        assert(g.seats[0].chips == 0);
        assert(g.seats[0].seat_state == SeatState::AllIn);
    }

    // TB12: All-in on Bet – chips(20) < amount(50): takes all chips, state → AllIn
    {
        resetSeats({{SeatState::InHand, 20}});
        auto r = g.takeBet(0, BetType::Bet, bet_t{50});
        assert(r.has_value() && *r == 20);
        assert(g.seats[0].chips == 0);
        assert(g.seats[0].seat_state == SeatState::AllIn);
    }

    // TB13: All-in on Raise – chips(30) < amount(40): takes all chips, state → AllIn
    {
        resetSeats({{SeatState::InHand, 30}});
        auto r = g.takeBet(0, BetType::Raise, bet_t{40});
        assert(r.has_value() && *r == 30);
        assert(g.seats[0].chips == 0);
        assert(g.seats[0].seat_state == SeatState::AllIn);
    }

    // TB14: Raise with exact chips – chips(40) == amount(40): full amount, state → InHand
    {
        resetSeats({{SeatState::InHand, 40}});
        auto r = g.takeBet(0, BetType::Raise, bet_t{40});
        assert(r.has_value() && *r == 40);
        assert(g.seats[0].chips == 0);
        assert(g.seats[0].seat_state == SeatState::InHand);
    }

    std::cout << "  takeBet: all 14 tests passed\n";
}

// ---------------------------------------------------------------------------
// setNextStreetOrFinishRound tests
// ---------------------------------------------------------------------------

void PokerTests::testSetNextStreetOrFinishRound() {
    std::cout << "=== setNextStreetOrFinishRound tests ===\n";

    // SNSFR1: 2+ InHand + next_street → advances to next street
    {
        resetSeats({{SeatState::InHand, 100}, {SeatState::InHand, 100}, {SeatState::InHand, 100}});
        g.state = GameState::Flop;
        g.setNextStreetOrFinishRound(GameState::Turn);
        assert(g.state == GameState::Turn);
    }

    // SNSFR2: 1 InHand + 1 AllIn + next_street → Showdown
    {
        resetSeats({{SeatState::InHand, 100}, {SeatState::AllIn, 0}, {SeatState::Folded, 0}});
        g.state = GameState::Flop;
        g.setNextStreetOrFinishRound(GameState::Turn);
        assert(g.state == GameState::Showdown);
    }

    // SNSFR3: 0 InHand + 2 AllIn → Showdown
    {
        resetSeats({{SeatState::AllIn, 0}, {SeatState::AllIn, 0}});
        g.state = GameState::Flop;
        g.setNextStreetOrFinishRound(GameState::Turn);
        assert(g.state == GameState::Showdown);
    }

    // SNSFR4: 1 InHand only (rest folded) + next_street → RoundComplete
    {
        resetSeats({{SeatState::InHand, 100}, {SeatState::Folded, 0}, {SeatState::Folded, 0}});
        g.state = GameState::Flop;
        g.setNextStreetOrFinishRound(GameState::Turn);
        assert(g.state == GameState::RoundComplete);
    }

    // SNSFR5: 2+ InHand, no next_street (river end) → Showdown
    {
        resetSeats({{SeatState::InHand, 100}, {SeatState::InHand, 100}});
        g.state = GameState::River;
        g.setNextStreetOrFinishRound(std::nullopt);
        assert(g.state == GameState::Showdown);
    }

    // SNSFR6: 1 InHand + 1 AllIn, no next_street → Showdown
    {
        resetSeats({{SeatState::InHand, 100}, {SeatState::AllIn, 0}});
        g.state = GameState::River;
        g.setNextStreetOrFinishRound(std::nullopt);
        assert(g.state == GameState::Showdown);
    }

    // SNSFR7: 1 InHand, 0 AllIn, no next_street → RoundComplete
    {
        resetSeats({{SeatState::InHand, 100}, {SeatState::Folded, 0}});
        g.state = GameState::River;
        g.setNextStreetOrFinishRound(std::nullopt);
        assert(g.state == GameState::RoundComplete);
    }

    // SNSFR8: 2 InHand + 1 AllIn + next_street → advances (in_hand_count > 1)
    {
        resetSeats({{SeatState::InHand, 100}, {SeatState::InHand, 100}, {SeatState::AllIn, 0}});
        g.state = GameState::Flop;
        g.setNextStreetOrFinishRound(GameState::Turn);
        assert(g.state == GameState::Turn);
    }

    // SNSFR9: 1 InHand + 2 AllIn + next_street → Showdown
    {
        resetSeats({{SeatState::InHand, 100}, {SeatState::AllIn, 0}, {SeatState::AllIn, 0}});
        g.state = GameState::Flop;
        g.setNextStreetOrFinishRound(GameState::Turn);
        assert(g.state == GameState::Showdown);
    }

    // SNSFR10: 1 InHand + 0 AllIn + next_street → RoundComplete
    {
        resetSeats({{SeatState::InHand, 100}, {SeatState::Folded, 0}});
        g.state = GameState::Flop;
        g.setNextStreetOrFinishRound(GameState::Turn);
        assert(g.state == GameState::RoundComplete);
    }

    std::cout << "  setNextStreetOrFinishRound: all 10 tests passed\n";
}

// ---------------------------------------------------------------------------
// handleBettingRound tests
// config.ante = 5 → SB = 5, BB = 10
// Dealer = seat 0 throughout.
// ---------------------------------------------------------------------------

void PokerTests::testHandleBettingRound() {
    std::cout << "=== handleBettingRound tests ===\n";

    // HB1: Pre-flop, 3 players, all call BB.
    {
        resetSeats({{SeatState::InHand, 200}, {SeatState::InHand, 200}, {SeatState::InHand, 200}});
        g.dealer = 0;
        g.state = GameState::PreFlop;
        setActions({{BetType::Call, 10}, {BetType::Call, 5}});
        auto bets = runBettingRound();
        assert(bets[0] == 10);
        assert(bets[1] == 10);
        assert(bets[2] == 10);
        assert(g.seats[0].chips == 190);
        assert(g.seats[1].chips == 190);
        assert(g.seats[2].chips == 190);
        assert(g.seats[0].seat_state == SeatState::InHand);
        assert(g.seats[1].seat_state == SeatState::InHand);
        assert(g.seats[2].seat_state == SeatState::InHand);
    }

    // HB2: Pre-flop, UTG(0) folds, SB(1) calls, BB(2) wins uncontested.
    {
        resetSeats({{SeatState::InHand, 200}, {SeatState::InHand, 200}, {SeatState::InHand, 200}});
        g.dealer = 0;
        g.state = GameState::PreFlop;
        setActions({{BetType::Fold, std::nullopt}, {BetType::Call, 5}});
        auto bets = runBettingRound();
        assert(bets.find(0) == bets.end() || bets[0] == 0);
        assert(bets[1] == 10);
        assert(bets[2] == 10);
        assert(g.seats[0].seat_state == SeatState::Folded);
        assert(g.seats[0].chips == 200);
        assert(g.seats[1].chips == 190);
        assert(g.seats[1].seat_state == SeatState::InHand);
        assert(g.seats[2].chips == 190);
        assert(g.seats[2].seat_state == SeatState::InHand);
    }

    // HB3: Pre-flop, UTG(0) raises to 20; SB(1) calls 15; BB(2) calls 10.
    {
        resetSeats({{SeatState::InHand, 200}, {SeatState::InHand, 200}, {SeatState::InHand, 200}});
        g.dealer = 0;
        g.state = GameState::PreFlop;
        setActions({{BetType::Raise, 20}, {BetType::Call, 15}, {BetType::Call, 10}});
        auto bets = runBettingRound();
        assert(bets[0] == 20);
        assert(bets[1] == 20);
        assert(bets[2] == 20);
        assert(g.seats[0].chips == 180);
        assert(g.seats[1].chips == 180);
        assert(g.seats[2].chips == 180);
    }

    // HB4: Flop, 3 players, all check. No chips moved.
    {
        resetSeats({{SeatState::InHand, 200}, {SeatState::InHand, 200}, {SeatState::InHand, 200}});
        g.dealer = 0;
        g.state = GameState::Flop;
        setActions({{BetType::Check, std::nullopt}, {BetType::Check, std::nullopt}, {BetType::Check, std::nullopt}});
        auto bets = runBettingRound();
        assert(bets.empty() || (bets[0] == 0 && bets[1] == 0 && bets[2] == 0));
        assert(g.seats[0].chips == 200);
        assert(g.seats[1].chips == 200);
        assert(g.seats[2].chips == 200);
    }

    // HB5: Flop, P1 bets 10; P2 calls 10; P0 calls 10.
    {
        resetSeats({{SeatState::InHand, 200}, {SeatState::InHand, 200}, {SeatState::InHand, 200}});
        g.dealer = 0;
        g.state = GameState::Flop;
        setActions({{BetType::Bet, 10}, {BetType::Call, 10}, {BetType::Call, 10}});
        auto bets = runBettingRound();
        assert(bets[0] == 10);
        assert(bets[1] == 10);
        assert(bets[2] == 10);
        assert(g.seats[0].chips == 190);
        assert(g.seats[1].chips == 190);
        assert(g.seats[2].chips == 190);
    }

    // HB6: Flop, P1 bets 10; P2 raises to 25; P0 calls 25; P1 calls 15 more.
    {
        resetSeats({{SeatState::InHand, 200}, {SeatState::InHand, 200}, {SeatState::InHand, 200}});
        g.dealer = 0;
        g.state = GameState::Flop;
        setActions({{BetType::Bet, 10}, {BetType::Raise, 25}, {BetType::Call, 25}, {BetType::Call, 15}});
        auto bets = runBettingRound();
        assert(bets[0] == 25);
        assert(bets[1] == 25);
        assert(bets[2] == 25);
        assert(g.seats[0].chips == 175);
        assert(g.seats[1].chips == 175);
        assert(g.seats[2].chips == 175);
    }

    // HB7: Pre-flop, 2 players. Seat 1 short stack (8 chips).
    //   SB(1) posts 5, BB(0) posts 10. P1 tries to call 5 more but only has 3 → all-in.
    {
        resetSeats({{SeatState::InHand, 200}, {SeatState::InHand, 8}});
        g.dealer = 0;
        g.state = GameState::PreFlop;
        setActions({{BetType::Call, 5}});
        auto bets = runBettingRound();
        assert(bets[0] == 10);
        assert(bets[1] == 8);
        assert(g.seats[0].chips == 190);
        assert(g.seats[0].seat_state == SeatState::InHand);
        assert(g.seats[1].chips == 0);
        assert(g.seats[1].seat_state == SeatState::AllIn);
    }

    // HB8: Pre-flop, 3 players. UTG(0) and SB(1) both fold; BB(2) wins uncontested.
    {
        resetSeats({{SeatState::InHand, 200}, {SeatState::InHand, 200}, {SeatState::InHand, 200}});
        g.dealer = 0;
        g.state = GameState::PreFlop;
        setActions({{BetType::Fold, std::nullopt}, {BetType::Fold, std::nullopt}});
        auto bets = runBettingRound();
        assert(bets.find(0) == bets.end() || bets[0] == 0);
        assert(bets[1] == 5);
        assert(bets[2] == 10);
        assert(g.seats[0].seat_state == SeatState::Folded);
        assert(g.seats[0].chips == 200);
        assert(g.seats[1].seat_state == SeatState::Folded);
        assert(g.seats[1].chips == 195);
        assert(g.seats[2].seat_state == SeatState::InHand);
        assert(g.seats[2].chips == 190);
    }

    // HB9: Flop, 2 players. P1 bets 10; P0 folds.
    {
        resetSeats({{SeatState::InHand, 200}, {SeatState::InHand, 200}});
        g.dealer = 0;
        g.state = GameState::Flop;
        setActions({{BetType::Bet, 10}, {BetType::Fold, std::nullopt}});
        auto bets = runBettingRound();
        assert(bets[1] == 10);
        assert(bets.find(0) == bets.end() || bets[0] == 0);
        assert(g.seats[1].chips == 190);
        assert(g.seats[1].seat_state == SeatState::InHand);
        assert(g.seats[0].seat_state == SeatState::Folded);
        assert(g.seats[0].chips == 200);
    }

    // HB10: Flop, 3 players. P1 checks; P2 bets 15; P0 and P1 both call.
    {
        resetSeats({{SeatState::InHand, 200}, {SeatState::InHand, 200}, {SeatState::InHand, 200}});
        g.dealer = 0;
        g.state = GameState::Flop;
        setActions({{BetType::Check, std::nullopt}, {BetType::Bet, 15}, {BetType::Call, 15}, {BetType::Call, 15}});
        auto bets = runBettingRound();
        assert(bets[0] == 15);
        assert(bets[1] == 15);
        assert(bets[2] == 15);
        assert(g.seats[0].chips == 185);
        assert(g.seats[1].chips == 185);
        assert(g.seats[2].chips == 185);
    }

    // HB11: Re-raise scenario. P0 raises to 30, P1 re-raises to 70, P2 calls, P0 calls.
    {
        resetSeats({{SeatState::InHand, 1000}, {SeatState::InHand, 1000}, {SeatState::InHand, 1000}});
        g.dealer = 0;
        g.state = GameState::PreFlop;
        setActions({
            {BetType::Raise, 30},
            {BetType::Raise, 65},
            {BetType::Call, 60},
            {BetType::Call, 40}
        });
        auto bets = runBettingRound();
        assert(bets[0] == 70);
        assert(bets[1] == 70);
        assert(bets[2] == 70);
        assert(g.seats[0].chips == 930);
        assert(g.seats[1].chips == 930);
        assert(g.seats[2].chips == 930);
    }

    // HB12: Check-raise scenario. Flop, 2 players.
    // P1 checks, P2 bets 20, P1 raises to 60 total, P2 calls.
    {
        resetSeats({{SeatState::Empty, 0}, {SeatState::InHand, 500}, {SeatState::InHand, 500}});
        g.dealer = 0;
        g.state = GameState::Flop;
        setActions({
            {BetType::Check, std::nullopt},
            {BetType::Bet, 20},
            {BetType::Raise, 60},
            {BetType::Call, 40}
        });
        auto bets = runBettingRound();
        assert(bets[1] == 60);
        assert(bets[2] == 60);
        assert(g.seats[1].chips == 440);
        assert(g.seats[2].chips == 440);
    }

    // HB13: Short stack all-in on BB post. P1(SB)=100, P2(BB)=5. P2 goes AllIn. P1 checks.
    {
        resetSeats({{SeatState::Empty, 0}, {SeatState::InHand, 100}, {SeatState::InHand, 5}});
        g.dealer = 0;
        g.state = GameState::PreFlop;
        setActions({{BetType::Check, std::nullopt}});
        auto bets = runBettingRound();
        assert(bets[1] == 5);
        assert(bets[2] == 5);
        assert(g.seats[1].chips == 95);
        assert(g.seats[1].seat_state == SeatState::InHand);
        assert(g.seats[2].chips == 0);
        assert(g.seats[2].seat_state == SeatState::AllIn);
    }

    // HB14: Everyone folds to a bet.
    {
        resetSeats({{SeatState::InHand, 100}, {SeatState::InHand, 100}, {SeatState::InHand, 100}});
        g.dealer = 0;
        g.state = GameState::Flop;
        setActions({
            {BetType::Bet, 10},
            {BetType::Fold, std::nullopt},
            {BetType::Fold, std::nullopt}
        });
        auto bets = runBettingRound();
        assert(bets[1] == 10);
        assert(bets.find(0) == bets.end() || bets[0] == 0);
        assert(bets.find(2) == bets.end() || bets[2] == 0);
        assert(g.seats[1].chips == 90);
        assert(g.seats[2].seat_state == SeatState::Folded);
        assert(g.seats[0].seat_state == SeatState::Folded);
    }

    // HB15: Skip All-in player during betting action.
    {
        resetSeats({{SeatState::InHand, 100}, {SeatState::AllIn, 0}, {SeatState::InHand, 100}});
        g.dealer = 0;
        g.state = GameState::Flop;
        setActions({
            {BetType::Check, std::nullopt},
            {BetType::Check, std::nullopt}
        });
        auto bets = runBettingRound();
        assert(bets.empty() || (bets[0] == 0 && bets[2] == 0));
        assert(g.seats[0].chips == 100);
        assert(g.seats[2].chips == 100);
    }

    // HB16: Raise after short-stack BB all-in.
    // P0(D), P1(SB)=100, P2(BB)=5 (All-In). P0 raises to 20, P1 calls 15 more.
    {
        resetSeats({{SeatState::InHand, 500}, {SeatState::InHand, 100}, {SeatState::InHand, 5}});
        g.dealer = 0;
        g.state = GameState::PreFlop;
        setActions({
            {BetType::Raise, 20},
            {BetType::Call, 15}
        });
        auto bets = runBettingRound();
        assert(bets[0] == 20);
        assert(bets[1] == 20);
        assert(bets[2] == 5);
        assert(g.seats[0].chips == 480);
        assert(g.seats[1].chips == 80);
        assert(g.seats[2].chips == 0);
        assert(g.seats[2].seat_state == SeatState::AllIn);
    }

    // HB17: Heads-Up pre-flop. P1 posts SB(5), P0 posts BB(10). P1 calls 5, P0 checks.
    {
        resetSeats({{SeatState::InHand, 500}, {SeatState::InHand, 500}});
        g.dealer = 0;
        g.state = GameState::PreFlop;
        setActions({
            {BetType::Call, 5},
            {BetType::Check, std::nullopt}
        });
        auto bets = runBettingRound();
        assert(bets[0] == 10);
        assert(bets[1] == 10);
        assert(g.seats[0].chips == 490);
        assert(g.seats[1].chips == 490);
    }

    std::cout << "  handleBettingRound: all 17 tests passed\n";
}

// ---------------------------------------------------------------------------
// processBetsOnRoundConclusion tests
// ---------------------------------------------------------------------------

void PokerTests::testProcessBetsOnRoundConclusion() {
    std::cout << "=== processBetsOnRoundConclusion tests ===\n";

    // PBORC1: Standard round, no all-ins.
    {
        resetSeats({{SeatState::InHand, 100}, {SeatState::InHand, 100}, {SeatState::InHand, 100}});
        std::unordered_map<size_t, bet_t> bets = {{0, 10}, {1, 10}, {2, 10}};
        g.processBetsOnRoundConclusion(bets);
        assert(g.pots.size() == 1);
        assert(g.pots[0].amount == 30);
        assert(g.pots[0].participants.size() == 3);
        assert(g.max_pot_sizes.empty());
    }

    // PBORC2: One player all-in for less.
    {
        resetSeats({{SeatState::AllIn, 0}, {SeatState::InHand, 100}, {SeatState::InHand, 100}});
        std::unordered_map<size_t, bet_t> bets = {{0, 5}, {1, 10}, {2, 10}};
        g.processBetsOnRoundConclusion(bets);
        assert(g.pots.size() == 2);
        assert(g.pots[0].amount == 15);
        assert(g.pots[0].participants.size() == 3);
        assert(g.pots[1].amount == 10);
        assert(g.pots[1].participants.size() == 2);
        assert(g.max_pot_sizes.size() == 1);
        assert(g.max_pot_sizes[0] == 5);
    }

    // PBORC3: Two players all-in for different amounts.
    {
        resetSeats({{SeatState::AllIn, 0}, {SeatState::AllIn, 0}, {SeatState::InHand, 100}});
        std::unordered_map<size_t, bet_t> bets = {{0, 5}, {1, 15}, {2, 20}};
        g.processBetsOnRoundConclusion(bets);
        assert(g.pots.size() == 3);
        assert(g.pots[0].amount == 15);
        assert(g.pots[1].amount == 20);
        assert(g.pots[2].amount == 5);
        assert(g.max_pot_sizes.size() == 2);
        assert(g.max_pot_sizes[0] == 5);
        assert(g.max_pot_sizes[1] == 10);
    }

    // PBORC4: Folded player contributes but doesn't participate.
    {
        resetSeats({{SeatState::InHand, 100}, {SeatState::Folded, 0}});
        std::unordered_map<size_t, bet_t> bets = {{1, 5}, {0, 10}};
        g.processBetsOnRoundConclusion(bets);
        assert(g.pots.size() == 1);
        assert(g.pots[0].amount == 15);
        assert(g.pots[0].participants.size() == 1);
        assert(g.pots[0].participants.contains(0));
    }

    // PBORC5: Multi-round side pot contribution.
    {
        resetSeats({{SeatState::AllIn, 0}, {SeatState::InHand, 100}, {SeatState::InHand, 100}});
        std::unordered_map<size_t, bet_t> bets1 = {{0, 10}, {1, 10}, {2, 10}};
        g.processBetsOnRoundConclusion(bets1);
        assert(g.pots.size() == 1);
        assert(g.pots[0].amount == 30);
        assert(g.max_pot_sizes.size() == 1);
        assert(g.max_pot_sizes[0] == 10);

        std::unordered_map<size_t, bet_t> bets2 = {{1, 20}, {2, 20}};
        g.processBetsOnRoundConclusion(bets2);
        assert(g.pots.size() == 2);
        assert(g.pots[0].amount == 50);
        assert(g.pots[1].amount == 20);
    }

    // PBORC6: All players all-in same amount.
    {
        resetSeats({{SeatState::AllIn, 0}, {SeatState::AllIn, 0}});
        std::unordered_map<size_t, bet_t> bets = {{0, 50}, {1, 50}};
        g.processBetsOnRoundConclusion(bets);
        assert(g.pots.size() == 1);
        assert(g.pots[0].amount == 100);
        assert(g.max_pot_sizes.size() == 1);
        assert(g.max_pot_sizes[0] == 50);
    }

    std::cout << "  processBetsOnRoundConclusion: all 6 tests passed\n";
    std::cout << "All tests passed!\n";
}

} // namespace pokergame::tests

// ---------------------------------------------------------------------------
// Bridge: called by PokerGame::start() in poker_game.cpp
// ---------------------------------------------------------------------------

namespace pokergame::core {

void runTests() {
    pokergame::tests::PokerTests tests;
    tests.run();
}

} // namespace pokergame::core
