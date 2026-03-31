//
// Created by tcressy on 2/11/26.
//

#pragma once
#include <optional>
#include <memory>
#include <unordered_set>
#include <vector>
#include <string>
#include <unordered_map>

#include "poker_types.h"
#include "events.h"

namespace pokergame::core {

    class PokerGame {
    public:
        explicit PokerGame(const types::PokerConfiguration& poker_configuration,
            std::shared_ptr<events::INotifier>,
            std::string_view);

        ~PokerGame() = default;

        events::PlayerEventAcknowledgement handlePlayerEvent(std::string_view username,
            events::PlayerEvent*,
            bool system_initiated);

        bool seatPlayer(const std::string &name, size_t seat_index);
        // TODO: unSeatPlayer
    private:
        const types::PokerConfiguration config;
        const std::shared_ptr<events::INotifier> notifier;
        const std::string game_id;

        types::Deck deck;
        std::vector<types::Card> community_cards = std::vector<types::Card>(0);
        std::vector<types::Seat> seats;
        std::unordered_map<std::string, size_t> username_to_seat;
        types::GameState state{types::GameState::NotStarted};
        size_t dealer{static_cast<size_t>(-1)};

        // Poker rounds can have multiple pots if someone goes all in.
        // So we maintain the total running value of the pots (pots).
        // max_pot_sizes tell us how much a player needs to bet for each particular
        // pot to be reconciled.
        std::vector<types::Pot> pots = std::vector<types::Pot>(0);
        std::vector<types::bet_t> max_pot_sizes = std::vector<types::bet_t>(0);

        // Handle betting helpers
        std::unordered_set<size_t> players_who_owe_action;
        bool first_pass_concluded{false};
        std::optional<size_t> current_better = std::nullopt;
        std::unordered_map<size_t, types::bet_t> bets;
        size_t first_to_act{static_cast<size_t>(-1)};
        std::optional<types::bet_t> current_bet_to_match = std::nullopt;

        // Player action helpers
        std::optional<types::GameState> state_before_requested_action = std::nullopt;
        std::optional<uint8_t> await_action_from = std::nullopt;


        std::optional<size_t> nextBetter(size_t other_player);

        void startBettingRound();
        void advanceToNextBetter();
        void onPlayerBettingAction(const events::BettingEvent&);

        void requestPlayerAction(size_t player,
                                     const std::unordered_set<types::BetType> &allowed_actions,
                                     types::bet_t amount_owed);

        std::optional<types::bet_t> takeBet(size_t player, types::BetType type, std::optional<types::bet_t> amount);
        void gotoNextStreetOrFinishRound(std::optional<types::GameState> next_street);

        void processBetsOnRoundConclusion(std::unordered_map<size_t, types::bet_t> &bet_map);

        void handleInit();
        void handlePreFlop();
        void handleFlop();
        void handleTurn();
        void handleRiver();
        void handleShowdown();
        void handleRoundComplete();
        bool canGameStart();

        void updatePlayersWhoOweActions(size_t);
        std::unordered_set<types::BetType> getAllowedActions();
        void resetTableStateForNewRound();

        void publishGameState();
    };

    struct PokerRoom {
        PokerGame game;
        std::string owner;
        std::unordered_set<std::string> users;
        // Add notifier here?
    };

    struct LeaveRoomResult {
        bool player_left;
        bool last_player_left;
    };

    // TODO: Determine when games are completed and remove them from the game map
    // TODO: Handle server restart
    class PokerLobby {
    public:
        explicit PokerLobby(std::string lobby_id): lobby_id(std::move(lobby_id)) {}
        ~PokerLobby() = default;

        std::string lobby_id;

        std::string createRoom(const types::PokerConfiguration& poker_configuration, const std::string& owner, std::shared_ptr<events::INotifier>);

        bool joinRoom(const std::string& room_id, const std::string& player_name);

        std::optional<events::PlayerEventAcknowledgement> sendMessageToRoom(std::string_view room_id, std::string_view username, events::PlayerEvent*);

        LeaveRoomResult leaveRoom(const std::string& room_id, const std::string& player_name);

    private:
        std::unordered_map<std::string_view, std::shared_ptr<PokerRoom>> rooms; // Room ID to Room instance

    };
}
