#pragma once

#include <string>
#include <jwt-cpp/jwt.h>

namespace pokergame::network::auth {
    struct JWTExtract {
        bool succeeded;
        std::optional<std::unordered_map<std::string, std::string>> extracted;
    };

    struct AuthContext {
        std::string lobby_id;
        std::string room_id;
        std::string username;
    };

    class JWTHandler {
    public:
        ~JWTHandler() = default;
        static JWTHandler& instance() {
            static JWTHandler handler;
            return handler;
        }

        [[nodiscard]] static std::string generateJwt(const std::unordered_map<std::string, std::string>& claims) ;
        [[nodiscard]] static std::optional<AuthContext> extractAuthContextFromCookie(const std::string_view &cookie_header);

    private:
        [[nodiscard]] JWTExtract decodeAndVerify(const std::string& token, const std::vector<std::string>& claims) const;

        JWTHandler();
        jwt::verifier<jwt::default_clock, jwt::traits::kazuho_picojson> verifier;
    };
};