#pragma once
#include "App.h"

#include "auth.h"
#include "poker_core.h"
#include "poker_notifications.h"

namespace pokergame::network::ws {

    // TODO: You are here. Implement....
class WSRoutes : public core::notifications::INotifier {
public:
    static WSRoutes& instance() {
        static WSRoutes routes;
        return routes;
    }

    void playerConnected();
    void playerMessageReceived();
    void playerDisconnected();

    void sendMessageToPlayer(const std::string &room_id, const std::string &username, core::notifications::Notification*) override;
    void sendMessageToTable(const std::string &room_id, core::notifications::Notification*) override;

private:
    WSRoutes();
    ~WSRoutes() override = default;

    std::unordered_map<std::string, std::string> table_channels;
    std::unordered_map<std::string, uWS::WebSocket<false, true, auth::AuthContext>*> user_sockets;
};

}
