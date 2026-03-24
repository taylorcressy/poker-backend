//
// Created by tcressy on 3/13/26.
//

#include "App.h"
#include "poker_network.h"
#include "detail/http_routes.h"

#include <functional>
#include <iostream>
#include <shared_mutex>

#include "detail/auth.h"
#include "detail/ws_routes.h"

namespace pokergame::network {
    struct LobbyLoop {
        core::PokerLobby lobby;
        uWS::Loop* loop;
    };

    // Thread to lobby & loop mapping
    static std::unordered_map<size_t, LobbyLoop> lobby_loops;
    static std::unordered_map<std::string, size_t> room_id_to_thread;
    static std::shared_mutex map_mutex;

    void PokerServer::start() const {
        static int port = 9001;
        const size_t number_of_cpu_threads = std::thread::hardware_concurrency();
        std::vector<std::jthread> threads;
        threads.reserve(number_of_cpu_threads);
        lobby_loops.reserve(number_of_cpu_threads);

        for (size_t i = 0; i < number_of_cpu_threads; i++) {
            threads.emplace_back([i] {
                auto app = uWS::App();
                auto ws_routes = std::make_shared<ws::WSRoutes>(ws::WSRoutes(app));

                {
                    std::lock_guard map_guard(map_mutex);
                    lobby_loops.emplace(i, LobbyLoop{core::PokerLobby{std::to_string(i)}, app.getLoop()});
                }

                const auto get_lobby_loop_thread_safe = [](uWS::HttpResponse<false> *res, uWS::HttpRequest *req, auto callback) {
                    const auto room_id = std::string(req->getParameter("room_id"));

                    if (room_id.empty()) {
                        res->writeStatus("400 Bad Request");
                        res->end();
                        return;
                    }

                    LobbyLoop *lobby_loop;
                    {
                        std::shared_lock map_guard(map_mutex);
                        const auto it = room_id_to_thread.find(room_id);
                        if (it == room_id_to_thread.end()) {
                            res->writeStatus("404 Bad Request");
                            res->end();
                            return;
                        }

                        const size_t thread_id = it->second;
                        lobby_loop = &lobby_loops.at(thread_id);
                    }

                    callback(lobby_loop, room_id);
                };

                app.post("/v1/poker/createGame", [i, ws_routes](auto *res, auto *req) {
                    // TODO: It's theoretically possible to have conflicting room_ids across these threads. Need to fix.
                    std::lock_guard map_guard(map_mutex);
                    http::HttpRoutes::instance().createGame(res, req, &lobby_loops.at(i).lobby, ws_routes, [i](std::string room_id) {
                        std::lock_guard internal_map_guard(map_mutex);
                        room_id_to_thread.emplace(std::move(room_id), i);
                    });
                });
                app.post("/v1/poker/room/:room_id/join", [get_lobby_loop_thread_safe](auto *res, auto *req) {
                    get_lobby_loop_thread_safe(res, req, [res, req](LobbyLoop* lobby_loop, const std::string& room_id) {
                        if (lobby_loop != nullptr) {
                            http::HttpRoutes::instance().joinRoom(res, req, &lobby_loop->lobby, lobby_loop->loop);
                        }
                    });
                });
                app.post("/v1/poker/room/:room_id/leave", [get_lobby_loop_thread_safe](auto *res, auto *req) {
                    get_lobby_loop_thread_safe(res, req, [res, req](LobbyLoop* lobby_loop, const std::string &room_id) {
                        if (lobby_loop != nullptr) {
                            http::HttpRoutes::instance().leaveRoom(res, req, &lobby_loop->lobby, lobby_loop->loop, [room_id] {
                                std::lock_guard internal_map_guard(map_mutex);
                                room_id_to_thread.erase(room_id);
                            });
                        }
                    });
                });

                app.ws<auth::AuthContext>("/*", {
                                              .compression = uWS::SHARED_COMPRESSOR,
                                              .maxPayloadLength = 16 * 1024,
                                              .idleTimeout = 16,
                                              .upgrade = [](uWS::HttpResponse<false> *res, uWS::HttpRequest *req,
                                                            us_socket_context_t *context) {
                                                  http::HttpRoutes::instance().upgradeToWs(res, req, context);
                                              },
                                              .open = [ws_routes](uWS::WebSocket<false, true, auth::AuthContext> *ws) {
                                                  std::cout << "Player [" << ws->getUserData()->username <<
                                                          "] connected to room [" << ws->getUserData()->room_id << "]" <<
                                                          std::endl;
                                                  ws_routes->playerConnected(ws, ws->getUserData());
                                              },
                                              .message = [](uWS::WebSocket<false, true, auth::AuthContext> *ws,
                                                            const std::string_view message, const uWS::OpCode op_code) {
                                                  if (op_code != uWS::TEXT) {
                                                      std::cout << "Received non text message from client: " << ws->
                                                              getUserData()->room_id << "-" << ws->getUserData()->username <<
                                                              std::endl;
                                                      return;
                                                  }

                                                  // TODO: Message handling
                                              },
                                              .close = [ws_routes](uWS::WebSocket<false, true, auth::AuthContext> *ws, int code,
                                                                   std::string_view message) {
                                                  std::cout << "Player [" << ws->getUserData()->username <<
                                                          "] disconnected from room [" << ws->getUserData()->room_id << "]" <<
                                                          std::endl;
                                                  ws_routes->playerDisconnected(ws->getUserData());
                                              },
                                          });

                app.listen(port, [](const auto *listen_socket) {
                    if (listen_socket) {
                        std::cout << "Listening on port " << port << std::endl;
                    } else {
                        std::cout << "Failed to listen to port " << port << std::endl;
                    }
                });

                app.run();
            });
        }

        for (auto &t: threads) {
            t.join();
        }
    }
}