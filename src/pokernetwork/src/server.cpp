//
// Created by tcressy on 3/13/26.
//

#include <iostream>
#include "App.h"
#include "poker_network.h"

namespace pokergame::network {

    void PokerServer::start() {
        static int port = 9001;
        auto app = uWS::App().get("/hello/:name", [](auto *res, auto *req) {
            res->writeStatus("200 OK");
            res->writeHeader("Content-Type", "text/html");
            res->write("Hello");
            res->end(req->getParameter("name"));
        }).listen(port, [](auto *listenSocket) {
            if (listenSocket) {
                std::cout << "Listening on port " << port << std::endl;
            }
            else {
                std::cout << "Failed to listen to port " << port << std::endl;
            }
        });

        app.run();
    }

}