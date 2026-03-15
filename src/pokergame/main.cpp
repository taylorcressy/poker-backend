#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <random>
#include <iostream>

#include "utils/crypto.h"
#include "poker_network.h"
#include "poker_core.h"
#include "utils/string_utils.h"

int load_env(const std::string& path) {
    std::ifstream in_file(path);

    if (!in_file.good()) {
        std::cout << "Environment file not found. Generating new one." << std::endl;

        std::ofstream out_file(path);
        if (!out_file.is_open()) {
            std::cerr << "Could not open "
                << path
                << " for writing. Error code: "
                << std::generic_category().message(errno)
                << std::endl;
            return errno;
        }

        out_file << "JWT_SECRET=" << utils::crypto::generate_unique_random_secret(64) << std::endl;
        out_file.close();

        in_file.clear();
        in_file.open(path);

        if (!in_file.good()) {
            std::cerr << "Could not open "
                << path
                << " for reading. Error code: "
                << std::generic_category().message(errno)
                << std::endl;
        }
    }

    std::string line;
    size_t line_num = 1;
    while (std::getline(in_file, line)) {
        auto tokens = utils::string::split_string(line, '=');

        if (tokens.size() != 2) {
            std::cerr << "Failed to parse env file on line " << line_num << std::endl;
            return -1;
        }

#ifdef __WIN32
        _putenv_s(tokens[0].c_str(), tokens[1].c_str());
#else
        setenv(tokens[0].c_str(), tokens[1].c_str(), 1);
#endif

        line_num++;
    }

    return 0;
}



int main(int argc, const char **argv) {
    const char* env_path = std::getenv("ENV_PATH");
    if (env_path == nullptr) {
        env_path = ".env";
    }
    std::cout << "Using environment path: " << env_path << std::endl;
    int env_res = load_env(env_path);
    if (env_res != 0) {
        return env_res;
    }


    pokergame::network::PokerServer::instance().start();
}
