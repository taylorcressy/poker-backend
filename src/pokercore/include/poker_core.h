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
#include "notifications.h"

namespace pokergame::core {

    class PokerGame {
    public:
        explicit PokerGame(const types::PokerConfiguration& poker_configuration);

        ~PokerGame() = default;

        bool seatPlayer(const std::string &name, size_t seat_index);

        bool start();

        [[nodiscard]] notifications::GameStateNotification getGameState() const;

    private:
        const types::PokerConfiguration config;

        types::Deck deck;
        std::vector<types::Card> community_cards;
        std::vector<types::Seat> seats;
        std::vector<size_t> seated_players;
        types::GameState state;
        size_t dealer;

        // Poker rounds can have multiple pots if someone goes all in.
        // So we maintain the total running value of the pots (pots).
        // max_pot_sizes tell us how much a player needs to bet for each particular
        // pot to be reconciled.
        std::vector<types::Pot> pots;
        std::vector<types::bet_t> max_pot_sizes;


        std::optional<size_t> nextBetter(size_t other_player);

        void handleBettingRound();

        types::PlayerAction getPlayerAction(size_t player,
                                     const std::unordered_set<types::BetType> &allowed_actions,
                                     types::bet_t amount_owed);

        std::optional<types::bet_t> takeBet(size_t player, types::BetType type, std::optional<types::bet_t> amount);
        void setNextStreetOrFinishRound(std::optional<types::GameState> next_street);

        void processBetsOnRoundConclusion(std::unordered_map<size_t, types::bet_t> &bet_map);

        void executeNextStateTransition();
        void handleInit();
        void handlePreFlop();
        void handleFlop();
        void handleTurn();
        void handleRiver();
        void handleShowdown();
        void handleRoundComplete();
        bool canGameStart();

        void resetTableStateForNewRound();
    };

    struct PokerRoom {
        PokerGame game;
        std::string owner;
        std::unordered_set<std::string> users;
        // Add notifier here?
    };

    // TODO: Determine when games are completed and remove them from the game map
    // TODO: Handle server restart
    class PokerLobby {
    public:
        explicit PokerLobby(std::string lobby_id): lobby_id(std::move(lobby_id)) {}
        ~PokerLobby() = default;

        std::string lobby_id;

        std::string createRoom(const types::PokerConfiguration& poker_configuration, const std::string& owner);

        bool joinRoom(const std::string& room_id, const std::string& player_name);

        bool leaveRoom(const std::string& room_id, const std::string& player_name);

        std::optional<notifications::GameStateNotification> getGameState(const std::string& room_id) const;

    private:
        std::unordered_map<std::string, std::shared_ptr<PokerRoom>> rooms; // Room ID to Room instance

    };
}
