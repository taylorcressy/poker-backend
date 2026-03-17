SET(POKER_NETWORK_INCLUDE
        include/poker_network.h
)

SET(POKER_NETWORK_PRIVATE_HEADERS
        src/detail/http_routes.h
        src/detail/ws_routes.h
        src/detail/auth.h
)

SET(POKER_NETWORK_SRC
        src/http_routes.cpp
        src/server.cpp
        src/auth.cpp
        src/ws_routes.cpp
)

add_library(pokernetwork STATIC
        ${POKER_NETWORK_INCLUDE}
        ${POKER_NETWORK_PRIVATE_HEADERS}
        ${POKER_NETWORK_SRC}
)

target_include_directories(pokernetwork
        PUBLIC
        include
        PRIVATE
        src
        ${CMAKE_SOURCE_DIR}/src

        ${uwebsockets_SOURCE_DIR}/src
        ${uwebsockets_SOURCE_DIR}/uSockets/src
)

target_link_libraries(pokernetwork
        PRIVATE
        pokercore
        uWebSockets
        jwt-cpp::jwt-cpp
)

