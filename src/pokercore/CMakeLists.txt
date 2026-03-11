SET(POKERCORE_INCLUDE
        include/poker_core.h
)

SET(POKERCORE_PRIVATE_HEADERS
        src/detail/poker_validator.h
)

SET(POKERCORE_SRC
        src/poker_deck.cpp src/poker_game.cpp
        src/poker_validator.cpp
)

add_library(pokercore STATIC
        ${POKERCORE_INCLUDE}
        ${POKERCORE_PRIVATE_HEADERS}
        ${POKERCORE_SRC}
)

target_include_directories(pokercore
        PUBLIC
        include
        PRIVATE
        src
        src/include
)