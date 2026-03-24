//
// Created by tcressy on 2/11/26.
//

#include <iostream>
#include <random>
#include <algorithm>
#include <string>

#include "poker_types.h"
#include "poker_core.h"

namespace pokergame::core::types {

    static constexpr Rank allRanks[] = {
        Rank::Two, Rank::Three, Rank::Four, Rank::Five, Rank::Six, Rank::Seven, Rank::Eight, Rank::Nine, Rank::Ten,
        Rank::Jack, Rank::Queen, Rank::King, Rank::Ace
    };

    static constexpr Suit allSuits[] = {
        Suit::Clubs, Suit::Diamonds, Suit::Hearts, Suit::Spades
    };

    Deck::Deck(): currentCard(0) {
        this->cards.reserve(MaxCards);

        for (auto const r: allRanks) {
            for (auto const s: allSuits) {
                this->cards.emplace_back(r, s);
            }
        }
    }

    Card Deck::drawCard() {
        return cards[currentCard++];
    }

    void Deck::shuffle() {
        this->currentCard = 0;

        std::random_device rd;
        std::default_random_engine gen(rd());

        std::ranges::shuffle(this->cards, gen);
    }

    std::string cardToString(const Card card) {
        return std::string(rankToString(card.rank)) + " of " + std::string(suitToString(card.suit));
    }

    std::string Deck::deckAsString() const {
        std::string formatted_string;
        for (const auto card: this->cards) {
            formatted_string += cardToString(card);
        }
        return formatted_string;
    }

    void Deck::printDeck() const {
        std::cout << this->deckAsString() << std::endl;
    }
}