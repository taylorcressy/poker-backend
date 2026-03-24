#include "detail/ws_routes.h"

#include "App.h"

#include <assert.h>

namespace pokergame::network::ws {

    inline std::string constructUserSocketId(const auth::AuthContext* auth) {
        return auth->room_id + "-" + auth->username;
    }

    inline std::string constructUserSocketId(const std::string& room_id, const std::string& username) {
        return room_id + "-" + username;
    }

    WSRoutes::WSRoutes(uWS::App& app): app(app) {}

    void WSRoutes::playerConnected(uWS::WebSocket<false, true, auth::AuthContext>* ws, const auth::AuthContext* context) {
        const std::string channel = context->room_id + "-broadcast";
        const std::string user_socket_id = constructUserSocketId(context);
        if (!this->table_channels.contains(context->room_id)) {
            this->table_channels.emplace(context->room_id, channel);
        }
        ws->subscribe(channel);

        if (!this->user_sockets.contains(user_socket_id)) {
            this->user_sockets.emplace(user_socket_id, ws);
        }
    }

    void WSRoutes::playerMessageReceived(const auth::AuthContext* context, const std::string_view& message) {
        const std::string socket_id = constructUserSocketId(context);
        if (!this->user_sockets.contains(socket_id)) {
            assert(false); // TODO: Handle
            return;
        }

        // Determine if poker room is expecting a message. If not ignore message.
        // If it is, interrupt the timeout and pass to poker room?
    }

    void WSRoutes::playerDisconnected(const auth::AuthContext* context) {
        const auto socket_id = constructUserSocketId(context);
        if (this->user_sockets.contains(socket_id)) {
            this->user_sockets.erase(socket_id);
        }

        // TODO: Inform game of seat disconnect
    }

    void WSRoutes::sendMessageToPlayer(const std::string &room_id, const std::string &username, core::notifications::Notification* notification) {
        const auto socket_id = constructUserSocketId(room_id, username);
        if (this->user_sockets.contains(socket_id)) {
            this->user_sockets.at(socket_id)->send(notification->dump(), uWS::TEXT);
        }
    }

    void WSRoutes::sendMessageToTable(const std::string &room_id, core::notifications::Notification* notification) {
        const std::string channel = room_id + "-broadcast";
        this->app.publish(channel, notification->dump(), uWS::TEXT);
    }

}
