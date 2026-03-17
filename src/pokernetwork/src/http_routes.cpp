#include "detail/http_routes.h"

#include <jwt-cpp/jwt.h>

#include "detail/auth.h"
#include "poker_core.h"
#include "utils/string_utils.h"

namespace pokergame::network::http {
    template<typename T>
    std::optional<T> extract(uWS::HttpRequest *req, const std::string_view &key) {
        std::string_view val = req->getParameter(key);
        if (val.empty()) {
            return std::nullopt;
        }

        T result;
        auto [ptr, ec] = std::from_chars(val.data(), val.data() + val.size(), result);
        if (ec != std::errc()) return std::nullopt;

        return result;
    }

    void onAborted() {
        std::cout << "Client walked away" << std::endl;
    }

    void HttpRoutes::createGame(uWS::HttpResponse<false> *res, uWS::HttpRequest *req) {
        res->onAborted(onAborted);

        const std::string_view username = req->getParameter("username");
        const auto number_of_seats = extract<size_t>(req, "number_of_seats");
        const auto ante = extract<unsigned long>(req, "ante");
        const auto chips_when_seated = extract<unsigned long>(req, "chips_when_seated");

        if (username.empty() || !number_of_seats || !ante || !chips_when_seated) {
            res->writeStatus("400 Bad Request");
            res->end();
            return;
        }

        const core::PokerConfiguration configuration{*number_of_seats, *ante, *chips_when_seated};
        const std::string room_id = core::PokerLobby::instance().createRoom(configuration, std::string(username));

        const auto token = auth::JWTHandler::instance().generateJwt({
            {"room_id", room_id},
            {"username", std::string(username)}
        });

        res->writeHeader("Set-Cookie", "jwt=" + token + "; HttpOnly; Secure; Path=/");
        res->writeStatus("200 OK");
        res->end("Authenticated");
    }

    void HttpRoutes::joinRoom(uWS::HttpResponse<false> *res, uWS::HttpRequest *req) {
        res->onAborted(onAborted);

        const auto username = std::string(req->getParameter("username"));
        const auto room_id = std::string(req->getParameter("room_id"));

        if (username.empty() || room_id.empty()) {
            res->writeStatus("400 Bad Request");
            res->end();
            return;
        }

        const bool success = core::PokerLobby::instance().joinRoom(room_id, username);
        if (!success) {
            res->writeStatus("404 Not Found");
            res->end();
            return;
        }

        const auto token = auth::JWTHandler::instance().generateJwt({
            {"room_id", room_id},
            {"username", username}
        });

        res->writeHeader("Set-Cookie", "jwt=" + token + "; HttpOnly; Secure; SameSite=Strict; Path=/");
        res->writeStatus("200 OK");
        res->end("Authenticated");
    }

    void HttpRoutes::leaveRoom(uWS::HttpResponse<false> *res, uWS::HttpRequest *req) {
        res->onAborted(onAborted);

        const auto auth_context = auth::JWTHandler::instance().extractAuthContextFromCookie(req->getHeader("Cookie"));
        if (!auth_context.has_value()) {
            res->writeStatus("401 Unauthorized");
            res->end();
            return;
        }

        if (core::PokerLobby::instance().leaveRoom(auth_context->room_id, auth_context->username)) {
            res->writeStatus("200 Okay");
            res->end();
        }
        else {
            // Could probably do better error handling here
            res->writeStatus("400 Bad Request");
            res->end();
        }
    }

    void HttpRoutes::upgradeToWs(uWS::HttpResponse<false>* res, uWS::HttpRequest* req, us_socket_context_t* context) {
        auto secKey = req->getHeader("sec-websocket-key");
        if (secKey.empty()) {
            res->writeStatus("400 Bad Request")->end("Not a WS Handshake");
            return;
        }

        const auto auth_context = auth::JWTHandler::instance()
                    .extractAuthContextFromCookie(req->getHeader("Cookie"));
        if (!auth_context.has_value()) {
            res->writeStatus("401 Unauthorized");
            res->end();
            return;
        }

        res->upgrade<auth::AuthContext> (
            {auth_context->room_id, auth_context->username},
            secKey,
            req->getHeader("sec-websocket-protocol"),
            req->getHeader("sec-websocket-extensions"),
            context
        );
    }
}
