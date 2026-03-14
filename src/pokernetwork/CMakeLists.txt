SET(POKER_NETWORK_INCLUDE
        include/poker_network.h
)

#SET(POKER_NETWORK_PRIVATE_HEADERS
#    src/detail/
#)

SET(POKER_NETWORK_SRC
        src/server.cpp
)

add_library(pokernetwork STATIC
        ${POKER_NETWORK_INCLUDE}
        ${POKER_NETWORK_SRC}
)

target_include_directories(pokernetwork
        PUBLIC
        include
        PRIVATE
        src
        src/detail
)

target_link_libraries(pokernetwork
    PRIVATE
        uWebSockets
)

