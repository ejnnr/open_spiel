#include "player_state.h"

#include <algorithm>
#include <random>
#include <sstream>
#include <string>

#include "card_registry.h"
#include "cards.h"
#include "dominion.h"

namespace open_spiel {
namespace dominion {
PlayerState::PlayerState(bool default_setup) {
  if (!default_setup) return;

  deck.reserve(10);
  for (int i = 0; i < 3; ++i) deck.push_back(card_registry::get("Estate"));
  for (int i = 0; i < 7; ++i) deck.push_back(card_registry::get("Copper"));
}

Card &PlayerState::PlayCard(size_t hand_index) {
  if (hand_index < hand.size()) {
    if (!hand[hand_index]->IsPlayable())
      throw std::runtime_error("Card is not playable");
    playing_area.push_back(hand[hand_index]);
    hand.erase(hand.begin() + hand_index);
    return *playing_area.back();
  }
  throw std::out_of_range("Hand index out of range");
}

int PlayerState::VpCount() const {
  int vp = 0;
  for (const auto &card : discard) {
    vp += card->GetVictoryPoints(*this);
  }
  for (const auto &card : playing_area) {
    vp += card->GetVictoryPoints(*this);
  }
  for (const auto &card : hand) {
    vp += card->GetVictoryPoints(*this);
  }
  for (const auto &card : deck) {
    vp += card->GetVictoryPoints(*this);
  }
  return vp;
}

// std::vector<std::unique_ptr<Choice>>
// GainDecision::legal_choices(GameState &state) const
// {
//     std::vector<std::unique_ptr<Choice>> choices;
//     for (const auto &[card_name, count] : state.supply_counts)
//     {
//         if (count > 0)
//             choices.push_back(
//                 std::make_unique<CardChoice>(card_registry::get(card_name)));
//     }
//     return choices;
// }

// void GainDecision::apply(GameState &state, Choice &choice) const
// {
//     auto *card_choice = dynamic_cast<CardChoice *>(&choice);
//     if (!card_choice)
//         throw std::runtime_error("Invalid choice type");
//     Card *card = card_choice->card;
//     state.current_discard().push_back(card);
// }
}  // namespace dominion
}  // namespace open_spiel
