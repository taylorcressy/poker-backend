#include "notifications.h"

namespace pokergame::core::notifications {

    void Notification::toJson(nlohmann::json& j) const  {
        j["type"] = type;
    }

    std::string Notification::dump() const {
        nlohmann::json j;
        toJson(j);
        return j.dump();
    }

    void BettingAction::toJson(nlohmann::json &j) const {
        Notification::toJson(j);
        j["action"] = action;
    }

    void GameStateNotification::toJson(nlohmann::json &j) const {
        Notification::toJson(j);
        j["round"] = types::gameStateToString(this->round);
    }

    std::optional<std::unique_ptr<Notification> > messageToNotification(const std::string &raw_message) {
        nlohmann::json json = nlohmann::json::parse(raw_message);

        if (!json.contains("type")) {
            return std::nullopt;
        }

        if (json["type"] == "ACTION") {
            return std::make_unique<BettingAction>(json.get<BettingAction>());
        }

        return std::nullopt;
    }
}
