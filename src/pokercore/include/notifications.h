#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include "poker_types.h"

namespace pokergame::core::notifications {
    struct Notification {
        std::string type;

        explicit Notification(std::string t) : type(std::move(t)) {
        }

        virtual ~Notification() = default;

        virtual void toJson(nlohmann::json &j) const;

        std::string dump() const;
    };

    struct BettingAction : Notification {
        std::string action;

        BettingAction() : Notification("BettingAction"), action("") {
        }

        explicit BettingAction(std::string a) : Notification("BettingAction"), action(std::move(a)) {
        }

        void toJson(nlohmann::json &j) const override;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(BettingAction, type, action);
    };


    struct GameStateNotification : Notification {
        types::GameState round;
        std::vector<types::Seat> seats;

        explicit GameStateNotification(const types::GameState round,
                                       std::vector<types::Seat> seats) : Notification("GameState"),
                                                                         round{round},
                                                                         seats{std::move(seats)} {
        }

        void toJson(nlohmann::json &j) const override;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(GameStateNotification, round);
    };

    std::optional<std::unique_ptr<Notification> > messageToNotification(const std::string &);

    class INotifier {
    public:
        virtual void sendMessageToPlayer(const std::string &room_id, const std::string &player_id, Notification *) = 0;

        virtual void sendMessageToTable(const std::string &room_id, Notification *) = 0;

        INotifier() = default;

        virtual ~INotifier() = default;
    };
};
