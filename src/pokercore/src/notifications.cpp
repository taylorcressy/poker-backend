#include "poker_notifications.h"

namespace pokergame::core::notifications {

    std::optional<std::unique_ptr<Notification>> messageToNotification(const std::string& raw_message) {
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