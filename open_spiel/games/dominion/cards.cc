#include "open_spiel/games/dominion/cards.h"

#include "open_spiel/games/dominion/card_registry.h"
#include "open_spiel/games/dominion/dominion.h"
#include "open_spiel/games/dominion/effects.h"

namespace open_spiel {
namespace dominion {

bool Card::IsType(CardType type) const {
  return card_types.find(type) != card_types.end();
}

bool Card::IsPlayable() const {
  if (IsType(CardType::Treasure)) return true;
  if (IsType(CardType::Action)) return true;
  return false;
}

Coroutine BasicTreasure::Play(DominionState &state) const {
  state.n_coins += value;
  co_return;
}

Coroutine Village::Play(DominionState &state) const {
  state.DrawCard(1);
  state.n_actions += 2;
  co_return;
}

Coroutine Woodcutter::Play(DominionState &state) const {
  state.n_buys += 1;
  state.n_coins += 2;
  co_return;
}

Coroutine Smithy::Play(DominionState &state) const {
  state.DrawCard(3);
  co_return;
}

Coroutine Market::Play(DominionState &state) const {
  state.DrawCard(1);
  state.n_actions += 1;
  state.n_coins += 1;
  state.n_buys += 1;
  co_return;
}

Coroutine Festival::Play(DominionState &state) const {
  state.n_actions += 2;
  state.n_buys += 1;
  state.n_coins += 2;
  co_return;
}

Coroutine Laboratory::Play(DominionState &state) const {
  state.DrawCard(2);
  state.n_actions += 1;
  co_return;
}

Coroutine CouncilRoom::Play(DominionState &state) const {
  co_await state.DrawCard(4);
  state.n_buys += 1;
  for (size_t i = 0; i < state.players.size(); ++i) {
    if (i != state.cur_player_) co_await state.DrawCardForPlayer(1, i);
  }
  co_return;
}

Coroutine Workshop::Play(DominionState &state) const {
  std::vector<Action> legal_actions;
  for (size_t i = 0; i < state.supply_counts.size(); ++i) {
    if (state.supply_counts[i] > 0 && card_registry::get(i)->cost <= 4) {
      legal_actions.push_back(
          DominionAction(DominionAction::Type::kSelectSupplyCard, i)
              .ToAction());
    }
  }
  if (legal_actions.empty()) co_return;

  DominionAction action = co_await getDominionAction(state, legal_actions);
  SPIEL_CHECK_EQ(action.type, DominionAction::Type::kSelectSupplyCard);
  SPIEL_CHECK_GE(action.index, 0);
  SPIEL_CHECK_LT(action.index, state.supply_counts.size());
  if (state.supply_counts[action.index] > 0) {
    state.supply_counts[action.index] -= 1;
    state.players[state.cur_player_].discard.push_back(
        card_registry::get(action.index));
  }
  co_return;
}

Coroutine Chapel::Play(DominionState &state) const {
  for (int i = 0; i < 4; ++i) {
    std::vector<Action> hand_choices(state.CurrentHand().size());
    std::iota(hand_choices.begin(), hand_choices.end(), 0);
    std::vector<Action> actions;
    actions.reserve(hand_choices.size() + 1);
    actions.push_back(0);
    for (Action hand_choice : hand_choices) {
      actions.push_back(
          DominionAction(DominionAction::Type::kSelectHandCard, hand_choice)
              .ToAction());
    }
    DominionAction action = co_await getDominionAction(state, actions);
    if (action.type == DominionAction::Type::kEnd) break;
    state.TrashFromHand(action.index);
  }
  co_return;
}

Coroutine Cellar::Play(DominionState &state) const {
  state.n_actions += 1;

  int cards_discarded = 0;
  while (true) {
    std::vector<Action> hand_choices(state.CurrentHand().size());
    std::iota(hand_choices.begin(), hand_choices.end(), 0);
    std::vector<Action> actions;
    actions.reserve(hand_choices.size() + 1);
    actions.push_back(0);  // End action
    for (Action hand_choice : hand_choices) {
      actions.push_back(
          DominionAction(DominionAction::Type::kSelectHandCard, hand_choice)
              .ToAction());
    }
    DominionAction action = co_await getDominionAction(state, actions);
    if (action.type == DominionAction::Type::kEnd) break;
    state.DiscardFromHand(action.index);
    cards_discarded++;
  }

  co_await state.DrawCard(cards_discarded);
  co_return;
}

Coroutine Moneylender::Play(DominionState &state) const {
  // find Copper in hand
  auto copper_it =
      std::find_if(state.CurrentHand().begin(), state.CurrentHand().end(),
                   [](const Card *card) { return card->name == "Copper"; });

  // If there's no Copper, Moneylender is a no-op
  if (copper_it == state.CurrentHand().end()) co_return;

  // Trashing is optional, which matters with throne room.
  // We use 0 as the "no-op" action for consistency with "End phase" etc.
  Action action = co_await getAction(state, {0, 1});
  if (action == 1) {
    state.TrashFromHand(copper_it);
    state.n_coins += 3;
  }
  co_return;
}

Coroutine Remodel::Play(DominionState &state) const {
  if (state.CurrentHand().size() == 0) co_return;

  std::vector<Action> legal_actions;
  legal_actions.reserve(state.CurrentHand().size());
  for (size_t i = 0; i < state.CurrentHand().size(); ++i) {
    legal_actions.push_back(
        DominionAction(DominionAction::Type::kSelectHandCard, i).ToAction());
  }

  DominionAction action = co_await getDominionAction(state, legal_actions);
  state.TrashFromHand(action.index);

  std::vector<Action> supply_choices{};
  for (size_t i = 0; i < state.supply_counts.size(); ++i) {
    if (state.supply_counts[i] > 0 && card_registry::get(i)->cost <= 4) {
      supply_choices.push_back(
          DominionAction(DominionAction::Type::kSelectSupplyCard, i)
              .ToAction());
    }
  }
  if (supply_choices.empty()) co_return;

  DominionAction supply_action =
      co_await getDominionAction(state, supply_choices);
  state.CurrentDiscard().push_back(card_registry::get(supply_action.index));
}

}  // namespace dominion
}  // namespace open_spiel