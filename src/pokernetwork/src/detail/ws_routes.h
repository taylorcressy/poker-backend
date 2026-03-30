#pragma once
#include "App.h"

#include "auth.h"
#include "poker_core.h"
#include "events.h"

namespace pokergame::network::ws {
    class WSRoutes : public core::events::INotifier {
    public:
        explicit WSRoutes(uWS::App &app);

        ~WSRoutes() override = default;

        void playerConnected(uWS::WebSocket<false, true, auth::AuthContext> *, const auth::AuthContext *);

        void playerMessageReceived(const auth::AuthContext *, const std::string_view &);

        void playerDisconnected(const auth::AuthContext *);

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

        // Game id to channel
        std::unordered_map<std::string, std::string> table_channels;
        // GameId-Username to ws
        std::unordered_map<std::string, uWS::WebSocket<false, true, auth::AuthContext> *> user_sockets;
        // GameId to Timer Id counter
        std::unordered_map<std::string, uint8_t> timer_counters;
        // GameId to Timer Id to Timer
        std::unordered_map<std::string, std::unordered_map<uint8_t, us_timer_t *> > running_timers;

        constexpr static uint8_t MaxTimerId = static_cast<uint8_t>(-1);

        uint8_t createCallback(const std::string &room_id,
                               uint8_t timeout_in_seconds,
                               std::function<void()> timeout_callback);
    };
}
