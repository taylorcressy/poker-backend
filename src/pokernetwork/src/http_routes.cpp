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

    // TODO: These are all wrong. Need to implement .onData
    void HttpRoutes::createGame(uWS::HttpResponse<false> *res,
                                uWS::HttpRequest *req,
                                core::PokerLobby *lobby,
                                std::shared_ptr<core::events::INotifier> notifier,
                                std::function<void(std::string)> game_created_callback) {
        auto buffer = std::make_shared<std::string>();
        res->onAborted([] {
            onAborted();
        });

        res->onData([buffer, res, lobby, game_created_callback, notifier](const std::string_view chunk, const bool is_last) {
            buffer->append(chunk);

            try {
                if (is_last) {
                    auto json = nlohmann::json::parse(*buffer);

                    if (!json.contains("username") ||
                        !json.contains("number_of_seats") ||
                        !json.contains("ante") ||
                        !json.contains("chips_when_seated")) {
                        res->writeStatus("400 Bad Request");
                        res->end();
                        return;
                    }

                    const std::string username = json.at("username").get<std::string>();
                    const size_t number_of_seats = json.at("number_of_seats").get<size_t>();
                    const size_t ante = json.at("ante").get<unsigned long>();
                    const size_t chips_when_seated = json.at("chips_when_seated").get<unsigned long>();


                    const core::types::PokerConfiguration configuration{number_of_seats, ante, chips_when_seated};
                    std::string room_id = lobby->createRoom(configuration, std::string(username), notifier);

                    const auto token = auth::JWTHandler::generateJwt({
                        {"lobby_id", lobby->lobby_id},
                        {"room_id", room_id},
                        {"username", std::string(username)}
                    });

                    res->writeHeader("Set-Cookie", "jwt=" + token + "; HttpOnly; Secure; Path=/");
                    res->writeStatus("201 CREATED");
                    res->end("Created");

                    buffer->clear();
                    game_created_callback(std::move(room_id));
                }
            } catch (nlohmann::detail::parse_error &err) {
                std::cerr << err.what() << std::endl;
                res->writeStatus("400 BAD REQUEST");
                res->end("Bad Request");
            } catch (...) {
                std::cerr << "Unknown critical error" << std::endl;
                res->writeStatus("500 INTERNAL SERVER ERROR");
                res->end("Internal Server Error");
            }
        });
    }

    void HttpRoutes::joinRoom(uWS::HttpResponse<false> *res,
                              uWS::HttpRequest *req,
                              core::PokerLobby *lobby,
                              uWS::Loop *loop) {
        auto buffer = std::make_shared<std::string>();

        auto aborted = std::make_shared<bool>();
        res->onAborted([aborted] {
            *aborted = true;
            onAborted();
        });

        const auto room_id = std::string(req->getParameter("room_id"));

        res->onData([aborted, buffer, room_id, res, loop, lobby](const std::string_view chunk, const bool is_last) {
            buffer->append(chunk);

            if (is_last) {
                try {
                    auto current_loop = uWS::Loop::get();

                    const auto json = nlohmann::json::parse(*buffer);

                    if (!json.contains("username") || room_id.empty()) {
                        res->writeStatus("400 Bad Request");
                        res->end();
                        return;
                    }
                    const auto username = json["username"].get<std::string>();
                    loop->defer([aborted, res, username, room_id, current_loop, lobby] {
                        // Need to perform game logic on originating loop (dictated by createGame
                        const bool success = lobby->joinRoom(room_id, username);
                        auto lobby_id = lobby->lobby_id; // Pull lobby_id from the owning thread/loop. Shouldn't change, but doing here for correctness
                        current_loop->defer([lobby_id, aborted, res, room_id, username, success] {
                            if (*aborted) {
                                return;
                            }

                            if (!success) {
                                res->writeStatus("404 Not Found");
                                res->end();
                                return;
                            }

                            const auto token = pokergame::network::auth::JWTHandler::generateJwt({
                                {"lobby_id", lobby_id},
                                {"room_id", room_id},
                                {"username", username}
                            });

                            res->writeHeader("Set-Cookie",
                                             "jwt=" + token + "; HttpOnly; Secure; SameSite=Strict; Path=/");
                            res->writeStatus("200 OK");
                            res->end("Authenticated");
                        });
                    });
                } catch (nlohmann::detail::parse_error &err) {
                    std::cerr << err.what() << std::endl;
                    res->writeStatus("400 BAD REQUEST");
                    res->end("Bad Request");
                } catch (...) {
                    std::cerr << "Unknown critical error" << std::endl;
                    res->writeStatus("500 INTERNAL SERVER ERROR");
                    res->end("Internal Server Error");
                }
            }
        });
    }

    void HttpRoutes::leaveRoom(uWS::HttpResponse<false> *res, uWS::HttpRequest *req, core::PokerLobby *lobby, uWS::Loop *loop, std::function<void()> final_player_left_callback) {
        auto aborted = std::make_shared<bool>();
        res->onAborted([aborted] {
            *aborted = true;
            onAborted();
        });

        auto current_loop = uWS::Loop::get();

        const auto auth_context = auth::JWTHandler::instance().extractAuthContextFromCookie(req->getHeader("Cookie"));
        if (!auth_context.has_value()) {
            res->writeStatus("401 Unauthorized");
            res->end();
            return;
        }

        loop->defer([final_player_left_callback, current_loop, lobby, auth_context, res, aborted] {
            auto [player_left, last_player_left] = lobby->leaveRoom(auth_context->room_id, auth_context->username);
            if (last_player_left) {
                final_player_left_callback();
            }
            current_loop->defer([res, player_left, aborted] {
                if (*aborted) {
                    return;
                }
                if (player_left) {
                    res->writeStatus("200 OK");
                    res->end();
                } else {
                    // Could probably do better error handling here
                    res->writeStatus("400 Bad Request");
                    res->end();
                }
            });
        });
    }

    void HttpRoutes::upgradeToWs(uWS::HttpResponse<false> *res, uWS::HttpRequest *req, us_socket_context_t *context) {
        res->onAborted(onAborted);
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

        res->upgrade<auth::AuthContext>(
            {auth_context->lobby_id, auth_context->room_id, auth_context->username},
            secKey,
            req->getHeader("sec-websocket-protocol"),
            req->getHeader("sec-websocket-extensions"),
            context
        );
    }
}
