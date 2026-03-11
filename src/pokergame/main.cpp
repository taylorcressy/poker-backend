#include <vector>
#include <iostream>

#include "poker_core.h"

using namespace pokergame;

int main(int argc, const char **argv) {
    PokerGame game{
        {
            .number_of_seats = 5,
            .ante = 5,
            .minimum_number_of_chips_to_start = 100
        }
    };
    game.start();
}
