//
// Created by tcressy on 3/13/26.
//

#include "App.h"
#include "poker_network.h"
#include "detail/http_routes.h"

#include <functional>
#include <iostream>

#include "detail/auth.h"

namespace pokergame::network {

    void PokerServer::start() {
        static int port = 9001;
        auto app = uWS::App();

        for (const auto& definition: http::route_definitions) {
            if (definition.method == "GET") {
                app.get(http::base_path + definition.route, [definition](auto *res, auto *req) {
                   std::invoke(definition.handler, http::HttpRoutes::instance(), res, req);
                });
            }
            else if (definition.method == "POST") {
                app.post(http::base_path + definition.route, [definition](auto *res, auto *req) {
                    std::invoke(definition.handler, http::HttpRoutes::instance(), res, req);
                });
            }
        }

        // TODO: WS Handling
        app.ws<auth::AuthContext>("/*", {
            .upgrade = [](uWS::HttpResponse<false>* res, uWS::HttpRequest* req, us_socket_context_t* context) {
                http::HttpRoutes::instance().upgradeToWs(res, req, context);
            },
            .open = [](uWS::WebSocket<false, true, auth::AuthContext> *ws) {

            },
            .message = [](uWS::WebSocket<false, true, auth::AuthContext> *ws, std::string_view message, uWS::OpCode op_code) {

            },
            .close = [](uWS::WebSocket<false, true, auth::AuthContext> *ws, int code, std::string_view message) {

            }
        });



        app.listen(port, [](const auto *listen_socket) {
            if (listen_socket) {
                std::cout << "Listening on port " << port << std::endl;
            }
            else {
                std::cout << "Failed to listen to port " << port << std::endl;
            }
        });

        app.run();
    }

}
