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

        Coroutine BasicTreasure::Play(DominionState &state) const
        {
            state.n_coins += value;
            co_return;
        }

        Coroutine Village::Play(DominionState &state) const
        {
            // co_await state.DrawCard(1);
            state.n_actions += 2;
            co_return;
        }

        Coroutine Woodcutter::Play(DominionState &state) const
        {
            state.n_buys += 1;
            state.n_coins += 2;
            co_return;
        }

        Coroutine Smithy::Play(DominionState &state) const
        {
            auto drawResult = state.DrawCard(3);
            // co_await drawResult;
            co_return;
        }

        Coroutine Market::Play(DominionState &state) const
        {
            // co_await state.DrawCard(1);
            state.n_actions += 1;
            state.n_coins += 1;
            state.n_buys += 1;
            co_return;
        }

        Coroutine Festival::Play(DominionState &state) const
        {
            state.n_actions += 2;
            state.n_buys += 1;
            state.n_coins += 2;
            co_return;
        }

        Coroutine Laboratory::Play(DominionState &state) const
        {
            // co_await state.DrawCard(2);
            state.n_actions += 1;
            co_return;
        }

        Coroutine CouncilRoom::Play(DominionState &state) const
        {
            // co_await state.DrawCard(4);
            state.n_buys += 1;
            for (size_t i = 0; i < state.players.size(); ++i)
            {
                // if (i != state.CurrentPlayer())
                // co_await state.DrawCardForPlayer(1, i);
            }
            co_return;
        }

        Coroutine Workshop::Play(DominionState &state) const
        {
            co_return;
        }
    } // namespace dominion
} // namespace open_spiel