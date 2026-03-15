add_executable(pokergame main.cpp)

target_include_directories(pokergame
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(
        pokergame
        PRIVATE
        pokercore
        pokernetwork
)