#include "detail/ws_routes.h"

#include "App.h"

// TODO: These player messages need an ack process.
// TODO: Clear from state (table channel / timer stuff) when games disconnect
namespace pokergame::network::ws {

    // Static member definitions
    std::unordered_map<std::string, UserSocket> WSRoutes::s_user_sockets;
    std::unordered_map<std::string, std::unordered_map<uWS::Loop *, int>> WSRoutes::s_room_loops;
    std::shared_mutex WSRoutes::s_sockets_mutex;
    std::unordered_map<uWS::Loop *, uWS::App *> WSRoutes::s_loop_to_app;
    std::shared_mutex WSRoutes::s_loop_to_app_mutex;

    inline std::string constructUserSocketId(const auth::AuthContext *auth) {
        return auth->room_id + "-" + auth->username;
    }

    inline std::string constructUserSocketId(const std::string &room_id, const std::string &username) {
        return room_id + "-" + username;
    }

    WSRoutes::WSRoutes(uWS::App &app) : app(app) {
    }

    void WSRoutes::registerThreadApp(uWS::App *app, uWS::Loop *loop) {
        std::lock_guard lock(s_loop_to_app_mutex);
        s_loop_to_app.emplace(loop, app);
    }

    void WSRoutes::registerSocket(const std::string &socket_id,
                                   const std::string &room_id,
                                   uWS::WebSocket<false, true, auth::AuthContext> *ws,
                                   uWS::Loop *loop) {
        std::lock_guard lock(s_sockets_mutex);
        s_user_sockets.insert_or_assign(socket_id, UserSocket{ws, loop, room_id});
        s_room_loops[room_id][loop]++;
    }

    void WSRoutes::unregisterSocket(const std::string &socket_id) {
        std::lock_guard lock(s_sockets_mutex);
        const auto it = s_user_sockets.find(socket_id);
        if (it == s_user_sockets.end()) return;

        const std::string &room_id = it->second.room_id;
        uWS::Loop *loop = it->second.loop;
        s_user_sockets.erase(it);

        auto room_it = s_room_loops.find(room_id);
        if (room_it != s_room_loops.end()) {
            auto loop_it = room_it->second.find(loop);
            if (loop_it != room_it->second.end() && --loop_it->second == 0) {
                room_it->second.erase(loop_it);
                if (room_it->second.empty()) {
                    s_room_loops.erase(room_it);
                }
            }
        }
    }

    void WSRoutes::playerConnected(uWS::WebSocket<false, true, auth::AuthContext> *ws,
                                   const auth::AuthContext *context) {
        const std::string channel = context->room_id + "-broadcast";
        if (!this->table_channels.contains(context->room_id)) {
            this->table_channels.emplace(context->room_id, channel);
        }
        ws->subscribe(channel);
    }

    void WSRoutes::playerDisconnected(const auth::AuthContext *context) {
        // TODO: Inform game of seat disconnect
    }

    void WSRoutes::sendMessageToPlayer(const std::string &room_id, const std::string &username,
                                       core::events::Notification *notification) {
        const auto socket_id = constructUserSocketId(room_id, username);
        const std::string msg = notification->dump();

        std::shared_lock lock(s_sockets_mutex);
        const auto it = s_user_sockets.find(socket_id);
        if (it == s_user_sockets.end()) return;
        auto *target_loop = it->second.loop;
        auto *target_ws = it->second.ws;
        lock.unlock();

        target_loop->defer([target_ws, msg] {
            target_ws->send(msg, uWS::TEXT);
        });
    }

    struct TimerData {
        uint8_t timer_id{};
        std::string room_id;
        std::function<void()> timeout_callback;
        std::unordered_map<std::string, std::unordered_map<uint8_t, us_timer_t *> > *running_timers{};
    };

    uint8_t WSRoutes::createCallback(const std::string &room_id,
                                     const uint8_t timeout_in_seconds,
                                     std::function<void()> timeout_callback) {
        auto *loop = reinterpret_cast<us_loop_t *>(uWS::Loop::get());

        const auto iter = timer_counters.find(room_id);
        uint8_t timer_id = 0;
        if (iter == timer_counters.end()) {
            timer_counters.insert_or_assign(room_id, 0);
        } else {
            timer_id = iter->second;
        }

        if (timer_id == MaxTimerId) {
            timer_id = 0;
        }

        timer_counters.insert_or_assign(room_id, ++timer_id);

        us_timer_t *timer = us_create_timer(loop, 0, sizeof(TimerData));

        auto *timer_data = new(us_timer_ext(timer)) TimerData();

        timer_data->timer_id = timer_id;
        timer_data->room_id = room_id;
        timer_data->timeout_callback = std::move(timeout_callback);
        timer_data->running_timers = &this->running_timers;

        const auto timer_iter = this->running_timers.find(room_id);
        if (timer_iter == this->running_timers.end()) {
            std::unordered_map<uint8_t, us_timer_t *> timer_map;
            timer_map.insert_or_assign(timer_id, timer);
            running_timers.insert_or_assign(room_id, timer_map);
        } else {
            timer_iter->second.insert_or_assign(timer_id, timer);
        }

        us_timer_set(timer, [](us_timer_t *t) {
            const auto *data = static_cast<TimerData *>(us_timer_ext(t));
            auto *global_map = data->running_timers;
            const std::string rid = data->room_id;
            const uint8_t tid = data->timer_id;

            data->timeout_callback();

            auto room_it = global_map->find(rid);
            if (room_it != global_map->end()) {
                auto timer_it = room_it->second.find(tid);
                if (timer_it != room_it->second.end()) {
                    // Only destroy if it hasn't been destroyed by cancelCallback already
                    data->~TimerData();
                    us_timer_close(t);
                    room_it->second.erase(timer_it);

                    if (room_it->second.empty()) {
                        global_map->erase(room_it);
                    }
                }
            }
        }, timeout_in_seconds * 1000, 0);

        return timer_id;
    }


    uint8_t WSRoutes::sendMessageToPlayerWithTimeoutCallback(const std::string &room_id,
                                                             const std::string &player_id,
                                                             core::events::Notification *notification,
                                                             uint8_t timeout_in_seconds,
                                                             std::function<void()> timeout_callback) {
        this->sendMessageToPlayer(room_id, player_id, notification);
        return createCallback(room_id, timeout_in_seconds, std::move(timeout_callback));
    }

    void WSRoutes::cancelCallback(const std::string &room_id, const uint8_t timer_id) {
        const auto room_iter = this->running_timers.find(room_id);
        if (room_iter != this->running_timers.end()) {
            const auto timer_iter = room_iter->second.find(timer_id);
            if (timer_iter != room_iter->second.end()) {
                const auto *data = static_cast<TimerData *>(us_timer_ext(timer_iter->second));
                data->~TimerData();
                us_timer_close(timer_iter->second);
                room_iter->second.erase(timer_id);
                if (room_iter->second.empty()) {
                    this->running_timers.erase(room_iter);
                }
            }
        }
    }

    void WSRoutes::sendMessageToTable(const std::string &room_id, core::events::Notification *notification) {
        const std::string channel = room_id + "-broadcast";
        const std::string msg = notification->dump();

        std::shared_lock sockets_lock(s_sockets_mutex);
        const auto room_it = s_room_loops.find(room_id);
        if (room_it == s_room_loops.end()) return;

        std::shared_lock apps_lock(s_loop_to_app_mutex);
        for (const auto &[loop, _] : room_it->second) {
            const auto app_it = s_loop_to_app.find(loop);
            if (app_it == s_loop_to_app.end()) continue;
            auto *app = app_it->second;
            loop->defer([app, channel, msg] {
                app->publish(channel, msg, uWS::TEXT);
            });
        }
    }

    uint8_t WSRoutes::sendMessageToTableWithTimeoutCallback(const std::string &room_id,
                                                            core::events::Notification *notification,
                                                            uint8_t timeout_in_seconds,
                                                            std::function<void()> callback) {
        sendMessageToTable(room_id, notification);
        return createCallback(room_id, timeout_in_seconds, std::move(callback));
    }
}
