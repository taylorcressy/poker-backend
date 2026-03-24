#include "detail/tests.h"

namespace pokergame::tests {


// ---------------------------------------------------------------------------
// JWT tests
// ---------------------------------------------------------------------------

void AuthTests::testJwt() {
    std::cout << "=== JWT tests ===\n";

    auto &handler = network::auth::JWTHandler::instance();

    // JWT1: round-trip – generate a token with multiple claims, decode and verify.
    {
        const std::unordered_map<std::string, std::string> claims = {
            {"test", "1"},
            {"test2", "2"}
        };
        const std::vector<std::string> claims_to_pull = {"test", "test2"};
        const auto token = handler.generateJwt(claims);
        const auto result = handler.decodeAndVerify(token, claims_to_pull);

        assert(result.succeeded);
        assert(result.extracted.has_value());
        assert(result.extracted->at("test") == "1");
        assert(result.extracted->at("test2") == "2");
    }

    // JWT2: single-claim round-trip.
    {
        const std::unordered_map<std::string, std::string> claims = {{"user", "alice"}};
        const auto token = handler.generateJwt(claims);
        const auto result = handler.decodeAndVerify(token, {"user"});

        assert(result.succeeded);
        assert(result.extracted.has_value());
        assert(result.extracted->at("user") == "alice");
    }

    std::cout << "  JWT: all 2 tests passed\n";
}

void AuthTests::run() {
    setenv("JWT_SECRET", "test_secret", 1);
    testJwt();
    std::cout << "All auth tests passed!\n";
}

} // namespace pokergame::tests
