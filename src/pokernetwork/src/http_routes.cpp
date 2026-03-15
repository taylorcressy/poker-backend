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

    struct AuthContext {
        std::string room_id;
        std::string username;
    };

    // TODO: Move somewhere else.
    std::optional<AuthContext> extractAuthContextFromCookie(const std::string_view &cookie_header) {
        const std::vector<std::string_view> cookies = utils::string::split_string_view(cookie_header, ';');
        if (cookies.empty()) {
            return std::nullopt;
        }

        for (const auto &cookie: cookies) {
            const auto trimmed = utils::string::trim_string_view(cookie);
            if (trimmed.size() > 0) {
                const auto split_cookie = utils::string::split_string_view(trimmed, '=');
                if (split_cookie.size() != 2) {
                    continue;
                }

                if (split_cookie[0] == "jwt") {
                    const auto extracted = auth::JWTHandler::instance()
                            .decodeAndVerify(
                                std::string(split_cookie[1]),
                                {"room_id", "username"}
                            );

                    if (!extracted.succeeded || !extracted.extracted.has_value()) {
                        std::cerr << "Received invalid jwt." << std::endl;
                        return std::nullopt;
                    }

                    auto claim_map = *extracted.extracted;
                    return AuthContext{ claim_map["room_id"], claim_map["username"] };
                }
            }
        }

        std::cout << "Failed to extract jwt from cookie header." << std::endl;
        return std::nullopt;
    }

    void HttpRoutes::createGame(uWS::HttpResponse<false> *res, uWS::HttpRequest *req) {
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

        res->writeHeader("Set-Cookie", "jwt=" + token + "; HttpOnly; Secure; SameSite=Strict; Path=/");
        res->writeStatus("200 OK");
        res->end("Authenticated");
    }

    void HttpRoutes::joinRoom(uWS::HttpResponse<false> *res, uWS::HttpRequest *req) {
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
        const auto auth_context = extractAuthContextFromCookie(req->getHeader("Cookie"));
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
}
