
#include "detail/tests.h"

int main(int argc, char **argv) {
    pokergame::tests::PokerTests().run();
    pokergame::tests::AuthTests().run();
}
