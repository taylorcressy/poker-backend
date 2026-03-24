
#include "poker_core.h"
#include "utils/crypto.h"

// TODO: Thread safety
namespace pokergame::core {
    std::string PokerLobby::createRoom(const types::PokerConfiguration &poker_configuration, const std::string &owner) {
        auto room_id = utils::crypto::generate_unique_random_secret(8);

        while (this->rooms.contains(room_id)) {
            // Shouldn't really every happen, but just in case.
            room_id = utils::crypto::generate_unique_random_secret(8);
        }

        // Users can exceed number of seats (spectators), but likely will be rare
        std::unordered_set<std::string> users;
        users.reserve(poker_configuration.number_of_seats);
        users.insert(owner);

        this->rooms.emplace(room_id, std::make_shared<PokerRoom>(PokerGame(poker_configuration), owner, users));
        return room_id;
    }


    bool PokerLobby::joinRoom(const std::string &room_id, const std::string &player_name) {
        if (this->rooms.contains(room_id)) {
            this->rooms[room_id]->users.insert(player_name);
            return true;
        }
        return false;
    }

    bool PokerLobby::leaveRoom(const std::string &room_id, const std::string &player_name) {
        if (this->rooms.contains(room_id)) {
            this->rooms[room_id]->users.erase(player_name);
            return true;
        }
        return false;
    }

    std::optional<notifications::GameStateNotification> PokerLobby::getGameState(const std::string &room_id) const {
        if (const auto it = this->rooms.find(room_id); it != this->rooms.end()) {
            return it->second->game.getGameState();
        }
        return std::nullopt;
    }
}
