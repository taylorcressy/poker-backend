#pragma once

#include <optional>
#include <unordered_set>

namespace pokergame::core::types {
    template<class T>
    constexpr std::underlying_type_t<T> to_underlying(T t) noexcept {
        static_assert(std::is_enum_v<T>);
        return static_cast<std::underlying_type_t<T>>(t);
    }

    enum class Rank {
        Two,
        Three,
        Four,
        Five,
        Six,
        Seven,
        Eight,
        Nine,
        Ten,
        Jack,
        Queen,
        King,
        Ace
    };

    constexpr int rankIndex(Rank r) noexcept {
        return to_underlying(r);
    }


    struct RankSorterDescending {
        bool operator()(const Rank &a, const Rank &b) const {
            return rankIndex(a) > rankIndex(b);
        }
    };

    constexpr std::string_view rankToString(const Rank rank) {
        switch (rank) {
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

    enum class Suit {
        Clubs,
        Diamonds,
        Hearts,
        Spades
    };

    constexpr int suitIndex(const Suit s) noexcept {
        return to_underlying(s);
    }

    constexpr std::string_view suitToString(const Suit s) {
        switch (s) {
            case Suit::Clubs: return "Clubs";
            case Suit::Spades: return "Spades";
            case Suit::Hearts: return "Hearts";
            case Suit::Diamonds: return "Diamonds";
            default: return "Unknown";
        }
    }

    struct Card {
        Rank rank;
        Suit suit;

        bool operator==(const Card &other) const {
            return other.rank == this->rank && other.suit == this->suit;
        }
    };

    std::string cardToString(Card);

    struct CardHasher {
        std::size_t operator()(const Card &card) const noexcept {
            return suitIndex(card.suit) << 2 | rankIndex(card.rank);
        }
    };

    struct CardGreater {
        bool operator()(const Card &a, const Card &b) const {
            return rankIndex(a.rank) > rankIndex(b.rank);
        }
    };

    class Deck {
    public:
        Deck();

        ~Deck() = default;

        /**
         * Shuffle and resets the deck pointer
         */
        void shuffle();

        Card drawCard();

        std::string deckAsString() const;

        void printDeck() const;

        static constexpr int MaxCards = 52;

    private:
        std::vector<Card> cards;
        int currentCard;
    };

    enum class SeatState {
        Empty,
        InHand,
        Folded,
        AllIn,
    };

    inline std::string_view seatStateToString(const SeatState &state) {
        switch (state) {
            case SeatState::AllIn:
                return "All In";
            case SeatState::Empty:
                return "Empty";
            case SeatState::Folded:
                return "Folded";
            case SeatState::InHand:
                return "In Hand";
            default:
                return "Unknown";
        }
    }

    typedef unsigned long bet_t;

    struct Seat {
        std::string name;
        std::vector<Card> hand;
        bet_t chips;
        SeatState seat_state;
    };



    enum class GameState {
        NotStarted,
        Init,
        PreFlop,
        Flop,
        Turn,
        River,
        Showdown,
        RoundComplete,
        RequestingPlayerAction,
    };

    inline std::string_view gameStateToString(const GameState &state) {
        switch (state) {
            case GameState::Init:
                return "INIT";
            case GameState::PreFlop:
                return "PRE_FLOP";
            case GameState::Flop:
                return "FLOP";
            case GameState::Turn:
                return "TURN";
            case GameState::River:
                return "RIVER";
            case GameState::Showdown:
                return "SHOWDOWN";
            case GameState::RoundComplete:
                return "ROUND_COMPLETE";
            case GameState::NotStarted:
                return "NOT_STARTED";
            case GameState::RequestingPlayerAction:
                return "REQUESTING_PLAYER_ACTION";
            default:
                return "UNKNOWN";
        }
    }

    struct PokerConfiguration {
        size_t number_of_seats;
        unsigned long ante;
        unsigned long chips_when_seated;
        uint8_t time_to_response_in_seconds;
    };

    struct Pot {
        bet_t amount = 0;
        std::unordered_set<size_t> participants;
    };

    enum class BetType {
        Check,
        SmallBlind,
        BigBlind,
        Fold,
        Call,
        Bet,
        Raise
    };
}
