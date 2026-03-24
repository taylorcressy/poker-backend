
#include "detail/auth.h"

#include <jwt-cpp/jwt.h>

#include "utils/string_utils.h"

namespace pokergame::network::auth {
    JWTHandler::JWTHandler() :
        verifier(jwt::verify()
            .with_issuer("auth0")
            .allow_algorithm(jwt::algorithm::hs256{std::getenv("JWT_SECRET")})) {
    }

    std::string JWTHandler::generateJwt(const std::unordered_map<std::string, std::string> &claims) {
        auto builder = jwt::create().set_type("JWS").set_issuer("auth0");

        for (const auto& [claim, claim_value]: claims) {
            builder.set_payload_claim(claim, jwt::claim(claim_value));
        }

        return builder.sign(jwt::algorithm::hs256{std::getenv("JWT_SECRET")});
    }

    JWTExtract JWTHandler::decodeAndVerify(const std::string& token, const std::vector<std::string>& claims) const {
        std::unordered_map<std::string, std::string> extracted;
        const auto decoded = jwt::decode(token);
        try {
            verifier.verify(decoded);
        } catch (const std::exception& e) {
            std::cerr << "JWT Verification Failed: " << e.what() << std::endl;
            return {false, std::nullopt};
        }

        for (const auto& claim: claims) {
            extracted[claim] = decoded.get_payload_claim(claim).as_string();
        }

        return {true, extracted};
    }

    std::optional<AuthContext> JWTHandler::extractAuthContextFromCookie(const std::string_view &cookie_header) {
        const std::vector<std::string_view> cookies = utils::string::split_string_view(cookie_header, ';');
        if (cookies.empty()) {
            return std::nullopt;
        }

        for (const auto &cookie: cookies) {
            const auto trimmed = utils::string::trim_string_view(cookie);
            if (!trimmed.empty()) {
                const auto split_cookie = utils::string::split_string_view(trimmed, '=');
                if (split_cookie.size() != 2) {
                    continue;
                }

                if (split_cookie[0] == "jwt") {
                    const auto extracted = instance()
                            .decodeAndVerify(
                                std::string(split_cookie[1]),
                                {"lobby_id", "room_id", "username"}
                            );

                    if (!extracted.succeeded || !extracted.extracted.has_value()) {
                        std::cerr << "Received invalid jwt." << std::endl;
                        return std::nullopt;
                    }

                    auto claim_map = *extracted.extracted;
                    return AuthContext{ claim_map["lobby_id"], claim_map["room_id"], claim_map["username"] };
                }
            }
        }

        std::cout << "Failed to extract jwt from cookie header." << std::endl;
        return std::nullopt;
    }
}
