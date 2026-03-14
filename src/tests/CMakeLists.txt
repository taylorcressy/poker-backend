SET(POKER_TEST_PRIVATE_HEADERS
        src/detail/tests.h
)

SET(POKER_TEST_SRC
        src/test_poker_core.cpp
        src/test_auth.cpp
        src/test_main.cpp
)

add_executable(
        pokertests
        ${POKER_TEST_SRC}
)

target_include_directories(pokertests
        PRIVATE
        ${CMAKE_SOURCE_DIR}/src/pokernetwork/src/detail
)

target_link_libraries(
        pokertests
        PRIVATE
        pokernetwork
        pokercore
        jwt-cpp::jwt-cpp
)
