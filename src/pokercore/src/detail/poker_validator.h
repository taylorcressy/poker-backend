#pragma once
#include <optional>
#include <set>
#include <unordered_map>
#include <unordered_set>

#include "poker_core.h"


namespace pokergame::core {
    enum class Hand {
        HighCard,
        OnePair,
        TwoPair,
        ThreeOfAKind,
        Straight,
        Flush,
        FullHouse,
        FourOfAKind,
        StraightFlush,
        RoyalFlush
    };

    std::string_view handToString(Hand h);

    struct ValidationResult {
        bool valid;
        std::string hand_display_name;
        std::optional<types::Card> high_card = std::nullopt;
        std::optional<std::unordered_set<types::Card, types::CardHasher> > winners = std::nullopt;
        std::optional<std::vector<types::Card> > kickers = std::nullopt;
    };

    class IValidator {
    public:
        IValidator() = default;

        virtual ~IValidator() = default;

        [[nodiscard]] virtual ValidationResult validate(const std::vector<types::Card> &hand) const = 0;
    };

    class RoyalFlushValidator : public IValidator {
    public:
        static RoyalFlushValidator &instance() {
            static RoyalFlushValidator instance;
            return instance;
        }

        [[nodiscard]] ValidationResult validate(const std::vector<types::Card> &hand) const override;

    private:
        RoyalFlushValidator() = default;
    };

    class StraightFlushValidator : public IValidator {
    public:
        static StraightFlushValidator &instance() {
            static StraightFlushValidator instance;
            return instance;
        }

        [[nodiscard]] ValidationResult validate(const std::vector<types::Card> &hand) const override;

    private:
        StraightFlushValidator() = default;
    };

    class FourOfAKindValidator : public IValidator {
    public:
        static FourOfAKindValidator &instance() {
            static FourOfAKindValidator instance;
            return instance;
        }

        [[nodiscard]] ValidationResult validate(const std::vector<types::Card> &hand) const override;

    private:
        FourOfAKindValidator() = default;
    };

    class FullHouseValidator : public IValidator {
    public:
        static FullHouseValidator &instance() {
            static FullHouseValidator instance;
            return instance;
        }

        [[nodiscard]] ValidationResult validate(const std::vector<types::Card> &hand) const override;

    private:
        FullHouseValidator() = default;
    };

    class FlushValidator : public IValidator {
    public:
        static FlushValidator &instance() {
            static FlushValidator instance;
            return instance;
        }

        [[nodiscard]] ValidationResult validate(const std::vector<types::Card> &hand) const override;

    private:
        FlushValidator() = default;
    };

    class StraightValidator : public IValidator {
    public:
        static StraightValidator &instance() {
            static StraightValidator instance;
            return instance;
        }

        [[nodiscard]] ValidationResult validate(const std::vector<types::Card> &hand) const override;

    private:
        StraightValidator() = default;
    };

    class ThreeOfAKindValidator : public IValidator {
    public:
        static ThreeOfAKindValidator &instance() {
            static ThreeOfAKindValidator instance;
            return instance;
        }

        [[nodiscard]] ValidationResult validate(const std::vector<types::Card> &hand) const override;

    private:
        ThreeOfAKindValidator() = default;
    };

    class TwoPairValidator : public IValidator {
    public:
        static TwoPairValidator &instance() {
            static TwoPairValidator instance;
            return instance;
        }

        [[nodiscard]] ValidationResult validate(const std::vector<types::Card> &hand) const override;

    private:
        TwoPairValidator() = default;
    };

    class OnePairValidator : public IValidator {
    public:
        static OnePairValidator &instance() {
            static OnePairValidator instance;
            return instance;
        }

        [[nodiscard]] ValidationResult validate(const std::vector<types::Card> &hand) const override;

    private:
        OnePairValidator() = default;
    };

    class HighCardValidator : public IValidator {
    public:
        static HighCardValidator &instance() {
            static HighCardValidator instance;
            return instance;
        }

        [[nodiscard]] ValidationResult validate(const std::vector<types::Card> &hand) const override;

    private:
        HighCardValidator() = default;
    };

    struct ShowdownResult {
        size_t winner_index = -1;
        ValidationResult result;
    };

    class ShowdownEvaluator {
    public:
        static ShowdownEvaluator &instance() {
            static ShowdownEvaluator validator;
            return validator;
        }

        [[nodiscard]] static std::vector<ShowdownResult> evaluate(const std::unordered_map<size_t, std::vector<types::Card>> &hands) ;

    private:
        ShowdownEvaluator() = default;
    };

    inline const IValidator *validators[] = {
        &RoyalFlushValidator::instance(),
        &StraightFlushValidator::instance(),
        &FourOfAKindValidator::instance(),
        &FullHouseValidator::instance(),
        &FlushValidator::instance(),
        &StraightValidator::instance(),
        &ThreeOfAKindValidator::instance(),
        &TwoPairValidator::instance(),
        &OnePairValidator::instance(),
        &HighCardValidator::instance(),
    };
}
