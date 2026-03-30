#pragma once

#include "App.h"

#include "poker_core.h"

namespace pokergame::network::http {
    class HttpRoutes {
    public:
        static HttpRoutes &instance() {
            static HttpRoutes http_routes;
            return http_routes;
        }

        // Creates a room and a game.
        void createGame(uWS::HttpResponse<false> *, uWS::HttpRequest *, core::PokerLobby*, std::shared_ptr<core::events::INotifier>, std::function<void(std::string)> game_created_callback);

        void joinRoom(uWS::HttpResponse<false> *, uWS::HttpRequest *, core::PokerLobby*, uWS::Loop*);

        void leaveRoom(uWS::HttpResponse<false> *, uWS::HttpRequest *, core::PokerLobby*, uWS::Loop*, std::function<void()>);

        void upgradeToWs(uWS::HttpResponse<false> *, uWS::HttpRequest *, us_socket_context_t*);

        ~HttpRoutes() = default;

    private:
        HttpRoutes() = default;
    };
};
