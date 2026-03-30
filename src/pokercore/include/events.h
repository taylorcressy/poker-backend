#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include "poker_types.h"

namespace pokergame::core::events {
    /////
    // Incoming event types
    /////
    enum class PlayerEventType {
        START,
        BETTING_ACTION
    };

    struct PlayerEvent {
        PlayerEventType event_type;

        explicit PlayerEvent(const PlayerEventType type) : event_type{type} {
        }

        virtual ~PlayerEvent() = default;
    };

    struct StartEvent : PlayerEvent {
        StartEvent() : PlayerEvent{PlayerEventType::START} {
        }
    };

    struct BettingEvent : PlayerEvent {
        types::BetType bet_type;
        std::optional<types::bet_t> amount;

        BettingEvent(const types::BetType t, const std::optional<types::bet_t> a)
            : PlayerEvent{PlayerEventType::BETTING_ACTION}, bet_type{t}, amount{a} {
        }
    };

    ////
    //Outgoing event types
    ////
    struct Notification {
        std::string type;

        explicit Notification(std::string t) : type(std::move(t)) {
        }

        virtual ~Notification() = default;

        virtual void toJson(nlohmann::json &j) const;

        std::string dump() const;
    };

    struct BettingAction : Notification {
        std::unordered_set<types::BetType> allowed_actions;
        types::bet_t amount_owed = static_cast<size_t>(-1);

        BettingAction() : Notification("BettingAction") {
        }

        explicit BettingAction(std::unordered_set<types::BetType> actions, const size_t amount_owed) : Notification(
                "BettingAction"),
            allowed_actions{std::move(actions)},
            amount_owed{amount_owed} {
        }

        void toJson(nlohmann::json &j) const override;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(BettingAction, type, allowed_actions, amount_owed);
    };


    struct GameStateNotification : Notification {
        types::GameState round;
        std::vector<types::Seat> seats;
        std::optional<uint8_t> game_paused_for_seconds;

        explicit GameStateNotification(const types::GameState round,
                                       std::vector<types::Seat> seats,
                                       const std::optional<uint8_t> paused_for_seconds) : Notification("GameState"),
                                                                                round{round},
                                                                                seats{std::move(seats)},
                                                                                game_paused_for_seconds{paused_for_seconds} {}

        void toJson(nlohmann::json &j) const override;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(GameStateNotification, round);
    };


    struct PlayerEventAcknowledgement : Notification {
        bool success;
        bool system_initiated;
        std::string_view message;

        explicit PlayerEventAcknowledgement(
            const bool success, const bool system_initiated, const std::string_view message
        ) : Notification("PlayerEventAcknowledgement"), success{success}, system_initiated{system_initiated},
            message{message} {
        }

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(PlayerEventAcknowledgement, success, system_initiated, message);
    };


    std::optional<std::unique_ptr<Notification> > messageToNotification(const std::string &);

    class INotifier {
    public:
        virtual void sendMessageToPlayer(const std::string &room_id, const std::string &player_id, Notification *) = 0;

        // Send message to player with a timeout for them to respond. The caller must call cancelTimeoutCallBack
        // if the player responded to the request in time. The id of the callback is returned from this function
        // and must be persisted in state
        virtual uint8_t sendMessageToPlayerWithTimeoutCallback(const std::string &room_id, const std::string &player_id,
                                                               Notification *, uint8_t, std::function<void()>) = 0;

        virtual void cancelCallback(const std::string &, const uint8_t);

        virtual void sendMessageToTable(const std::string &room_id, Notification *) = 0;

        virtual uint8_t sendMessageToTableWithTimeoutCallback(const std::string &room_id, Notification *, uint8_t,
                                                              std::function<void()>) = 0;

        INotifier() = default;

        virtual ~INotifier() = default;
    };
};
