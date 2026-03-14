#include "http_routes.h"

#include <jwt-cpp/jwt.h>

namespace pokergame::network::http {

    void HttpRoutes::createGame(uWS::HttpResponse<false> *res, uWS::HttpRequest *req) {
        res->writeStatus("200 OK");
        res->writeHeader("Content-Type", "text/html");
        res->write("Hello");
        res->end(req->getParameter("name"));
    }
}
