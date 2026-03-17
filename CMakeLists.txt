cmake_minimum_required(VERSION 4.1.2)
project(Poker)

set(CMAKE_CXX_STANDARD 20)

include(FetchContent)

## uWebSockets
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)

FetchContent_Declare(
        uWebSockets
        GIT_REPOSITORY https://github.com/uNetworking/uWebSockets.git
        GIT_TAG v20.75.0
)

FetchContent_MakeAvailable(uWebSockets)

add_library(uWebSockets INTERFACE)

target_include_directories(uWebSockets INTERFACE
        ${uwebsockets_SOURCE_DIR}/src
        ${uwebsockets_SOURCE_DIR}/uSockets/src
)

## uSocket
file(GLOB_RECURSE USOCKET_SOURCES
        ${uwebsockets_SOURCE_DIR}/uSockets/src/*.c
        ${uwebsockets_SOURCE_DIR}/uSockets/src/*.cpp
)
add_library(uSockets STATIC ${USOCKET_SOURCES})
target_include_directories(uSockets PRIVATE "${uwebsockets_SOURCE_DIR}/uSockets/src")

target_compile_definitions(uSockets PRIVATE LIBUS_USE_EPOLL LIBUS_USE_OPENSSL)
target_link_libraries(uSockets PRIVATE OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB)

target_link_libraries(uWebSockets INTERFACE uSockets)

## jwt-cpp
FetchContent_Declare(
        jwt-cpp
        GIT_REPOSITORY https://github.com/Thalhammer/jwt-cpp.git
        GIT_TAG v0.7.2
)
set(JWT_BUILD_EXAMPLES OFF CACHE BOOL "disable building examples" FORCE)
FetchContent_MakeAvailable(jwt-cpp)

## nlohman json
FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.12.0
)
FetchContent_MakeAvailable(nlohmann_json)

add_subdirectory(src)
