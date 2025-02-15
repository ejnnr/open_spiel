#include "open_spiel/games/dominion/cards.h"

#include "open_spiel/games/dominion/card_registry.h"
#include "open_spiel/games/dominion/dominion.h"
#include "open_spiel/games/dominion/effects.h"
#include "open_spiel/games/dominion/player_state.h"

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
  co_await state.DrawCard(1);
  state.n_actions += 2;
  co_return;
}

Coroutine Woodcutter::Play(DominionState &state) const {
  state.n_buys += 1;
  state.n_coins += 2;
  co_return;
}

Coroutine Smithy::Play(DominionState &state) const {
  co_await state.DrawCard(3);
  co_return;
}

Coroutine Market::Play(DominionState &state) const {
  co_await state.DrawCard(1);
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
  co_await state.DrawCard(2);
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
      legal_actions.push_back(GetActionId(ActionType::kSelectSupplyCard, i));
    }
  }
  if (legal_actions.empty()) co_return;

  DominionAction action = co_await getDominionAction(state, legal_actions);
  SPIEL_CHECK_EQ(action.type, ActionType::kSelectSupplyCard);
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
      actions.push_back(GetActionId(ActionType::kSelectHandCard, hand_choice));
    }
    DominionAction action = co_await getDominionAction(state, actions);
    if (action.type == ActionType::kEnd) break;
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
      actions.push_back(GetActionId(ActionType::kSelectHandCard, hand_choice));
    }
    DominionAction action = co_await getDominionAction(state, actions);
    if (action.type == ActionType::kEnd) break;
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
    legal_actions.push_back(GetActionId(ActionType::kSelectHandCard, i));
  }

  DominionAction action = co_await getDominionAction(state, legal_actions);
  state.TrashFromHand(action.index);

  std::vector<Action> supply_choices{};
  for (size_t i = 0; i < state.supply_counts.size(); ++i) {
    if (state.supply_counts[i] > 0 && card_registry::get(i)->cost <= 4) {
      supply_choices.push_back(GetActionId(ActionType::kSelectSupplyCard, i));
    }
  }
  if (supply_choices.empty()) co_return;

  DominionAction supply_action =
      co_await getDominionAction(state, supply_choices);
  state.CurrentDiscard().push_back(card_registry::get(supply_action.index));
}

Coroutine ThroneRoom::Play(DominionState &state) const {
  std::vector<Action> legal_actions{};
  legal_actions.push_back(GetActionId(ActionType::kEnd));
  for (size_t i = 0; i < state.CurrentHand().size(); ++i) {
    if (state.CurrentHand()[i]->IsAction())
      legal_actions.push_back(GetActionId(ActionType::kSelectHandCard, i));
  }

  // Only kEnd available, so can just skip instead of pointlessly awaiting that
  if (legal_actions.size() == 1) co_return;

  DominionAction action = co_await getDominionAction(state, legal_actions);
  if (action.type == ActionType::kEnd) co_return;

  // Note: we don't want to use DominionState::PlayCard here, because that costs
  // an action (and we can't use it the second time anyway because the card
  // won't be in hand).

  // First, use PlayerState::PlayCard to put the card in the playing area
  Card &card = state.CurrentPlayerState().PlayCard(action.index);
  // Then, execute the card's effect twice. We need to co_await to make sure we
  // finish the first effect before starting the second.
  co_await card.Play(state);
  co_await card.Play(state);
}

int Gardens::GetVictoryPoints(const PlayerState &player_state) const {
  int total_cards = player_state.deck.size() + player_state.hand.size() +
                    player_state.playing_area.size() +
                    player_state.discard.size();
  return total_cards / 10;
}

Coroutine Library::Play(DominionState &state) const {
  std::vector<Card *> set_aside_cards{};
  while (state.CurrentHand().size() < 7 &&
         !(state.CurrentDeck().empty() && state.CurrentDiscard().empty())) {
    co_await state.DrawCard(1);
    Card *card = state.CurrentPlayerState().hand.back();
    if (card->IsAction()) {
      // 0: keep card, 1: set aside
      Action action = co_await getAction(state, {0, 1});
      if (action == 1) {
        set_aside_cards.push_back(card);
        state.CurrentPlayerState().hand.pop_back();
      }
    }
  }
  for (Card *card : set_aside_cards) {
    state.CurrentDiscard().push_back(card);
  }
  co_return;
}

}  // namespace dominion
}  // namespace open_spiel