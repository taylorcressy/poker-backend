#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace pokergame::core::notifications {
    struct Notification {
        std::string type;

        explicit Notification(std::string t): type(std::move(t)) {}
        virtual ~Notification() = default;

        virtual void toJson(nlohmann::json& j) const {
            j["type"] = type;
        }

        std::string dump() const {
            nlohmann::json j;
            toJson(j);
            return j.dump();
        }
    };

    struct BettingAction : Notification {
        std::string action;

        BettingAction(): Notification("BettingAction"), action("") {}
        BettingAction(std::string a): Notification("BettingAction"), action(std::move(a)) {}

        void toJson(nlohmann::json &j) const override {
            Notification::toJson(j);
            j["action"] = action;
        }

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(BettingAction, type, action);
    };


    std::optional<std::unique_ptr<Notification>> messageToNotification(const std::string&);

    class INotifier {
    public:
        virtual void sendMessageToPlayer(const std::string& room_id, const std::string& player_id, Notification*) = 0;
        virtual void sendMessageToTable(const std::string& room_id, Notification*) = 0;

        INotifier() = default;
        virtual ~INotifier() = default;
    };
};
