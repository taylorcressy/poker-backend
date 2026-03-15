#pragma once

#include <random>
#include <string>

namespace utils::crypto {
    inline std::string generate_unique_random_secret(const size_t secret_length) {
        const std::string charset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
        std::random_device rd;
        std::mt19937 generator(rd());
        std::uniform_int_distribution<> dist(0, charset.size() - 1);

        std::string secret;
        for (size_t i = 0; i < secret_length; i++) {
            secret += charset[dist(generator)];
        }

        return secret;
    }
}