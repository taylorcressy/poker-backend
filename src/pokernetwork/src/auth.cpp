
#include "detail/auth.h"

#include <jwt-cpp/jwt.h>

namespace pokergame::network::auth {
    JWTHandler::JWTHandler() :
        verifier(jwt::verify()
            .with_issuer("auth0")
            .allow_algorithm(jwt::algorithm::hs256{std::getenv("JWT_SECRET")})) {
    }

    std::string JWTHandler::generateJwt(const std::unordered_map<std::string, std::string> &claims) const {
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
}
