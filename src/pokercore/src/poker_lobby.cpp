
#include "poker_core.h"

namespace pokergame::core {

    PokerLobby::PokerLobby() {}

    std::string PokerLobby::createGame(const PokerConfiguration& poker_configuration) {
        auto game_id = generate_unique_random_secret(8);
        this->games[game_id] = std::make_unique<PokerGame>(poker_configuration);
        return game_id;
    }

    bool PokerLobby::seatPlayer(const std::string& game_id, const std::string& player_name, const size_t seat_index) {
        if (this->games.contains(game_id)) {
            return this->games[game_id]->seatPlayer(player_name, seat_index);
        }
        return false;
    }

}