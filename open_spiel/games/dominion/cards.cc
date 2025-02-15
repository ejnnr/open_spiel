#include "open_spiel/games/dominion/cards.h"

#include "open_spiel/games/dominion/card_registry.h"
#include "open_spiel/games/dominion/dominion.h"
#include "open_spiel/games/dominion/effects.h"
#include "open_spiel/games/dominion/player_state.h"

namespace open_spiel {
namespace dominion {

Coroutine<std::optional<size_t>> SelectSupplyCard(DominionState &state,
                                                  bool allow_none) {
  return SelectSupplyCard(state, std::numeric_limits<int>::max(), allow_none);
}

Coroutine<std::optional<size_t>> SelectSupplyCard(DominionState &state,
                                                  int max_cost,
                                                  bool allow_none) {
  std::vector<Action> legal_actions;
  if (allow_none) legal_actions.push_back(GetActionId(ActionType::kEnd));
  for (const auto &[card_id, count] : state.supply_counts) {
    if (count > 0 && card_registry::get(card_id)->cost <= max_cost) {
      legal_actions.push_back(
          GetActionId(ActionType::kSelectSupplyCard, card_id));
    }
  }
  if (legal_actions.empty()) co_return std::nullopt;
  // open_spiel requires actions to be sorted, and our supply might not be
  std::sort(legal_actions.begin(), legal_actions.end());
  DominionAction action = co_await getDominionAction(state, legal_actions);
  if (action.type == ActionType::kEnd) co_return std::nullopt;
  co_return action.index;
}

Coroutine<std::optional<size_t>> SelectHandCard(DominionState &state,
                                                bool allow_none) {
  std::vector<Action> legal_actions;
  if (allow_none) legal_actions.push_back(GetActionId(ActionType::kEnd));
  for (size_t i = 0; i < state.CurrentHand().size(); ++i) {
    legal_actions.push_back(GetActionId(ActionType::kSelectHandCard, i));
  }
  if (legal_actions.empty()) co_return std::nullopt;
  DominionAction action = co_await getDominionAction(state, legal_actions);
  if (action.type == ActionType::kEnd) co_return std::nullopt;
  co_return action.index;
}

bool Card::IsType(CardType type) const {
  return card_types.find(type) != card_types.end();
}

bool Card::IsPlayable() const {
  if (IsType(CardType::Treasure)) return true;
  if (IsType(CardType::Action)) return true;
  return false;
}

Coroutine<void> BasicTreasure::Play(DominionState &state) const {
  state.n_coins += value;
  co_return;
}

Coroutine<void> Village::Play(DominionState &state) const {
  co_await state.DrawCard(1);
  state.n_actions += 2;
  co_return;
}

Coroutine<void> Woodcutter::Play(DominionState &state) const {
  state.n_buys += 1;
  state.n_coins += 2;
  co_return;
}

Coroutine<void> Smithy::Play(DominionState &state) const {
  co_await state.DrawCard(3);
  co_return;
}

Coroutine<void> Market::Play(DominionState &state) const {
  co_await state.DrawCard(1);
  state.n_actions += 1;
  state.n_coins += 1;
  state.n_buys += 1;
  co_return;
}

Coroutine<void> Festival::Play(DominionState &state) const {
  state.n_actions += 2;
  state.n_buys += 1;
  state.n_coins += 2;
  co_return;
}

Coroutine<void> Laboratory::Play(DominionState &state) const {
  co_await state.DrawCard(2);
  state.n_actions += 1;
  co_return;
}

Coroutine<void> CouncilRoom::Play(DominionState &state) const {
  co_await state.DrawCard(4);
  state.n_buys += 1;
  for (size_t i = 0; i < state.players.size(); ++i) {
    if (i != state.cur_player_) co_await state.DrawCardForPlayer(1, i);
  }
  co_return;
}

Coroutine<void> Workshop::Play(DominionState &state) const {
  std::optional<size_t> supply_card = co_await SelectSupplyCard(state, 4);
  if (!supply_card) co_return;
  state.GainCard(supply_card.value());
  co_return;
}

Coroutine<void> Chapel::Play(DominionState &state) const {
  for (int i = 0; i < 4; ++i) {
    std::optional<size_t> hand_card = co_await SelectHandCard(state, true);
    if (!hand_card) co_return;
    state.TrashFromHand(hand_card.value());
  }
  co_return;
}

Coroutine<void> Cellar::Play(DominionState &state) const {
  state.n_actions += 1;

  int cards_discarded = 0;
  while (true) {
    std::optional<size_t> hand_card = co_await SelectHandCard(state, true);
    if (!hand_card) break;
    state.DiscardFromHand(hand_card.value());
    cards_discarded++;
  }

  co_await state.DrawCard(cards_discarded);
  co_return;
}

Coroutine<void> Moneylender::Play(DominionState &state) const {
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

Coroutine<void> Remodel::Play(DominionState &state) const {
  if (state.CurrentHand().size() == 0) co_return;

  std::optional<size_t> hand_card = co_await SelectHandCard(state, false);
  if (!hand_card) co_return;
  int cost = state.CurrentHand()[hand_card.value()]->cost;
  state.TrashFromHand(hand_card.value());

  std::optional<size_t> supply_card =
      co_await SelectSupplyCard(state, cost + 2);
  if (!supply_card) co_return;
  state.GainCard(supply_card.value());
}

Coroutine<void> ThroneRoom::Play(DominionState &state) const {
  std::vector<Action> legal_actions{};
  legal_actions.push_back(GetActionId(ActionType::kEnd));
  for (size_t i = 0; i < state.CurrentHand().size(); ++i) {
    if (state.CurrentHand()[i]->IsAction())
      legal_actions.push_back(GetActionId(ActionType::kSelectHandCard, i));
  }

  // Only kEnd available, so can just skip instead of pointlessly awaiting
  // that
  if (legal_actions.size() == 1) co_return;

  DominionAction action = co_await getDominionAction(state, legal_actions);
  if (action.type == ActionType::kEnd) co_return;

  // Note: we don't want to use DominionState::PlayCard here, because that
  // costs an action (and we can't use it the second time anyway because the
  // card won't be in hand).

  // First, use PlayerState::PlayCard to put the card in the playing area
  Card &card = state.CurrentPlayerState().PlayCard(action.index);
  // Then, execute the card's effect twice. We need to co_await to make sure
  // we finish the first effect before starting the second.
  co_await card.Play(state);
  co_await card.Play(state);
}

int Gardens::GetVictoryPoints(const PlayerState &player_state) const {
  int total_cards = player_state.deck.size() + player_state.hand.size() +
                    player_state.playing_area.size() +
                    player_state.discard.size();
  return total_cards / 10;
}

Coroutine<void> Library::Play(DominionState &state) const {
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

Coroutine<void> Witch::Play(DominionState &state) const {
  // First draw 2 cards
  co_await state.DrawCard(2);

  // Then each other player gains a curse if available
  for (size_t i = 0; i < state.players.size(); ++i) {
    if (i != state.cur_player_) {
      // Find curse pile index
      size_t curse_id = card_registry::get_id("Curse");
      state.GainCard(curse_id, i);
    }
  }
  co_return;
}

Coroutine<void> Artisan::Play(DominionState &state) const {
  std::optional<size_t> supply_card = co_await SelectSupplyCard(state, 5);
  if (supply_card) {
    state.GainCard(supply_card.value());
    // Artisan gains to hand, so move the card there
    state.CurrentHand().push_back(state.CurrentDiscard().back());
    state.CurrentDiscard().pop_back();
  }
  std::optional<size_t> hand_card = co_await SelectHandCard(state, true);
  if (hand_card) {
    // TODO: this is wrong, the card should be top-decked. But the current
    // implementation doesn't allow that, since we don't track deck order
    // We'll need to track deck order, where most cards are "unknown"
    // placeholders.
    state.CurrentDeck().push_back(state.CurrentHand()[hand_card.value()]);
    state.CurrentHand().erase(state.CurrentHand().begin() + hand_card.value());
  }
  co_return;
}

}  // namespace dominion
}  // namespace open_spiel