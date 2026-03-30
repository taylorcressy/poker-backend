SET(POKERCORE_INCLUDE
        include/poker_types.h
        include/poker_core.h
        include/events.h
)

SET(POKERCORE_PRIVATE_HEADERS
        src/detail/poker_validator.h
)

SET(POKERCORE_SRC
        src/poker_deck.cpp
        src/poker_game.cpp
        src/poker_validator.cpp
        src/poker_lobby.cpp
        src/events.cpp
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
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(pokercore
    PUBLIC
        nlohmann_json::nlohmann_json
)