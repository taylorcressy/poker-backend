#include <vector>
#include <iostream>

#include "poker_network.h"
#include "poker_core.h"

int main(int argc, const char **argv) {
    pokergame::network::PokerServer::instance().start();
}
