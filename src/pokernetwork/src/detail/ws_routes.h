#pragma once
#include "App.h"

#include "auth.h"
#include "poker_core.h"
#include "notifications.h"

namespace pokergame::network::ws {

class WSRoutes : public core::notifications::INotifier {
public:
    explicit WSRoutes(uWS::App& app);
    ~WSRoutes() override = default;

    void playerConnected(uWS::WebSocket<false, true, auth::AuthContext>*, const auth::AuthContext*);
    void playerMessageReceived(const auth::AuthContext*, const std::string_view&);
    void playerDisconnected(const auth::AuthContext*);

    void sendMessageToPlayer(const std::string &room_id, const std::string &username, core::notifications::Notification*) override;
    void sendMessageToTable(const std::string &room_id, core::notifications::Notification*) override;

private:
    uWS::App& app;

    // Game id to channel
    std::unordered_map<std::string, std::string> table_channels;
    // GameId-Username to ws
    std::unordered_map<std::string, uWS::WebSocket<false, true, auth::AuthContext>*> user_sockets;
};

}
