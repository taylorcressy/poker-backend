//
// Created by tcressy on 3/13/26.
//

#include "App.h"
#include "poker_network.h"
#include "http_routes.h"

#include <functional>
#include <iostream>


namespace pokergame::network {

    void PokerServer::start() {
        // static int port = 9001;
        // auto app = uWS::App();
        //
        // for (const auto& definition: http::route_definitions) {
        //     if (definition.method == "GET") {
        //         app.get(http::base_path + definition.route, [definition](auto *res, auto *req) {
        //            std::invoke(definition.handler, http::HttpRoutes::instance(), res, req);
        //         });
        //     }
        //     else if (definition.method == "POST") {
        //         app.post(http::base_path + definition.route, [definition](auto *res, auto *req) {
        //             std::invoke(definition.handler, http::HttpRoutes::instance(), res, req);
        //         });
        //     }
        // }
        //
        //
        // app.listen(port, [](const auto *listen_socket) {
        //     if (listen_socket) {
        //         std::cout << "Listening on port " << port << std::endl;
        //     }
        //     else {
        //         std::cout << "Failed to listen to port " << port << std::endl;
        //     }
        // });
        //
        // app.run();
    }

}
