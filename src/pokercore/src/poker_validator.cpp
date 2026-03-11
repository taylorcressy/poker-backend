#include "detail/poker_validator.h"

#include <algorithm>
#include <limits>
#include <optional>
#include <ranges>
#include <unordered_set>
#include <unordered_map>
#include <map>

namespace pokergame::detail {
    /////
    // Helpers
    ////
    bool cardComparatorDescending(const Card &a, const Card &b) {
        return a.rank > b.rank;
    }

    struct StraightResult {
        const bool is_straight;
        const std::optional<Card> high_card;
        const std::optional<std::unordered_set<Card, CardHasher> > winners;
    };

    std::map<Rank, std::vector<Card>, RankSorterDescending> group_by_rank(const std::vector<Card> &cards) {
        std::map<Rank, std::vector<Card>, RankSorterDescending> result;
        for (const auto &card: cards) {
            result[card.rank].push_back(card);
        }

        return result;
    }

    ValidationResult multiple_card_validator(const std::vector<Card> &hand, const size_t size, const char *name) {
        auto grouped_hand = group_by_rank(hand);
        size_t expected_kickers = 5 - size;
        size_t found_kickers = 0;
        std::vector<Card> kickers;
        ValidationResult result(false, name);

        for (const auto &cards: grouped_hand | std::views::values) {
            if (cards.size() >= size && !result.valid) {
                std::unordered_set<Card, CardHasher> winners;
                winners.reserve(size);
                for (size_t i = 0; i < size; i++) {
                    winners.insert(cards[i]);
                }
                result.valid = true;
                result.high_card = cards[0];
                result.winners = winners;
            } else if (found_kickers < expected_kickers) {
                for (const auto &kicker: cards) {
                    if (found_kickers < expected_kickers) {
                        kickers.push_back(kicker);
                        found_kickers += 1;
                    }
                }
            }
        }

        if (result.valid) {
            result.kickers = kickers;
        }

        return result;
    }

    StraightResult find_straight(std::vector<Card> cards) {
        std::ranges::sort(cards.begin(), cards.end(), cardComparatorDescending);
        std::unordered_set<Card, CardHasher> winners;

        Card high_card = cards[0];
        Card last_card = cards[0];
        winners.insert(cards[0]);

        int running_streak = 1;
        bool has_ace = high_card.rank == Rank::Ace;

        for (size_t i = 1; i < cards.size(); i++) {
            const int last_rank = rankIndex(last_card.rank);
            const int current_rank = rankIndex(cards[i].rank);
            if (current_rank == last_rank) {
                continue;
            }

            if (last_rank - 1 == current_rank) {
                running_streak += 1;
                winners.insert(cards[i]);
            } else {
                // Straight broken
                running_streak = 1;
                high_card = cards[i];
                winners.clear();
                winners.insert(cards[i]);
            }
            if (running_streak == 5) {
                return {
                    true, high_card, winners
                };
            }
            if (running_streak == 4 && cards[i].rank == Rank::Two && has_ace) {
                // Low-Ace straight
                winners.insert(cards[0]); // First card has to be an Ace here
                return {
                    true, high_card, winners
                };
            }
            last_card = cards[i];
        }

        return {
            false
        };
    }

    //////
    // Validators
    ///
    ValidationResult RoyalFlushValidator::validate(const std::vector<Card> &hand) const {
        const auto straight_flush_result = StraightFlushValidator::instance().validate(hand);
        if (straight_flush_result.valid && straight_flush_result.high_card.value().rank == Rank::Ace) {
            return {
                true, "Royal Flush", straight_flush_result.high_card, straight_flush_result.winners
            };
        }
        return {
            false, "Royal Flush"
        };
    }


    ValidationResult StraightFlushValidator::validate(const std::vector<Card> &hand) const {
        std::unordered_map<Suit, std::vector<Card> > grouped_by_suit;
        grouped_by_suit.reserve(4); // 4 suits

        for (const auto &card: hand) {
            grouped_by_suit[card.suit].push_back(card);
        }

        for (auto &v: grouped_by_suit | std::views::values) {
            if (v.size() >= 5) {
                const auto [is_straight, high_card, winners] = find_straight(v);
                if (is_straight) {
                    return {
                        true, "Straight Flush", high_card, winners
                    };
                }
            }
        }

        return {
            false, "Straight Flush"
        };
    }

    ValidationResult FullHouseValidator::validate(const std::vector<Card> &hand) const {
        auto grouped_ranking = group_by_rank(hand);
        std::optional<Card> three_high_card = std::nullopt;
        std::optional<std::unordered_set<Card, CardHasher> > winners = std::nullopt;
        std::optional<std::vector<Card>> kickers = std::nullopt;

        for (const auto &v: grouped_ranking | std::views::values) {
            if (v.size() >= 3 && !three_high_card) {
                three_high_card = v[0];
                if (!winners) {
                    winners.emplace();
                    winners->reserve(5);
                }

                for (size_t i = 0; i < 3; i++) {
                    winners->insert(v[i]);
                }
            } else if (v.size() >= 2 && !kickers) {
                kickers.emplace();
                if (!winners) {
                    winners.emplace();
                    winners->reserve(5);
                }
                for (size_t i = 0; i < 2; i++) {
                    winners->insert(v[i]);
                    kickers->push_back(v[i]);
                }
            }
        }
        if (three_high_card && kickers) {
            return {
                true,
                "Full House",
                three_high_card,
                winners,
                kickers
            };
        }
        return {
            false, "Full House"
        };
    }

    ValidationResult FlushValidator::validate(const std::vector<Card> &hand) const {
        std::unordered_map<Suit, std::vector<Card> > grouped_by_suit;
        grouped_by_suit.reserve(4); // 4 suits

        for (const auto &card: hand) {
            grouped_by_suit[card.suit].push_back(card);
        }

        for (auto &v: grouped_by_suit | std::views::values) {
            if (v.size() >= 5) {
                std::ranges::sort(v.begin(), v.end(), cardComparatorDescending);
                std::unordered_set<Card, CardHasher> winners;
                for (size_t i = 0; i < 5; i++) {
                    winners.insert(v[i]);
                }

                return {
                    true,
                    "Flush",
                    v[0],
                    winners
                };
            }
        }

        return {
            false, "Flush"
        };
    }

    ValidationResult StraightValidator::validate(const std::vector<Card> &hand) const {
        const auto [is_straight, high_card, winners] = find_straight(hand);
        if (is_straight) {
            return {
                true, "Straight", high_card, winners
            };
        }
        return {
            false, "Straight"
        };
    }

    ValidationResult FourOfAKindValidator::validate(const std::vector<Card> &hand) const {
        return multiple_card_validator(hand, 4, "Four of a Kind");
    }

    ValidationResult ThreeOfAKindValidator::validate(const std::vector<Card> &hand) const {
        return multiple_card_validator(hand, 3, "Three of a Kind");
    }

    ValidationResult TwoPairValidator::validate(const std::vector<Card> &hand) const {
        auto grouped_hand = group_by_rank(hand);
        std::unordered_set<Card, CardHasher> winners;
        std::optional<Card> high_card;
        bool found_first_pair = false;
        bool found_second_pair = false;
        std::vector<Card> kickers;
        ValidationResult result(false, "Two Pair");

        for (const auto &v: grouped_hand | std::views::values) {
            if (v.size() >= 2 && !found_second_pair) {
                for (size_t i = 0; i < 2; i++) {
                    winners.insert(v[i]);
                }
                if (found_first_pair) {
                    result.valid = true;
                    result.high_card = high_card;
                    result.winners = winners;
                    found_second_pair = true;
                    continue;
                }
                found_first_pair = true;
                high_card = v[0];
            } else if (kickers.empty()) {
                kickers.push_back(v[0]);
                result.kickers = kickers;
            }
        }

        return result;
    }

    ValidationResult OnePairValidator::validate(const std::vector<Card> &hand) const {
        return multiple_card_validator(hand, 2, "Two of a Kind");
    }

    ValidationResult HighCardValidator::validate(const std::vector<Card> &hand) const {
        std::vector<Card> sorted_hand = hand;
        std::ranges::sort(sorted_hand.begin(), sorted_hand.end(), cardComparatorDescending);
        std::vector<Card> kickers;
        Card high_card = sorted_hand[0];
        std::unordered_set<Card, CardHasher> winners{high_card};

        for (size_t i = 1; i < 5; i++) {
            kickers.push_back(sorted_hand[i]);
        }

        return {
            true, "High Card", high_card, winners, kickers
        };
    }

    std::vector<ShowdownResult> ShowdownEvaluator::evaluate(const std::unordered_map<size_t, std::vector<Card> > &hands) {
        std::vector<std::pair<size_t, ValidationResult> > winners;
        for (const auto *validator: validators) {
            for (const auto &[index, hand]: hands) {
                const auto validation_result = validator->validate(hand);
                if (validation_result.valid) {
                    winners.emplace_back(index, validation_result);
                }
            }

            if (!winners.empty()) {
                break;
            }
        }

        size_t kicker_count = winners[0].second.kickers->size();
        if (winners.size() > 1 && kicker_count > 0) {
            // Determine winner based on kickers.

            for (size_t kicker = 0; kicker < kicker_count; kicker++) {
                std::vector<std::pair<size_t, ValidationResult>> continued_winners;

                const Rank* highest_rank = nullptr;

                for (auto & winner : winners) {
                    const auto &current_kicker = winner.second.kickers.value()[kicker];
                    const auto current_rank_index = rankIndex(current_kicker.rank);

                    if (highest_rank == nullptr || current_rank_index == rankIndex(*highest_rank)) {
                        continued_winners.push_back(winner);
                        highest_rank = &current_kicker.rank;
                    }
                    else if (rankIndex(*highest_rank) < current_rank_index) {
                        continued_winners.clear();
                        continued_winners.push_back(winner);
                        highest_rank = &current_kicker.rank;
                    }

                }

                winners = continued_winners;
                if (winners.size() == 1) {
                    break;
                }
            }
        }

        std::vector<ShowdownResult> showdown_results;
        showdown_results.reserve(winners.size());
        for (const auto &[index, result]: winners) {
            showdown_results.emplace_back(index, result);
        }

        return showdown_results;
    }
}
