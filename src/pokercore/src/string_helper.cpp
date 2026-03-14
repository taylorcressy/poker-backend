
#include "poker_core.h"
#include <sstream>

namespace pokergame::core {
    std::vector<std::string> split_string(const std::string& str, const char delimiter) {
        std::istringstream stream(str);
        std::vector<std::string> tokens;
        std::string token;

        while (std::getline(stream, token, delimiter)) {
            tokens.push_back(token);
        }

        return tokens;
    }
}