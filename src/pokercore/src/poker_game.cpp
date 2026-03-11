#include <algorithm>
#include <assert.h>

#include "poker_core.h"
#include "detail/poker_validator.h"
#include <iostream>
#include <ranges>

namespace pokergame {
    PokerGame::PokerGame(const PokerConfiguration poker_configuration) : config{poker_configuration},
                                                                         community_cards(0),
                                                                         seats(poker_configuration.number_of_seats),
                                                                         state{GameState::NotStarted},
                                                                         dealer{static_cast<size_t>(-1)},
                                                                         pots(0),
                                                                         max_pot_sizes(0) {
        this->community_cards.reserve(5);
    }

    bool PokerGame::seatPlayer(const std::string &name, const long chips, const short seat_index) {
        auto &seat = this->seats[seat_index];
        if (seat.seat_state != SeatState::Empty || this->config.minimum_number_of_chips_to_start < chips) {
            return false;
        }

        seat.seat_state = SeatState::InHand; // TODO: Need a better state
        seat.name = name;
        seat.chips = chips;
        return true;
    }

    int numberOfSeatsByState(std::vector<Seat> &seats, const SeatState state) {
        int count_of_state = 0;
        for (const auto &seat: seats) {
            if (seat.seat_state == state) {
                count_of_state += 1;
            }
        }

        return count_of_state;
    }

    bool PokerGame::canGameStart() {
        if (this->state != GameState::NotStarted) {
            return false;
        }

        if (numberOfSeatsByState(this->seats, SeatState::Empty) >= this->seats.size() - 1) {
            // One or zero players seated, can't start
            return false;
        }

        return true;
    }

    std::optional<size_t> PokerGame::nextBetter(const size_t other_player) {
        for (size_t offset = 1; offset < this->seats.size(); ++offset) {
            const size_t idx = (other_player + offset) % this->seats.size();
            if (this->seats[idx].seat_state == SeatState::InHand) {
                return idx;
            }
        }

        return std::nullopt;
    }

    std::optional<bet_t> PokerGame::takeBet(const size_t player,
                                            const BetType type,
                                            const std::optional<bet_t> amount = std::nullopt) {
        auto take_chips = [&](const size_t player_idx, const bet_t amnt) -> bet_t {
            if (this->seats[player_idx].chips < amnt) {
                const bet_t actual_amount = this->seats[player_idx].chips;
                this->seats[player_idx].chips = 0;
                this->seats[player_idx].seat_state = SeatState::AllIn;
                return actual_amount;
            }
            this->seats[player_idx].chips -= amnt;
            return amnt;
        };

        switch (type) {
            case BetType::Check:
                return std::nullopt;
            case BetType::SmallBlind:
                return take_chips(player, this->config.ante);
            case BetType::BigBlind:
                return take_chips(player, this->config.ante * 2);
            case BetType::Fold:
                this->seats[player].seat_state = SeatState::Folded;
                return std::nullopt;
            case BetType::Call:
            case BetType::Bet:
            case BetType::Raise:
                assert(amount.has_value());
                return take_chips(player, *amount);
        }
    }

    /**
     * Pot calculation logic: This function is run after any betting round. What makes pot caculated
     * complicated is that in any round, multiple pots can be created. This happens when a player
     * goes all in. As such, this function expects that the game logic outside of this function
     * accurately sets state such as "all in", "in hand" and "folded".
     * @param bets
     */
    void PokerGame::processBetsOnRoundConclusion(std::vector<std::pair<size_t, bet_t> > &bets) {
        auto sort = [&](const std::pair<size_t, bet_t> &a, const std::pair<size_t, bet_t> &b) -> bool {
            return a.second < b.second;
        };

        // Sort from lowest to highest bets
        std::ranges::sort(bets.begin(), bets.end(), sort);

        // First round of betting
        if (this->pots.empty()) {
            this->pots.emplace_back();
        }

        // Go through each better from lowest to highest, determine if they went all in
        // if they have, then set a max bet size for that pot.
        const size_t start_j = pots.size() - 1;
        for (auto [player_idx, player_bet]: bets) {
            bet_t remaining_amount_to_bet = player_bet;

            bet_t last_allocation = 0;
            size_t last_pot_bet_on = 0;
            // Every round we start by evaluating from the latest pot
            for (size_t j = start_j; j < pots.size(); j++) {
                if (remaining_amount_to_bet == 0) {
                    break;
                }

                auto &pot = pots[j];
                if (j < max_pot_sizes.size()) {
                    last_allocation = max_pot_sizes[j];
                    assert(remaining_amount_to_bet >= max_pot_sizes[j]);
                    remaining_amount_to_bet -= max_pot_sizes[j];
                    pot.amount += max_pot_sizes[j];
                } else {
                    last_allocation = remaining_amount_to_bet;
                    pot.amount += remaining_amount_to_bet;
                    remaining_amount_to_bet = 0;
                }
                last_pot_bet_on = j;

                if (this->seats[player_idx].seat_state != SeatState::Folded) {
                    pot.participants.insert(player_idx);
                }
            }

            if (remaining_amount_to_bet > 0) {
                this->pots.emplace_back(Pot{remaining_amount_to_bet, {player_idx}});
                last_pot_bet_on = this->pots.size() - 1;
                last_allocation = remaining_amount_to_bet;
            }

            if (this->seats[player_idx].seat_state == SeatState::AllIn && this->max_pot_sizes.size() <=
                last_pot_bet_on) {
                this->max_pot_sizes.push_back(last_allocation);
            }
        }
    }


    void PokerGame::runTests() {
        // -----------------------------------------------------------------------
        // Test infrastructure
        // -----------------------------------------------------------------------
        // Resets the action queue owned by getPlayerAction.
        auto resetActions = [&]() {
            getPlayerAction(static_cast<size_t>(-1), {}, 0);
        };

        // Pushes one action onto getPlayerAction's internal queue.
        // bet_t(-1) is used as the wire encoding for "nullopt amount".
        auto pushAction = [&](BetType bt, std::optional<bet_t> amt) {
            bet_t enc = amt.has_value() ? *amt : static_cast<bet_t>(-1);
            getPlayerAction(static_cast<size_t>(-2), {bt}, enc);
        };

        // Resets and repopulates the action queue for one test scenario.
        auto setActions = [&](std::initializer_list<std::pair<BetType, std::optional<bet_t>>> actions) {
            resetActions();
            for (auto [bt, amt] : actions) pushAction(bt, amt);
        };

        // Clears all seats to Empty then sets seats 0..N from the provided list.
        // Also resets pots so each test starts clean.
        auto resetSeats = [&](std::initializer_list<std::pair<SeatState, bet_t>> defs) {
            for (auto &seat: this->seats) {
                seat.seat_state = SeatState::Empty;
                seat.chips = 0;
                seat.hand.clear();
            }
            this->pots.clear();
            this->max_pot_sizes.clear();
            size_t idx = 0;
            for (auto [st, chips]: defs) {
                this->seats[idx].seat_state = st;
                this->seats[idx].chips = chips;
                idx++;
            }
        };

        // -----------------------------------------------------------------------
        // takeBet tests   (config.ante = 5 → SB = 5, BB = 10)
        // -----------------------------------------------------------------------
        std::cout << "=== takeBet tests ===\n";

        // TB1: Check – nullopt returned, chips unchanged
        {
            resetSeats({{SeatState::InHand, 100}});
            auto r = takeBet(0, BetType::Check);
            assert(!r.has_value());
            assert(seats[0].chips == 100);
            assert(seats[0].seat_state == SeatState::InHand);
        }

        // TB2: SmallBlind – deducts ante (5)
        {
            resetSeats({{SeatState::InHand, 100}});
            auto r = takeBet(0, BetType::SmallBlind);
            assert(r.has_value() && *r == 5);
            assert(seats[0].chips == 95);
            assert(seats[0].seat_state == SeatState::InHand);
        }

        // TB3: BigBlind – deducts ante*2 (10)
        {
            resetSeats({{SeatState::InHand, 100}});
            auto r = takeBet(0, BetType::BigBlind);
            assert(r.has_value() && *r == 10);
            assert(seats[0].chips == 90);
            assert(seats[0].seat_state == SeatState::InHand);
        }

        // TB4: Fold – nullopt returned, seat becomes Folded, chips untouched
        {
            resetSeats({{SeatState::InHand, 100}});
            auto r = takeBet(0, BetType::Fold);
            assert(!r.has_value());
            assert(seats[0].chips == 100);
            assert(seats[0].seat_state == SeatState::Folded);
        }

        // TB5: Call – deducts exact amount requested
        {
            resetSeats({{SeatState::InHand, 100}});
            auto r = takeBet(0, BetType::Call, 30);
            assert(r.has_value() && *r == 30);
            assert(seats[0].chips == 70);
            assert(seats[0].seat_state == SeatState::InHand);
        }

        // TB6: Bet – deducts requested bet amount
        {
            resetSeats({{SeatState::InHand, 100}});
            auto r = takeBet(0, BetType::Bet, 25);
            assert(r.has_value() && *r == 25);
            assert(seats[0].chips == 75);
            assert(seats[0].seat_state == SeatState::InHand);
        }

        // TB7: Raise – deducts requested raise amount
        {
            resetSeats({{SeatState::InHand, 100}});
            auto r = takeBet(0, BetType::Raise, 40);
            assert(r.has_value() && *r == 40);
            assert(seats[0].chips == 60);
            assert(seats[0].seat_state == SeatState::InHand);
        }

        // TB8: SmallBlind all-in – chips(3) < ante(5): takes all chips, state → AllIn
        {
            resetSeats({{SeatState::InHand, 3}});
            auto r = takeBet(0, BetType::SmallBlind);
            assert(r.has_value() && *r == 3);
            assert(seats[0].chips == 0);
            assert(seats[0].seat_state == SeatState::AllIn);
        }

        // TB9: Call all-in – chips(15) < amount(20): takes all chips, state → AllIn
        {
            resetSeats({{SeatState::InHand, 15}});
            auto r = takeBet(0, BetType::Call, 20);
            assert(r.has_value() && *r == 15);
            assert(seats[0].chips == 0);
            assert(seats[0].seat_state == SeatState::AllIn);
        }

        // TB10: BigBlind with chips == ante*2 – takes full amount, chips reach 0,
        //       but state stays InHand (condition is chips < amount, strictly less-than)
        {
            resetSeats({{SeatState::InHand, 10}});
            auto r = takeBet(0, BetType::BigBlind);
            assert(r.has_value() && *r == 10);
            assert(seats[0].chips == 0);
            assert(seats[0].seat_state == SeatState::InHand);
        }

        // TB11: All-in BigBlind – chips(7) < BB(10): takes all chips, state → AllIn
        {
            resetSeats({{SeatState::InHand, 7}});
            auto r = takeBet(0, BetType::BigBlind);
            assert(r.has_value() && *r == 7);
            assert(seats[0].chips == 0);
            assert(seats[0].seat_state == SeatState::AllIn);
        }

        // TB12: All-in on Bet – chips(20) < amount(50): takes all chips, state → AllIn
        {
            resetSeats({{SeatState::InHand, 20}});
            auto r = takeBet(0, BetType::Bet, 50);
            assert(r.has_value() && *r == 20);
            assert(seats[0].chips == 0);
            assert(seats[0].seat_state == SeatState::AllIn);
        }

        // TB13: All-in on Raise – chips(30) < amount(40): takes all chips, state → AllIn
        {
            resetSeats({{SeatState::InHand, 30}});
            auto r = takeBet(0, BetType::Raise, 40);
            assert(r.has_value() && *r == 30);
            assert(seats[0].chips == 0);
            assert(seats[0].seat_state == SeatState::AllIn);
        }

        // TB14: Raise with exact chips – chips(40) == amount(40): takes full amount, state → InHand
        {
            resetSeats({{SeatState::InHand, 40}});
            auto r = takeBet(0, BetType::Raise, 40);
            assert(r.has_value() && *r == 40);
            assert(seats[0].chips == 0);
            assert(seats[0].seat_state == SeatState::InHand);
        }

        std::cout << "  takeBet: all 14 tests passed\n";

        // -----------------------------------------------------------------------
        // setNextStreetOrFinishRound tests
        // -----------------------------------------------------------------------
        std::cout << "=== setNextStreetOrFinishRound tests ===\n";

        // SNSFR1: 2+ InHand + next_street → advances to next street
        {
            resetSeats({{SeatState::InHand, 100}, {SeatState::InHand, 100}, {SeatState::InHand, 100}});
            this->state = GameState::Flop;
            setNextStreetOrFinishRound(GameState::Turn);
            assert(this->state == GameState::Turn);
        }

        // SNSFR2: 1 InHand + 1 AllIn + next_street → Showdown
        //         (in_hand_count == 1, not > 1; but in_hand + all_in == 2 > 1)
        {
            resetSeats({{SeatState::InHand, 100}, {SeatState::AllIn, 0}, {SeatState::Folded, 0}});
            this->state = GameState::Flop;
            setNextStreetOrFinishRound(GameState::Turn);
            assert(this->state == GameState::Showdown);
        }

        // SNSFR3: 0 InHand + 2 AllIn → Showdown  (in_hand=0, all_in=2, total > 1)
        {
            resetSeats({{SeatState::AllIn, 0}, {SeatState::AllIn, 0}});
            this->state = GameState::Flop;
            setNextStreetOrFinishRound(GameState::Turn);
            assert(this->state == GameState::Showdown);
        }

        // SNSFR4: 1 InHand only (rest folded) + next_street → RoundComplete
        //         (in_hand=1, all_in=0, total not > 1)
        {
            resetSeats({{SeatState::InHand, 100}, {SeatState::Folded, 0}, {SeatState::Folded, 0}});
            this->state = GameState::Flop;
            setNextStreetOrFinishRound(GameState::Turn);
            assert(this->state == GameState::RoundComplete);
        }

        // SNSFR5: 2+ InHand, no next_street (river end) → Showdown
        //         (next_street is nullopt; falls to else-if: in_hand + all_in > 1)
        {
            resetSeats({{SeatState::InHand, 100}, {SeatState::InHand, 100}});
            this->state = GameState::River;
            setNextStreetOrFinishRound(std::nullopt);
            assert(this->state == GameState::Showdown);
        }

        // SNSFR6: 1 InHand + 1 AllIn, no next_street → Showdown
        {
            resetSeats({{SeatState::InHand, 100}, {SeatState::AllIn, 0}});
            this->state = GameState::River;
            setNextStreetOrFinishRound(std::nullopt);
            assert(this->state == GameState::Showdown);
        }

        // SNSFR7: 1 InHand, 0 AllIn, no next_street → RoundComplete
        {
            resetSeats({{SeatState::InHand, 100}, {SeatState::Folded, 0}});
            this->state = GameState::River;
            setNextStreetOrFinishRound(std::nullopt);
            assert(this->state == GameState::RoundComplete);
        }

        // SNSFR8: 2 InHand + 1 AllIn + next_street → advances (in_hand_count > 1)
        {
            resetSeats({{SeatState::InHand, 100}, {SeatState::InHand, 100}, {SeatState::AllIn, 0}});
            this->state = GameState::Flop;
            setNextStreetOrFinishRound(GameState::Turn);
            assert(this->state == GameState::Turn);
        }

        // SNSFR9: 1 InHand + 2 AllIn + next_street → Showdown
        {
            resetSeats({{SeatState::InHand, 100}, {SeatState::AllIn, 0}, {SeatState::AllIn, 0}});
            this->state = GameState::Flop;
            setNextStreetOrFinishRound(GameState::Turn);
            assert(this->state == GameState::Showdown);
        }

        // SNSFR10: 1 InHand + 0 AllIn + next_street available but only 1 player remains → RoundComplete
        {
            resetSeats({{SeatState::InHand, 100}, {SeatState::Folded, 0}});
            this->state = GameState::Flop;
            setNextStreetOrFinishRound(GameState::Turn);
            assert(this->state == GameState::RoundComplete);
        }

        std::cout << "  setNextStreetOrFinishRound: all 10 tests passed\n";

        // -----------------------------------------------------------------------
        // handleBettingRound tests
        // config.ante = 5 → SB = 5, BB = 10
        // Dealer = seat 0 throughout → SB = seat 1, BB = seat 2 (pre-flop).
        // nextBetter skips Empty/AllIn/Folded; only returns InHand seats.
        // -----------------------------------------------------------------------
        std::cout << "=== handleBettingRound tests ===\n";

        // HB1: Pre-flop, 3 players, all call BB.
        //   SB(1)=5, BB(2)=10; first-to-act=0 calls 10; SB(1) calls 5 more;
        //   BB(2) gets no action (owe-queue is empty when control reaches them).
        {
            resetSeats({{SeatState::InHand, 200}, {SeatState::InHand, 200}, {SeatState::InHand, 200}});
            this->dealer = 0;
            this->state = GameState::PreFlop;
            setActions({{BetType::Call, 10}, {BetType::Call, 5}});
            handleBettingRound();
            assert(seats[0].chips == 190); // called 10
            assert(seats[1].chips == 190); // SB 5 + called 5 more
            assert(seats[2].chips == 190); // BB 10
            assert(seats[0].seat_state == SeatState::InHand);
            assert(seats[1].seat_state == SeatState::InHand);
            assert(seats[2].seat_state == SeatState::InHand);
        }

        // HB2: Pre-flop, UTG(0) folds, SB(1) calls, BB(2) wins uncontested.
        //   owe-queue after BB posts: {0,1}. P0 folds → {1}. P1 calls 5 → {}.
        //   BB gets no action.
        {
            resetSeats({{SeatState::InHand, 200}, {SeatState::InHand, 200}, {SeatState::InHand, 200}});
            this->dealer = 0;
            this->state = GameState::PreFlop;
            setActions({{BetType::Fold, std::nullopt}, {BetType::Call, 5}});
            handleBettingRound();
            assert(seats[0].seat_state == SeatState::Folded);
            assert(seats[0].chips == 200); // folded before putting in chips
            assert(seats[1].chips == 190); // SB 5 + called 5 more
            assert(seats[1].seat_state == SeatState::InHand);
            assert(seats[2].chips == 190); // BB 10
            assert(seats[2].seat_state == SeatState::InHand);
        }

        // HB3: Pre-flop, UTG(0) raises to 20; SB(1) calls 15; BB(2) calls 10.
        //   Raise resets owe-queue to {1,2}. Both call → queue empty; P0 no further action.
        {
            resetSeats({{SeatState::InHand, 200}, {SeatState::InHand, 200}, {SeatState::InHand, 200}});
            this->dealer = 0;
            this->state = GameState::PreFlop;
            setActions({{BetType::Raise, 20}, {BetType::Call, 15}, {BetType::Call, 10}});
            handleBettingRound();
            assert(seats[0].chips == 180); // raised 20
            assert(seats[1].chips == 180); // SB 5 + called 15 more
            assert(seats[2].chips == 180); // BB 10 + called 10 more
        }

        // HB4: Flop, 3 players, all check. No chips moved.
        //   first_to_act=1; P1 checks (sets first_pass_concluded=true); P2 and P0 check;
        //   back to P1: first_pass_concluded + no current_bet → loop exits.
        {
            resetSeats({{SeatState::InHand, 200}, {SeatState::InHand, 200}, {SeatState::InHand, 200}});
            this->dealer = 0;
            this->state = GameState::Flop;
            setActions({{BetType::Check, std::nullopt}, {BetType::Check, std::nullopt}, {BetType::Check, std::nullopt}});
            handleBettingRound();
            assert(seats[0].chips == 200);
            assert(seats[1].chips == 200);
            assert(seats[2].chips == 200);
        }

        // HB5: Flop, P1 bets 10; P2 calls 10; P0 calls 10.
        //   After P0 calls, owe-queue empty; P1 gets no further action.
        {
            resetSeats({{SeatState::InHand, 200}, {SeatState::InHand, 200}, {SeatState::InHand, 200}});
            this->dealer = 0;
            this->state = GameState::Flop;
            setActions({{BetType::Bet, 10}, {BetType::Call, 10}, {BetType::Call, 10}});
            handleBettingRound();
            assert(seats[0].chips == 190);
            assert(seats[1].chips == 190);
            assert(seats[2].chips == 190);
        }

        // HB6: Flop, P1 bets 10; P2 raises to 25; P0 calls 25; P1 calls 15 more.
        //   Raise resets owe-queue to {P0,P1}. Both call → queue empty; P2 no further action.
        {
            resetSeats({{SeatState::InHand, 200}, {SeatState::InHand, 200}, {SeatState::InHand, 200}});
            this->dealer = 0;
            this->state = GameState::Flop;
            setActions({{BetType::Bet, 10}, {BetType::Raise, 25}, {BetType::Call, 25}, {BetType::Call, 15}});
            handleBettingRound();
            assert(seats[0].chips == 175); // called 25
            assert(seats[1].chips == 175); // bet 10 + called 15 more
            assert(seats[2].chips == 175); // raised to 25
        }

        // HB7: Pre-flop, 2 players. Seat 1 is a short stack with only 8 chips (< BB=10).
        //   SB(1) posts 5 (3 chips left). BB(0) posts 10 (190 chips left).
        //   first-to-act = seat 1: owes 5, has only 3 → goes all-in for 3 (total bet=8).
        //   P0: owe-queue empty → no action. Verifies short-stack all-in on call.
        {
            resetSeats({{SeatState::InHand, 200}, {SeatState::InHand, 8}});
            this->dealer = 0;
            this->state = GameState::PreFlop;
            setActions({{BetType::Call, 5}});
            handleBettingRound();
            assert(seats[0].chips == 190);
            assert(seats[0].seat_state == SeatState::InHand);
            assert(seats[1].chips == 0);
            assert(seats[1].seat_state == SeatState::AllIn);
        }

        // HB8: Pre-flop, 3 players. UTG(0) and SB(1) both fold; BB(2) wins uncontested.
        //   owe-queue after BB posts: {0,1}. P0 folds → {1}. P1 folds → {}.
        //   BB(2): owe-queue empty → no action.
        {
            resetSeats({{SeatState::InHand, 200}, {SeatState::InHand, 200}, {SeatState::InHand, 200}});
            this->dealer = 0;
            this->state = GameState::PreFlop;
            setActions({{BetType::Fold, std::nullopt}, {BetType::Fold, std::nullopt}});
            handleBettingRound();
            assert(seats[0].seat_state == SeatState::Folded);
            assert(seats[0].chips == 200); // dealer position, no blind posted
            assert(seats[1].seat_state == SeatState::Folded);
            assert(seats[1].chips == 195); // posted SB of 5, then folded
            assert(seats[2].seat_state == SeatState::InHand);
            assert(seats[2].chips == 190); // posted BB of 10
        }

        // HB9: Flop, 2 players. P1 bets 10; P0 folds.
        //   After P0 folds, owe-queue empties; P1 gets no further action.
        {
            resetSeats({{SeatState::InHand, 200}, {SeatState::InHand, 200}});
            this->dealer = 0;
            this->state = GameState::Flop;
            setActions({{BetType::Bet, 10}, {BetType::Fold, std::nullopt}});
            handleBettingRound();
            assert(seats[1].chips == 190);
            assert(seats[1].seat_state == SeatState::InHand);
            assert(seats[0].seat_state == SeatState::Folded);
            assert(seats[0].chips == 200);
        }

        // HB10: Flop, 3 players. P1 checks; P2 bets 15 (late aggression); P0 and P1 both call.
        //   P1 checks first (sets first_pass_concluded, no bet set yet).
        //   P2 bets 15 → current_bet_to_match=15, owe-queue reset to {P0,P1}.
        //   P0 calls 15; P1 calls 15 (bets[P1]=0, owes full 15). P2: queue empty → done.
        {
            resetSeats({{SeatState::InHand, 200}, {SeatState::InHand, 200}, {SeatState::InHand, 200}});
            this->dealer = 0;
            this->state = GameState::Flop;
            setActions({{BetType::Check, std::nullopt}, {BetType::Bet, 15}, {BetType::Call, 15}, {BetType::Call, 15}});
            handleBettingRound();
            assert(seats[0].chips == 185);
            assert(seats[1].chips == 185);
            assert(seats[2].chips == 185);
        }

        // HB11: Re-raise scenario. P1(SB)=5, P2(BB)=10, P0(D)=UTG.
        // P0 raises to 30. P1 re-raises to 70 total. P2 calls. P0 calls.
        {
            resetSeats({{SeatState::InHand, 1000}, {SeatState::InHand, 1000}, {SeatState::InHand, 1000}});
            this->dealer = 0;
            this->state = GameState::PreFlop;
            setActions({
                {BetType::Raise, 30}, // P0 raises to 30
                {BetType::Raise, 65}, // P1 re-raises to 70 total (had 5 in)
                {BetType::Call, 60},  // P2 calls to 70 total (had 10 in)
                {BetType::Call, 40}   // P0 calls to 70 total (had 30 in)
            });
            handleBettingRound();
            assert(seats[0].chips == 930);
            assert(seats[1].chips == 930);
            assert(seats[2].chips == 930);
        }

        // HB12: Check-raise scenario. Flop, 2 players.
        // P1 checks, P2 bets 20, P1 raises to 60 total, P2 calls.
        {
            resetSeats({{SeatState::Empty, 0}, {SeatState::InHand, 500}, {SeatState::InHand, 500}});
            this->dealer = 0; // nextBetter(0) -> 1
            this->state = GameState::Flop;
            setActions({
                {BetType::Check, std::nullopt}, // P1 checks
                {BetType::Bet, 20},            // P2 bets 20
                {BetType::Raise, 60},          // P1 raises to 60 total
                {BetType::Call, 40}            // P2 calls
            });
            handleBettingRound();
            assert(seats[1].chips == 440);
            assert(seats[2].chips == 440);
        }

        // HB13: Short stack All-in on BB post.
        // P1(SB)=100, P2(BB)=5. BB should be AllIn after posting 5 chips.
        {
            resetSeats({{SeatState::Empty, 0}, {SeatState::InHand, 100}, {SeatState::InHand, 5}});
            this->dealer = 0;
            this->state = GameState::PreFlop;
            setActions({{BetType::Check, std::nullopt}});
            handleBettingRound();
            assert(seats[1].chips == 95);
            assert(seats[1].seat_state == SeatState::InHand);
            assert(seats[2].chips == 0);
            assert(seats[2].seat_state == SeatState::AllIn);
        }

        // HB14: Everyone folds to a bet.
        {
            resetSeats({{SeatState::InHand, 100}, {SeatState::InHand, 100}, {SeatState::InHand, 100}});
            this->dealer = 0;
            this->state = GameState::Flop;
            setActions({
                {BetType::Bet, 10},
                {BetType::Fold, std::nullopt},
                {BetType::Fold, std::nullopt}
            });
            handleBettingRound();
            assert(seats[1].chips == 90);
            assert(seats[2].seat_state == SeatState::Folded);
            assert(seats[0].seat_state == SeatState::Folded);
        }

        // HB15: Skip All-in player during betting action.
        {
            resetSeats({{SeatState::InHand, 100}, {SeatState::AllIn, 0}, {SeatState::InHand, 100}});
            this->dealer = 0;
            this->state = GameState::Flop;
            setActions({
                {BetType::Check, std::nullopt},
                {BetType::Check, std::nullopt}
            });
            handleBettingRound();
            assert(seats[2].chips == 100);
            assert(seats[0].chips == 100);
        }

        std::cout << "  handleBettingRound: all 15 tests passed\n";
        std::cout << "All tests passed!\n";

        resetActions(); // leave queue clean for production use
    }

    bool PokerGame::start() {
        runTests();
        // if (!this->canGameStart()) {
        //     return false;
        // }
        // this->state = GameState::Init;
        // while (this->state != GameState::RoundComplete) {
        //     this->executeNextStateTransition();
        // }
        // this->executeNextStateTransition(); // Handle round complete
        return true;
    }

    void PokerGame::executeNextStateTransition() {
        switch (this->state) {
            case GameState::Init:
                this->handleInit();
                break;
            case GameState::PreFlop:
                this->handlePreFlop();
                break;
            case GameState::Flop:
                this->handleFlop();
                break;
            case GameState::Turn:
                this->handleTurn();
                break;
            case GameState::River:
                this->handleRiver();
                break;
            case GameState::Showdown:
                this->handleShowdown();
                break;
            case GameState::RoundComplete:
                this->handleRoundComplete();
                break;
            case GameState::NotStarted:
                break;
            default:
                break;
        }
    }


    void PokerGame::handleInit() {
        // This should only be called once per game. A new game happens when >= 2
        // players have been seated.

        this->seated_players.reserve(this->seats.size());
        for (size_t i = 0; i < this->seats.size(); i++) {
            if (this->seats[i].seat_state != SeatState::Empty) {
                this->seated_players.push_back(i);
            }
        }

        this->deck.shuffle();
        std::vector<size_t> dealer_candidates;
        dealer_candidates.reserve(this->seated_players.size());
        for (const size_t &player: this->seated_players) {
            dealer_candidates.push_back(player);
        }

        const auto sorter = [&](const size_t &a, const size_t &b) -> bool {
            return rankIndex(this->seats[a].hand[0].rank) > rankIndex(this->seats[b].hand[0].rank);
        };

        bool dealer_found = false;

        while (!dealer_found) {
            for (const size_t &i: this->seated_players) {
                this->seats[i].hand.push_back(this->deck.drawCard());
            }
            std::ranges::sort(dealer_candidates.begin(), dealer_candidates.end(), sorter);
            const auto &highCardIndex = rankIndex(this->seats[dealer_candidates[0]].hand[0].rank);

            for (size_t i = dealer_candidates.size() - 1; i >= 1; i++) {
                const auto &currentRankIndex = rankIndex(this->seats[i].hand[0].rank);
                if (currentRankIndex < highCardIndex) {
                    dealer_candidates.pop_back();
                }
            }

            if (dealer_candidates.size() == 1) {
                dealer_found = true;
            }
        }

        this->dealer = dealer_candidates[0];
        this->state = GameState::PreFlop;
    }

    void PokerGame::resetTableStateForNewRound() {
        // Reset the table
        this->community_cards.clear();
        this->deck.shuffle();
        this->pots.clear();
        this->max_pot_sizes.clear();
        size_t count = 0;
        for (Seat &seat: this->seats) {
            if (!seat.hand.empty()) {
                seat.hand.clear();
            }
            if (seat.seat_state != SeatState::Empty) {
                seat.seat_state = SeatState::InHand;
                count++;
            }
        }

        if (count <= 1) {
            this->state = GameState::NotStarted;
        }
    }

    PlayerAction PokerGame::getPlayerAction(size_t player,
                                            const std::unordered_set<BetType> &allowed_actions,
                                            bet_t amount_owed) {
        // Test-infrastructure sentinels (called only from runTests):
        //   player == size_t(-1): reset queue and index
        //   player == size_t(-2): push one action; BetType = sole element of allowed_actions,
        //                         amount = amount_owed  (bet_t(-1) encodes nullopt)
        static std::vector<PlayerAction> s_queue;
        static size_t s_idx = 0;

        if (player == static_cast<size_t>(-1)) {
            s_queue.clear();
            s_idx = 0;
            return {BetType::Fold, std::nullopt};
        }
        if (player == static_cast<size_t>(-2)) {
            assert(allowed_actions.size() == 1);
            BetType bt = *allowed_actions.begin();
            std::optional<bet_t> amt = (amount_owed == static_cast<bet_t>(-1))
                                           ? std::nullopt
                                           : std::optional<bet_t>{amount_owed};
            s_queue.push_back({bt, amt});
            return {BetType::Fold, std::nullopt};
        }

        // Consume from the test queue when populated
        if (s_idx < s_queue.size()) {
            return s_queue[s_idx++];
        }

        // Default production behaviour: check > call > bet > raise > fold
        (void) player;
        if (allowed_actions.contains(BetType::Check))  return {BetType::Check, std::nullopt};
        if (allowed_actions.contains(BetType::Call))   return {BetType::Call, amount_owed};
        if (allowed_actions.contains(BetType::Bet))    return {BetType::Bet, 10};
        if (allowed_actions.contains(BetType::Raise))  return {BetType::Raise, amount_owed + 10};
        return {BetType::Fold, std::nullopt};
    }


    // TODO: We got some work. Check errors from AI. Also not handling what happens to these state variables when
    // seat state changes. i.e. someone folds. Particularly thinking about last aggressor / first better / etc
    void PokerGame::handleBettingRound() {
        std::unordered_map<size_t, bet_t> bets;
        std::optional<size_t> current_better = nextBetter(dealer);
        // TODO: If current better is null, then there is only one player. Which shouldn't happen here?
        assert(current_better.has_value());
        std::unordered_set<size_t> players_who_owe_action;
        std::optional<bet_t> current_bet_to_match = std::nullopt;
        bool first_pass_concluded = false;

        auto update_players_who_owe_actions = [&](const size_t aggressor) -> void {
            players_who_owe_action.clear();
            for (size_t offset = 1; offset < this->seats.size(); ++offset) {
                const size_t idx = (aggressor + offset) % this->seats.size();
                if (this->seats[idx].seat_state == SeatState::InHand && idx != aggressor) {
                    players_who_owe_action.insert(idx);
                }
            }
        };

        // Handle antes
        if (this->state == GameState::PreFlop) {
            assert(current_better.has_value());
            bet_t small_blind = *takeBet(*current_better, BetType::SmallBlind);
            bets[*current_better] = small_blind;
            current_better = nextBetter(*current_better);
            assert(current_better.has_value());
            bet_t big_blind = *takeBet(*current_better, BetType::BigBlind);
            bets[*current_better] = big_blind;
            update_players_who_owe_actions(*current_better);
            current_better = nextBetter(*current_better);
            assert(current_better.has_value());

            if (small_blind > big_blind) {
                current_bet_to_match = small_blind;
            } else {
                current_bet_to_match = big_blind;
            }
        }

        const size_t first_to_act = *current_better;

        auto allowed_actions = [&]() -> std::unordered_set<BetType> {
            // Assumes player is InHand
            if (first_pass_concluded && current_better == first_to_act && current_bet_to_match == std::nullopt) {
                return {};
            }

            if (current_bet_to_match.has_value() && players_who_owe_action.empty()) {
                return {};
            }
            std::unordered_set<BetType> types;
            types.insert(BetType::Fold);
            if (current_bet_to_match == std::nullopt) {
                types.insert(BetType::Check);
                types.insert(BetType::Bet);
            } else {
                assert(bets[*current_better] <= *current_bet_to_match);
                const bet_t amount_owed = *current_bet_to_match - bets[*current_better];
                if (this->seats[*current_better].chips > amount_owed) {
                    types.insert(BetType::Raise);
                }
                if (amount_owed == 0) {
                    types.insert(BetType::Check);
                } else {
                    types.insert(BetType::Call);
                }
            }


            if (current_better == first_to_act) {
                first_pass_concluded = true;
            }

            return types;
        };

        std::unordered_set<BetType> better_allowed_actions = allowed_actions();
        while (!better_allowed_actions.empty()) {
            bet_t amount_owed = 0;
            if (current_bet_to_match.has_value() && bets[*current_better] < *current_bet_to_match) {
                amount_owed = *current_bet_to_match - bets[*current_better];
            }
            const auto [action, amount] = this->getPlayerAction(
                *current_better,
                better_allowed_actions,
                amount_owed
            );

            auto potential_bet = takeBet(*current_better, action, amount);
            if (potential_bet.has_value()) {
                bets[*current_better] += *potential_bet;
            }
            if (action == BetType::Bet || action == BetType::Raise) {
                current_bet_to_match = bets[*current_better];
                update_players_who_owe_actions(*current_better);
            } else {
                players_who_owe_action.erase(*current_better);
            }

            const auto potentially_next_better = nextBetter(*current_better);
            if (potentially_next_better == std::nullopt) {
                break;
            }
            current_better = potentially_next_better;
            better_allowed_actions = allowed_actions();
        }
    }

    void PokerGame::setNextStreetOrFinishRound(std::optional<GameState> next_street) {
        using std::views::filter;
        using std::ranges::distance;
        auto in_hand = this->seats |
                       filter([](const auto &seat) { return seat.seat_state == SeatState::InHand; });
        auto all_in = this->seats |
                        filter([](const auto &seat) { return seat.seat_state == SeatState::AllIn; });

        const size_t in_hand_count = distance(in_hand);
        const size_t all_in_count = distance(all_in);
        if (in_hand_count > 1 && next_street.has_value()) {
            this->state = *next_street;
        } else if (in_hand_count + all_in_count > 1) {
            this->state = GameState::Showdown;
        } else {
            this->state = GameState::RoundComplete;
        }
    }

    void PokerGame::handlePreFlop() {
        // Take antes
        this->resetTableStateForNewRound();
        // Check to see if people left the table and we need to stop the game loop
        // This is the only place this can happen (even if players leave their seats during the round).
        if (this->state == GameState::NotStarted) {
            return;
        }
        this->handleBettingRound();
        this->setNextStreetOrFinishRound(GameState::Flop);
    }

    void PokerGame::handleFlop() {
        for (size_t i= 0; i < 3; i++) {
            this->community_cards.push_back(this->deck.drawCard());
        }
        this->handleBettingRound();
        this->setNextStreetOrFinishRound(GameState::Turn);
    }

    void PokerGame::handleTurn() {
        this->community_cards.push_back(this->deck.drawCard());
        this->handleBettingRound();
        this->setNextStreetOrFinishRound(GameState::River);
    }

    void PokerGame::handleRiver() {
        this->community_cards.push_back(this->deck.drawCard());
        this->handleBettingRound();
        this->setNextStreetOrFinishRound(std::nullopt);
    }

    void PokerGame::handleShowdown() {
        using std::views::filter;

        std::unordered_map<size_t, std::vector<Card>> final_cards;

        auto in_play = this->seats |
            filter([](const auto& card) {return card.seat_state == SeatState::InHand || card.seat_state == SeatState::AllIn;} );

        for (size_t i = 0; auto& seat: in_play) {
            std::vector<Card> seats_hand;
            seats_hand.reserve(7);
            seats_hand.insert(seats_hand.end(), this->community_cards.begin(), this->community_cards.end());
            seats_hand.insert(seats_hand.end(), seat.hand.begin(), seat.hand.end());
            final_cards[i] = seats_hand;
            i++;
        }

        std::vector<detail::ShowdownResult> result = detail::ShowdownEvaluator::evaluate(final_cards);
        // TODO: Do stuff with result
    }

    void PokerGame::handleRoundComplete() {

    }
};
