#include "events.h"

#include <iostream>

namespace pokergame::core::events {

    void INotifier::cancelCallback(const std::string &, const uint8_t) {}

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
        j["allowed_actions"] = this->allowed_actions;
        j["amount_owed"] = this->amount_owed;
    }

    void GameStateNotification::toJson(nlohmann::json &j) const {
        Notification::toJson(j);
        j["round"] = types::gameStateToString(this->round);
        j["seats"] = nlohmann::json::array();
        for (const auto& seat: seats) {
            j["seats"].push_back( {
                {"chips", seat.chips},
                {"name", seat.name},
                {"seat_state", types::seatStateToString(seat.seat_state)}
            } );
        }
    }

    void PlayerEventAcknowledgement::toJson(nlohmann::json &j) const {
        Notification::toJson(j);
        j["success"] = success;
        j["system_initiated"] = system_initiated;
        j["message"] = message;
    }

    std::optional<std::unique_ptr<PlayerEvent>> from_json(std::string_view str) {
        nlohmann::json j = nlohmann::json::parse(str);

        const auto type = j.at("event_type").get<PlayerEventType>();

        try {
            switch (type) {
                case PlayerEventType::START:
                    return std::make_unique<StartEvent>();
                case PlayerEventType::BETTING_ACTION: {
                    std::optional<types::bet_t> amount;
                    if (j.contains("amount") and !j["amount"].is_null()) {
                        amount = j.at("amount").get<types::bet_t>();
                    }
                    return std::make_unique<BettingEvent>(
                        j.at("bet_type").get<types::BetType>(),
                        amount
                    );
                }
                case PlayerEventType::SEAT_PLAYER:
                    return std::make_unique<SeatPlayerEvent>(
                        j.at("seat_id").get<size_t>()
                    );
                case PlayerEventType::UNSEAT_PLAYER:
                    return std::make_unique<UnseatPlayerEvent>();
            }
        } catch (const nlohmann::json::exception &e) {
            std::cerr << "JSON Error: " << e.what() << std::endl;
            return std::nullopt;
        }

        return std::nullopt;
    }

}
