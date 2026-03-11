//
// Created by tcressy on 2/11/26.
//

#pragma once
#include <atomic>
#include <optional>
#include <unordered_set>
#include <vector>
#include <string>
#include <type_traits>

namespace pokergame {
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

    std::string_view rankToString(Rank r);

    enum class Suit {
        Clubs,
        Diamonds,
        Hearts,
        Spades
    };

    constexpr int suitIndex(Suit s) noexcept {
        return to_underlying(s);
    }

    std::string_view suitToString(Suit s);

    struct Card {
        Rank rank;
        Suit suit;

        bool operator==(const Card &other) const {
            return other.rank == this->rank && other.suit == this->suit;
        }
    };

    std::string cardToString(Card s);

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

        std::string deckAsString();

        void printDeck();

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
        RoundComplete
    };

    struct PokerConfiguration {
        size_t number_of_seats;
        unsigned long ante;
        unsigned long minimum_number_of_chips_to_start;
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

    struct PlayerAction {
        BetType type;
        std::optional<bet_t> amount;
    };

    class PokerGame {
    public:
        PokerGame(PokerConfiguration poker_configuration);

        ~PokerGame() = default;

        bool seatPlayer(const std::string &name, long chips, short seat_index);

        bool start();

    private:
        const PokerConfiguration config;

        Deck deck;
        std::vector<Card> community_cards;
        std::vector<Seat> seats;
        std::vector<size_t> seated_players;
        GameState state;
        size_t dealer;

        // Poker rounds can have multiple pots if someone goes all in.
        // So we maintain the total running value of the pots (pots).
        // max_pot_sizes tell us how much a player needs to bet for each particular
        // pot to be reconciled.
        std::vector<Pot> pots;
        std::vector<bet_t> max_pot_sizes;


        std::optional<size_t> nextBetter(size_t other_player);

        void handleBettingRound();

        PlayerAction getPlayerAction(size_t player,
                                     const std::unordered_set<BetType> &allowed_actions,
                                     bet_t amount_owed);

        std::optional<bet_t> takeBet(size_t player, BetType type, std::optional<bet_t> amount);
        void setNextStreetOrFinishRound(std::optional<GameState> next_street);

        void processBetsOnRoundConclusion(std::vector<std::pair<size_t, bet_t> > &bets);

        void executeNextStateTransition();
        void handleInit();
        void handlePreFlop();
        void handleFlop();
        void handleTurn();
        void handleRiver();
        void handleShowdown();
        void handleRoundComplete();
        bool canGameStart();

        void resetTableStateForNewRound();

        // TODO: Remove / move
        void runTests();
    };
}
