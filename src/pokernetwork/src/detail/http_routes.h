#pragma once

#include "App.h"
#include <string>

namespace pokergame::network::http {
    class HttpRoutes {
    public:
        static HttpRoutes& instance() {
            static HttpRoutes http_routes;
            return http_routes;
        }

        void createGame(uWS::HttpResponse<false>*, uWS::HttpRequest*);

        ~HttpRoutes() = default;
    private:
        HttpRoutes() = default;
    };

    using RoutingHandler = void (HttpRoutes::*)(uWS::HttpResponse<false>*, uWS::HttpRequest*);

    struct RouteDefinition {
        std::string method;
        std::string route;
        RoutingHandler handler;
    };

    const std::string base_path = "/v1/path";

    constexpr std::array<RouteDefinition, 1> route_definitions = {
       {"POST", "/createGame", &HttpRoutes::createGame}
   };
};
