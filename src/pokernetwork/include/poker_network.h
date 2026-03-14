
#pragma once

namespace pokergame::network {

    class PokerServer {
    public:
        static PokerServer instance() {
            static PokerServer server;
            return server;
        }

        ~PokerServer() = default;

        void start();
    private:
        PokerServer() = default;
    };

}