#pragma once
#include "App.h"

#include <shared_mutex>

#include "auth.h"
#include "poker_core.h"
#include "events.h"

namespace pokergame::network::ws {

    struct UserSocket {
        uWS::WebSocket<false, true, auth::AuthContext> *ws;
        uWS::Loop *loop;
    };

    class WSRoutes : public core::events::INotifier {
    public:
        explicit WSRoutes(uWS::App &app);

        ~WSRoutes() override = default;

        void playerConnected(uWS::WebSocket<false, true, auth::AuthContext> *, const auth::AuthContext *);

        void playerDisconnected(const auth::AuthContext *);

        // Called by server.cpp's open/close handlers to maintain the global socket registry
        static void registerSocket(const std::string &socket_id,
                                   uWS::WebSocket<false, true, auth::AuthContext> *ws,
                                   uWS::Loop *loop);
        static void unregisterSocket(const std::string &socket_id);

        void sendMessageToPlayer(const std::string &room_id, const std::string &username,
                                 core::events::Notification *) override;

        uint8_t sendMessageToPlayerWithTimeoutCallback(const std::string &room_id, const std::string &player_id,
                                                       core::events::Notification *, uint8_t,
                                                       std::function<void()>) override;

        void cancelCallback(const std::string &, uint8_t) override;

        void sendMessageToTable(const std::string &room_id, core::events::Notification *) override;

        uint8_t sendMessageToTableWithTimeoutCallback(const std::string &room_id, core::events::Notification *, uint8_t,
                                                      std::function<void()>) override;

    private:
        uWS::App &app;

        // Per-thread: room id to pub/sub channel name
        std::unordered_map<std::string, std::string> table_channels;
        // Per-thread timer state
        std::unordered_map<std::string, uint8_t> timer_counters;
        std::unordered_map<std::string, std::unordered_map<uint8_t, us_timer_t *>> running_timers;

        constexpr static uint8_t MaxTimerId = static_cast<uint8_t>(-1);

        uint8_t createCallback(const std::string &room_id,
                               uint8_t timeout_in_seconds,
                               std::function<void()> timeout_callback);

        // Global socket registry — one entry per connected player across all threads
        static std::unordered_map<std::string, UserSocket> s_user_sockets;
        static std::shared_mutex s_sockets_mutex;
    };
}
