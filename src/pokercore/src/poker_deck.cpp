//
// Created by tcressy on 2/11/26.
//

#include <iostream>
#include <random>
#include <algorithm>
#include <string>

#include "poker_core.h"

namespace pokergame {
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

    std::string_view rankToString(Rank r) {
        switch (r) {
            case Rank::Ace: return "Ace";
            case Rank::King: return "King";
            case Rank::Queen: return "Queen";
            case Rank::Jack: return "Jack";
            case Rank::Ten: return "Ten";
            case Rank::Nine: return "Nine";
            case Rank::Eight: return "Eight";
            case Rank::Seven: return "Seven";
            case Rank::Six: return "Six";
            case Rank::Five: return "Five";
            case Rank::Four: return "Four";
            case Rank::Three: return "Three";
            case Rank::Two: return "Two";
            default: return "Unknown";
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

    std::string_view suitToString(Suit s) {
        switch (s) {
            case Suit::Clubs: return "Clubs";
            case Suit::Spades: return "Spades";
            case Suit::Hearts: return "Hearts";
            case Suit::Diamonds: return "Diamonds";
            default: return "Unknown";
        }
    }

    std::string cardToString(Card card) {
        return std::string(rankToString(card.rank)) + " of " + std::string(suitToString(card.suit));
    }

    std::string Deck::deckAsString() {
        std::string formatted_string;
        for (const auto card: this->cards) {
            formatted_string += cardToString(card);
        }
        return formatted_string;
    }

    void Deck::printDeck() {
        std::cout << this->deckAsString() << std::endl;
    }
}