#include <algorithm>
#include <cassert>

#include "events.h"
#include "poker_core.h"
#include "detail/poker_validator.h"
#include <iostream>
#include <ranges>

namespace pokergame::core {
    using namespace types;

    PokerGame::PokerGame(const PokerConfiguration &poker_configuration,
                         std::shared_ptr<events::INotifier> notifier,
                         const std::string_view game_id) : config{poker_configuration},
                                                           notifier{std::move(notifier)},
                                                           game_id{game_id},
                                                           seats(poker_configuration.number_of_seats) {
        this->community_cards.reserve(5);
    }

    void PokerGame::publishGameState() {
        events::GameStateNotification notification{
            this->state,
            this->seats,
            std::nullopt
        };

        this->notifier->sendMessageToTable(this->game_id, &notification);
    }

    bool PokerGame::seatPlayer(const std::string &name, const size_t seat_index) {
        if (this->username_to_seat.contains(name)) {
            return false;
        }

        auto &seat = this->seats[seat_index];
        if (seat.seat_state != SeatState::Empty) {
            return false;
        }

        seat.seat_state = SeatState::InHand; // TODO: Need a better start state
        seat.name = name;
        seat.chips = this->config.chips_when_seated;
        this->username_to_seat[seat.name] = seat_index;
        this->publishGameState();
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
            default:
                throw std::runtime_error("Invalid BetType received in takeBet");
        }
    }

    /**
     * Pot calculation logic: This function is run after any betting round. What makes pot caculated
     * complicated is that in any round, multiple pots can be created. This happens when a player
     * goes all in. As such, this function expects that the game logic outside of this function
     * accurately sets state such as "all in", "in hand" and "folded".
     * @param bet_map
     * @param bets
     */
    void PokerGame::processBetsOnRoundConclusion(std::unordered_map<size_t, bet_t> &bet_map) {
        auto sort = [&](const std::pair<size_t, bet_t> &a, const std::pair<size_t, bet_t> &b) -> bool {
            return a.second < b.second;
        };

        // Sort from lowest to highest bets
        std::vector<std::pair<size_t, bet_t> > bets;
        bets.reserve(bet_map.size());
        for (const auto &[player_idx, bet]: bet_map) {
            bets.emplace_back(player_idx, bet);
        }
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
                this->pots.emplace_back(remaining_amount_to_bet, std::unordered_set{player_idx});
                last_pot_bet_on = this->pots.size() - 1;
                last_allocation = remaining_amount_to_bet;
            }

            if (this->seats[player_idx].seat_state == SeatState::AllIn && this->max_pot_sizes.size() <=
                last_pot_bet_on) {
                this->max_pot_sizes.push_back(last_allocation);
            }
        }

        // Next street
        std::optional<GameState> next_street = std::nullopt;
        switch (this->state) {
            case GameState::PreFlop:
                next_street = GameState::Flop;
                break;
            case GameState::Flop:
                next_street = GameState::Turn;
                break;
            case GameState::Turn:
                next_street = GameState::River;
                break;
            default:
                break;
        }
        this->gotoNextStreetOrFinishRound(next_street);
    }

    void PokerGame::requestPlayerAction(const size_t player,
                                        const std::unordered_set<BetType> &allowed_actions,
                                        const bet_t amount_owed) {
        events::BettingAction requested_action{allowed_actions, amount_owed};
        this->state_before_requested_action = this->state;
        this->await_action_from = player;
        this->state = GameState::RequestingPlayerAction;

        const auto user_timed_out = [this, player] {
            // Calculate what BettingAction
            const auto allowed = this->getAllowedActions();
            BetType bet_type = BetType::Fold;
            if (allowed.contains(BetType::Check)) {
                bet_type = BetType::Check;
            }
            auto betting_event = events::BettingEvent{bet_type, std::nullopt};
            auto player_ack = this->handlePlayerEvent(this->seats[player].name, &betting_event, false);
            this->notifier->sendMessageToPlayer(this->game_id, this->seats[player].name, &player_ack);
        };

        this->notifier->sendMessageToPlayerWithTimeoutCallback(this->game_id,
                                                               this->seats[player].name,
                                                               &requested_action,
                                                               this->config.time_to_response_in_seconds,
                                                               user_timed_out
        );
    }

    events::PlayerEventAcknowledgement PokerGame::handlePlayerEvent(const std::string_view username,
                                                                    events::PlayerEvent *player_event,
                                                                    const bool system_initiated) {

        switch (player_event->event_type) {
            case events::PlayerEventType::START: {
                if (this->state != GameState::NotStarted) {
                    return events::PlayerEventAcknowledgement{false, system_initiated, "Game already started"};
                }
                if (!this->canGameStart()) {
                    return events::PlayerEventAcknowledgement{false, system_initiated, "Game can't start"};
                }
                // TODO: Verify event came from owner of game

                this->handleInit();
                return events::PlayerEventAcknowledgement{true, system_initiated, "Game Started"};
            }
            case events::PlayerEventType::BETTING_ACTION: {
                if (state != GameState::RequestingPlayerAction) {
                    return events::PlayerEventAcknowledgement{false, system_initiated, "Not your turn"};
                }
                const auto iter = this->username_to_seat.find(std::string(username));
                if (iter == this->username_to_seat.end()) {
                    return events::PlayerEventAcknowledgement{false, system_initiated, "Player not found"};
                }
                const size_t player_id = iter->second;
                if (player_id != this->await_action_from) {
                    return events::PlayerEventAcknowledgement{false, system_initiated, "Not your turn"};
                }
                const auto bet_event = dynamic_cast<events::BettingEvent*>(player_event);
                if (bet_event == nullptr) {
                    return events::PlayerEventAcknowledgement{false, system_initiated, "Unknown Error"};
                }
                this->await_action_from = std::nullopt;
                this->state = *state_before_requested_action;
                this->state_before_requested_action = std::nullopt;
                onPlayerBettingAction(*bet_event);
                return events::PlayerEventAcknowledgement{true, system_initiated, "Bet Accepted"};
            }
            case events::PlayerEventType::SEAT_PLAYER: {
                const auto seat_player_event = dynamic_cast<events::SeatPlayerEvent*>(player_event);
                if (seat_player_event == nullptr) {
                    return events::PlayerEventAcknowledgement{false, system_initiated, "Unknown error"};
                }
                const bool result = this->seatPlayer(std::string(username), seat_player_event->seat_id);
                return events::PlayerEventAcknowledgement{result, system_initiated, result ? "Player Seated" : "Seat Occupied"};
            }

            case events::PlayerEventType::UNSEAT_PLAYER: {
                throw std::runtime_error("Not implemented");
            }
        }

        return events::PlayerEventAcknowledgement{false, system_initiated, "Unknown Error"};
    }


    void PokerGame::handleInit() {
        // This should only be called once per game. A new game happens when >= 2
        // players have been seated.
        this->state = GameState::Init;
        std::vector<size_t> seated_players;

        seated_players.reserve(this->seats.size());
        for (size_t i = 0; i < this->seats.size(); i++) {
            if (this->seats[i].seat_state != SeatState::Empty) {
                seated_players.push_back(i);
            }
        }

        this->deck.shuffle();
        std::vector<size_t> dealer_candidates;
        dealer_candidates.reserve(seated_players.size());
        for (const size_t &player: seated_players) {
            dealer_candidates.push_back(player);
        }

        const auto sorter = [&](const size_t &a, const size_t &b) -> bool {
            return rankIndex(this->seats[a].hand[0].rank) > rankIndex(this->seats[b].hand[0].rank);
        };

        bool dealer_found = false;

        while (!dealer_found) {
            for (const size_t &i: seated_players) {
                this->seats[i].hand.push_back(this->deck.drawCard());
            }
            std::ranges::sort(dealer_candidates.begin(), dealer_candidates.end(), sorter);
            const auto highCardIndex = rankIndex(this->seats[dealer_candidates[0]].hand[0].rank);

            for (size_t i = dealer_candidates.size() - 1; i >= 1; i--) {
                const auto currentRankIndex = rankIndex(this->seats[i].hand[0].rank);
                if (currentRankIndex < highCardIndex) {
                    dealer_candidates.pop_back();
                }
            }

            if (dealer_candidates.size() == 1) {
                dealer_found = true;
            }
        }

        this->dealer = dealer_candidates[0];
        this->handlePreFlop();
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
                if (seat.chips == 0) {
                    seat.seat_state = SeatState::Empty;
                    this->username_to_seat.erase(seat.name);
                    seat.name = "";
                    continue;
                }
                seat.seat_state = SeatState::InHand;
                count++;
            }
        }

        if (count <= 1) {
            this->state = GameState::NotStarted;
        }
    }

    void PokerGame::updatePlayersWhoOweActions(const size_t aggressor) {
        players_who_owe_action.clear();
        for (size_t offset = 1; offset < this->seats.size(); ++offset) {
            const size_t idx = (aggressor + offset) % this->seats.size();
            if (this->seats[idx].seat_state == SeatState::InHand && idx != aggressor) {
                players_who_owe_action.insert(idx);
            }
        }
    }

    std::unordered_set<BetType> PokerGame::getAllowedActions() {
        // Assumes player is InHand
        if (this->first_pass_concluded && this->current_better == first_to_act && current_bet_to_match ==
            std::nullopt) {
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
    }

    // TODO: seat state changes. i.e. someone folds. Particularly thinking about last aggressor / first better / etc
    void PokerGame::startBettingRound() {
        current_better = nextBetter(dealer);
        // TODO: If current better is null, then there is only one player. Which shouldn't happen here?
        assert(current_better.has_value());
        current_bet_to_match = std::nullopt;
        this->first_pass_concluded = false;

        // Handle antes
        if (this->state == GameState::PreFlop) {
            const size_t number_of_players = numberOfSeatsByState(this->seats, SeatState::InHand);
            if (number_of_players == 2) {
                current_better = dealer;
            }
            assert(current_better.has_value());
            bet_t small_blind = *takeBet(*current_better, BetType::SmallBlind);
            bets[*current_better] = small_blind;
            current_better = nextBetter(*current_better);
            assert(current_better.has_value());
            bet_t big_blind = *takeBet(*current_better, BetType::BigBlind);
            bets[*current_better] = big_blind;
            this->updatePlayersWhoOweActions(*current_better);
            current_better = nextBetter(*current_better);
            assert(current_better.has_value());

            if (small_blind > big_blind) {
                current_bet_to_match = small_blind;
            } else {
                current_bet_to_match = big_blind;
            }
        }

        this->first_to_act = *current_better;
        this->advanceToNextBetter();
    }

    void PokerGame::advanceToNextBetter() {
        const std::unordered_set<BetType> better_allowed_actions = this->getAllowedActions();
        if (!better_allowed_actions.empty()) {
            bet_t amount_owed = 0;
            if (current_bet_to_match.has_value() && bets[*current_better] < *current_bet_to_match) {
                amount_owed = *current_bet_to_match - bets[*current_better];
            }

            this->requestPlayerAction(
                *current_better,
                better_allowed_actions,
                amount_owed
            );
        } else {
            this->processBetsOnRoundConclusion(bets);
        }
    }

    void PokerGame::onPlayerBettingAction(const events::BettingEvent &betting_event) {
        const auto action = betting_event.bet_type;
        const auto amount = betting_event.amount;
        const auto potential_bet = takeBet(*current_better, action, amount);
        if (potential_bet.has_value()) {
            bets[*current_better] += *potential_bet;
        }
        if (action == BetType::Bet || action == BetType::Raise) {
            current_bet_to_match = bets[*current_better];
            this->updatePlayersWhoOweActions(*current_better);
        } else {
            players_who_owe_action.erase(*current_better);
        }

        const auto potentially_next_better = nextBetter(*current_better);
        if (potentially_next_better == std::nullopt) {
            this->processBetsOnRoundConclusion(bets);
            return;
        }
        current_better = potentially_next_better;
        this->advanceToNextBetter();
    }

    void PokerGame::gotoNextStreetOrFinishRound(const std::optional<GameState> next_street) {
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
            for (auto &seat: this->seats) {
                if (seat.seat_state == SeatState::AllIn || seat.seat_state == SeatState::InHand) {
                    for (auto& pot: this->pots) {
                        seat.chips += pot.amount;
                    }
                    break;
                }
            }

            this->state = GameState::RoundComplete; // TODO: Is this right?
        }

        switch (this->state) {
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
            default:
                assert(false); // Should never get here.
        }
    }

    void PokerGame::handlePreFlop() {
        // TODO: Check to see if people left the table and we need to stop the game loop
        // This is the only place this can happen (even if players leave their seats during the round).

        this->state = GameState::PreFlop;
        this->resetTableStateForNewRound();
        if (this->state == GameState::NotStarted) {
            return;
        }
        for (auto& seat: seats) {
            if (seat.seat_state == SeatState::InHand) {
                seat.hand.emplace_back(this->deck.drawCard());
                seat.hand.emplace_back(this->deck.drawCard());
            }
        }
        this->publishGameState();
        this->startBettingRound();
    }

    void PokerGame::handleFlop() {
        this->state = GameState::Flop;
        for (size_t i = 0; i < 3; i++) {
            this->community_cards.push_back(this->deck.drawCard());
        }
        this->startBettingRound();
    }

    void PokerGame::handleTurn() {
        this->state = GameState::Turn;
        this->community_cards.push_back(this->deck.drawCard());
        this->startBettingRound();
    }

    void PokerGame::handleRiver() {
        this->community_cards.push_back(this->deck.drawCard());
        this->startBettingRound();
    }


    // TODO: What happens if everyone folds and a guy just takes the pots.

    // TODO: Do we need handle round complete?
    void PokerGame::handleShowdown() {
        // If streets have not occurred, add to community cards
        while (this->community_cards.size() < 5) {
            this->community_cards.emplace_back(this->deck.drawCard());
        }

        auto player_left_of_player = [&](const size_t start_player,
                                         const std::unordered_set<size_t> &eligible_players) -> size_t {
            size_t current_player = start_player + 1;
            if (current_player == this->seats.size()) {
                current_player = 0;
            }

            while (current_player != start_player) {
                if (current_player >= this->seats.size()) {
                    current_player = 0;
                }
                if (eligible_players.contains(current_player)) {
                    return current_player;
                }
                current_player += 1;
            }

            // Should never reach here...
            assert(false);
        };

        for (const auto &[amount, participants]: this->pots) {
            std::unordered_map<size_t, std::vector<Card> > final_cards;
            for (const auto &seat_idx: participants) {
                const auto &seat = this->seats[seat_idx];
                std::vector<Card> seats_hand;
                seats_hand.reserve(7);
                seats_hand.insert(seats_hand.end(), this->community_cards.begin(), this->community_cards.end());
                seats_hand.insert(seats_hand.end(), seat.hand.begin(), seat.hand.end());
                final_cards.insert_or_assign(seat_idx, seats_hand);
            }

            std::vector<ShowdownResult> winners = ShowdownEvaluator::evaluate(final_cards);
            // TODO: winners[N].result == The hand that was won, do something with it later.
            // payout the winners
            assert(!winners.empty());
            std::unordered_map<size_t, bet_t> payout_amounts;
            const bet_t base_payout_amount = amount / winners.size();
            for (const auto &winner: winners) {
                payout_amounts[winner.winner_index] = base_payout_amount;
            }

            const size_t leftover_chips = amount % winners.size();
            if (winners.size() > 1 && leftover_chips > 0) {
                std::unordered_set<size_t> eligible_participants_for_extra_payout;

                size_t source_player = this->dealer;
                for (const auto &winner: winners) {
                    eligible_participants_for_extra_payout.insert(winner.winner_index);
                }

                for (size_t i = 0; i < leftover_chips; i++) {
                    size_t closest_player =
                            player_left_of_player(source_player, eligible_participants_for_extra_payout);
                    payout_amounts[closest_player] += 1;
                    source_player = closest_player;
                }
            }

            for (const auto &[player_idx, payout]: payout_amounts) {
                this->seats[player_idx].chips += payout;
            }
            this->publishGameState();
        }
    }

    void PokerGame::handleRoundComplete() {
        this->state = GameState::RoundComplete;
        events::GameStateNotification notification{
            this->state,
            this->seats,
            10
        };
        do {
            this->dealer = (this->dealer + 1) % this->seats.size();
        } while (this->seats.at(dealer).seat_state == SeatState::Empty);

        this->notifier->sendMessageToTableWithTimeoutCallback(this->game_id, &notification, 10, [this] {
            this->handlePreFlop();
        });
    }
};
