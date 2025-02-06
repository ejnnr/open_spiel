#include "open_spiel/games/dominion/cards.h"
#include "open_spiel/games/dominion/dominion.h"

namespace open_spiel
{
    namespace dominion
    {

        bool Card::IsType(CardType type) const { return card_types.find(type) != card_types.end(); }

        bool Card::IsPlayable() const
        {
            if (IsType(CardType::Treasure))
                return true;
            if (IsType(CardType::Action))
                return true;
            return false;
        }

        void BasicTreasure::Play(DominionState &state) const { state.n_coins += value; }

        void Village::Play(DominionState &state) const
        {
            state.CurrentPlayerState().DrawCard(1);
            state.n_actions += 2;
        }

        void Woodcutter::Play(DominionState &state) const
        {
            state.n_buys += 1;
            state.n_coins += 2;
        }

        void Smithy::Play(DominionState &state) const
        {
            state.CurrentPlayerState().DrawCard(3);
        }

        void Market::Play(DominionState &state) const
        {
            state.CurrentPlayerState().DrawCard(1);
            state.n_actions += 1;
            state.n_coins += 1;
            state.n_buys += 1;
        }

        void Festival::Play(DominionState &state) const
        {
            state.n_actions += 2;
            state.n_buys += 1;
            state.n_coins += 2;
        }

        void Laboratory::Play(DominionState &state) const
        {
            state.CurrentPlayerState().DrawCard(2);
            state.n_actions += 1;
        }

        void CouncilRoom::Play(DominionState &state) const
        {
            state.CurrentPlayerState().DrawCard(4);
            state.n_buys += 1;
            for (size_t i = 0; i < state.players.size(); ++i)
            {
                if (i != state.CurrentPlayer())
                    state.players[i].DrawCard(1);
            }
        }

        void Workshop::Play(DominionState &state) const
        {
        }
    } // namespace dominion
} // namespace open_spiel