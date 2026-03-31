
#include "poker_core.h"
#include "utils/crypto.h"

namespace pokergame::core {
    std::string PokerLobby::createRoom(const types::PokerConfiguration &poker_configuration, const std::string &owner, std::shared_ptr<events::INotifier> notifier) {
        auto room_id = utils::crypto::generate_unique_random_secret(8);

        while (this->rooms.contains(room_id)) {
            // Shouldn't really every happen, but just in case.
            room_id = utils::crypto::generate_unique_random_secret(8);
        }

        // Users can exceed number of seats (spectators), but likely will be rare
        std::unordered_set<std::string> users;
        users.reserve(poker_configuration.number_of_seats);
        users.insert(owner);

        this->rooms.emplace(room_id, std::make_shared<PokerRoom>(PokerGame(poker_configuration, notifier, room_id), owner, users));
        return room_id;
    }


    bool PokerLobby::joinRoom(const std::string &room_id, const std::string &player_name) {
        if (const auto room_iter = this->rooms.find(room_id); room_iter != this->rooms.end()) {
            room_iter->second->users.insert(player_name);
            return true;
        }
        return false;
    }

    std::optional<events::PlayerEventAcknowledgement> PokerLobby::sendMessageToRoom(const std::string_view room_id,
        const std::string_view username,
        events::PlayerEvent* event) {

        const auto it = this->rooms.find(room_id);
        if (this->rooms.end() == it) {
            return std::nullopt;
        }

        return it->second->game.handlePlayerEvent(username, event, false);
    }

    // TODO: We want probably want to enhance this logic. If everyone drops we would want a grace period before the game just ends.
    LeaveRoomResult PokerLobby::leaveRoom(const std::string &room_id, const std::string &player_name) {
        if (const auto room_iter = this->rooms.find(room_id); room_iter != this->rooms.end()) {
            room_iter->second->users.erase(player_name);
            const auto last_user_left = room_iter->second->users.empty();
            if (last_user_left) {
                rooms.erase(room_id);
            }
            return LeaveRoomResult{ true, last_user_left};
        }
        return LeaveRoomResult{
            false, false
        };
    }

}
