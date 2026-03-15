#pragma once

#include "App.h"
#include <string>

namespace pokergame::network::http {
    class HttpRoutes {
    public:
        static HttpRoutes &instance() {
            static HttpRoutes http_routes;
            return http_routes;
        }

        // Creates a room and a game.
        void createGame(uWS::HttpResponse<false> *, uWS::HttpRequest *);

        void joinRoom(uWS::HttpResponse<false> *, uWS::HttpRequest *);

        void leaveRoom(uWS::HttpResponse<false> *, uWS::HttpRequest *);


        ~HttpRoutes() = default;

    private:
        HttpRoutes() = default;
    };

    using RoutingHandler = void (HttpRoutes::*)(uWS::HttpResponse<false> *, uWS::HttpRequest *);

    struct RouteDefinition {
        std::string method;
        std::string route;
        RoutingHandler handler;
    };

    const std::string base_path = "/v1/poker";

    constexpr std::array<RouteDefinition, 3> route_definitions = {
        {
            {"POST", "/createGame", &HttpRoutes::createGame},
            {"POST", "/joinRoom", &HttpRoutes::joinRoom},
            {"POST", "/leaveRoom", &HttpRoutes::leaveRoom},
        }
    };
};
