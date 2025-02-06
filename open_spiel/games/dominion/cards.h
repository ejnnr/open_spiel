#pragma once

#include <set>
#include <string>

namespace open_spiel
{
    namespace dominion
    {
        class DominionState;

        enum class CardType
        {
            Treasure,
            Victory,
            Action,
        };

        class Card
        {
        public:
            const std::set<CardType> card_types;
            const int cost;
            const int vp;
            const std::string name;

            // Card(Card&&) = default;
            // Card& operator=(Card&&) = default;

            // // Cards should not be copied
            // Card(const Card&) = delete;
            // Card& operator=(const Card&) = delete;

            virtual void Play(DominionState &state) const {};
            bool IsType(CardType type) const;
            bool IsPlayable() const;
            bool IsTreasure() const { return IsType(CardType::Treasure); }
            bool IsVictory() const { return IsType(CardType::Victory); }
            bool IsAction() const { return IsType(CardType::Action); }

        protected:
            Card(std::set<CardType> card_types, int cost, const std::string &name,
                 int vp = 0)
                : card_types{std::move(card_types)}, cost{cost}, name{name}, vp{vp} {}
        };

        class Village : public Card
        {
        public:
            Village() : Card{{CardType::Action}, 3, "Village"} {}
            void Play(DominionState &state) const override;
        };

        class Woodcutter : public Card
        {
        public:
            Woodcutter() : Card{{CardType::Action}, 3, "Woodcutter"} {}
            void Play(DominionState &state) const override;
        };

        class Smithy : public Card
        {
        public:
            Smithy() : Card{{CardType::Action}, 4, "Smithy"} {}
            void Play(DominionState &state) const override;
        };

        class Market : public Card
        {
        public:
            Market() : Card{{CardType::Action}, 5, "Market"} {}
            void Play(DominionState &state) const override;
        };

        class Festival : public Card
        {
        public:
            Festival() : Card{{CardType::Action}, 5, "Festival"} {}
            void Play(DominionState &state) const override;
        };

        class Laboratory : public Card
        {
        public:
            Laboratory() : Card{{CardType::Action}, 5, "Laboratory"} {}
            void Play(DominionState &state) const override;
        };

        class CouncilRoom : public Card
        {
        public:
            CouncilRoom() : Card{{CardType::Action}, 5, "Council Room"} {}
            void Play(DominionState &state) const override;
        };

        class Workshop : public Card
        {
        public:
            Workshop() : Card{{CardType::Action}, 3, "Workshop"} {}
            void Play(DominionState &state) const override;
        };

        class BasicTreasure : public Card
        {
        public:
            BasicTreasure(int cost, int value, const std::string &name)
                : Card{{CardType::Treasure}, cost, name}, value{value} {}
            void Play(DominionState &state) const override;

        private:
            int value;
        };

        class Copper : public BasicTreasure
        {
        public:
            Copper() : BasicTreasure{0, 1, "Copper"} {}
        };

        class Silver : public BasicTreasure
        {
        public:
            Silver() : BasicTreasure{3, 2, "Silver"} {}
        };

        class Gold : public BasicTreasure
        {
        public:
            Gold() : BasicTreasure{6, 3, "Gold"} {}
        };

        class BasicVictory : public Card
        {
        public:
            BasicVictory(int cost, int vp, const std::string &name)
                : Card{{CardType::Victory}, cost, name, vp} {}
        };

        class Estate : public BasicVictory
        {
        public:
            Estate() : BasicVictory{2, 1, "Estate"} {}
        };

        class Duchy : public BasicVictory
        {
        public:
            Duchy() : BasicVictory{5, 3, "Duchy"} {}
        };

        class Province : public BasicVictory
        {
        public:
            Province() : BasicVictory{8, 6, "Province"} {}
        };
    } // namespace dominion
} // namespace open_spiel