#pragma once

#include <string>
#include <jwt-cpp/jwt.h>

namespace pokergame::network::auth {
    struct JWTExtract {
        bool succeeded;
        std::optional<std::unordered_map<std::string, std::string>> extracted;
    };

    class JWTHandler {
    public:
        ~JWTHandler() = default;
        static JWTHandler& instance() {
            static JWTHandler handler;
            return handler;
        }

        [[nodiscard]] std::string generateJwt(const std::unordered_map<std::string, std::string>& claims) const;
        [[nodiscard]] JWTExtract decodeAndVerify(const std::string& token, const std::vector<std::string>& claims) const;

    private:
        JWTHandler();
        jwt::verifier<jwt::default_clock, jwt::traits::kazuho_picojson> verifier;
    };
};