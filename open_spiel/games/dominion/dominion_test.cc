// Copyright 2019 DeepMind Technologies Limited
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "open_spiel/games/dominion/dominion.h"

#include "open_spiel/games/dominion/card_registry.h"
#include "open_spiel/games/dominion/cards.h"
#include "open_spiel/games/dominion/player_state.h"
#include "open_spiel/spiel.h"
#include "open_spiel/tests/basic_tests.h"

namespace open_spiel {
namespace dominion {
namespace {

namespace testing = open_spiel::testing;

void PlayerStateTests() {
  // Test play card
  {
    PlayerState player{false};
    // Need to call this manually here since we're not loading a game
    card_registry::init();
    player.hand.push_back(card_registry::get("Copper"));
    Card &played_card = player.PlayCard(0);
    SPIEL_CHECK_TRUE(player.hand.empty());
    SPIEL_CHECK_EQ(player.playing_area.size(), 1);
    SPIEL_CHECK_TRUE(player.playing_area.back()->IsType(CardType::Treasure));
    SPIEL_CHECK_TRUE(&played_card == player.playing_area.back());
  }
}

void GameStateTests() {
  GameParameters params;
  std::shared_ptr<const Game> game = LoadGame("dominion", params);
  std::unique_ptr<State> state = game->NewInitialState();
  DominionState *dominion_state = static_cast<DominionState *>(state.get());

  // Test initial state
  SPIEL_CHECK_EQ(dominion_state->phase, Phase::Action);
  SPIEL_CHECK_EQ(dominion_state->n_actions, 1);
  SPIEL_CHECK_EQ(dominion_state->n_buys, 1);
  SPIEL_CHECK_EQ(dominion_state->n_coins, 0);

  // Test playing a Copper
  auto &hand = dominion_state->CurrentHand();
  auto copper_it = std::find_if(hand.begin(), hand.end(), [](const Card *card) {
    return card->IsTreasure();
  });
  SPIEL_CHECK_TRUE(copper_it != hand.end());
  size_t copper_index = copper_it - hand.begin();
  dominion_state->PlayCard(copper_index);
  SPIEL_CHECK_EQ(dominion_state->n_coins, 1);

  // Test phase transitions
  dominion_state->ApplyAction(0);
  SPIEL_CHECK_EQ(dominion_state->phase, Phase::Buy);
  dominion_state->ApplyAction(0);
  SPIEL_CHECK_EQ(dominion_state->phase, Phase::Action);
  SPIEL_CHECK_EQ(dominion_state->CurrentPlayer(), 1);  // Next player's turn
  dominion_state->ApplyAction(0);
  SPIEL_CHECK_EQ(dominion_state->phase, Phase::Buy);
  SPIEL_CHECK_EQ(dominion_state->CurrentPlayer(), 1);
  dominion_state->ApplyAction(0);
  SPIEL_CHECK_EQ(dominion_state->phase, Phase::Action);
  SPIEL_CHECK_EQ(dominion_state->CurrentPlayer(), 0);
  dominion_state->ApplyAction(0);
  SPIEL_CHECK_EQ(dominion_state->phase, Phase::Buy);
  SPIEL_CHECK_EQ(dominion_state->CurrentPlayer(), 0);

  // Check that reshuffle triggers a chance node
  dominion_state->ApplyAction(0);
  SPIEL_CHECK_TRUE(dominion_state->IsChanceNode());
  SPIEL_CHECK_EQ(dominion_state->players[0].deck.size(), 0);
  SPIEL_CHECK_EQ(dominion_state->players[0].hand.size(), 0);
  SPIEL_CHECK_EQ(dominion_state->players[0].discard.size(), 10);

  dominion_state->ApplyAction(0);
  // Now the shuffle should be complete and phase advanced
  SPIEL_CHECK_EQ(dominion_state->players[0].deck.size(), 5);
  SPIEL_CHECK_EQ(dominion_state->players[0].hand.size(), 5);
  SPIEL_CHECK_EQ(dominion_state->players[0].discard.size(), 0);
  SPIEL_CHECK_EQ(dominion_state->phase, Phase::Action);
  SPIEL_CHECK_EQ(dominion_state->CurrentPlayer(), 1);
}

void SmithyTests() {
  GameParameters params;
  std::shared_ptr<const Game> game = LoadGame("dominion", params);
  std::unique_ptr<State> state = game->NewInitialState();
  DominionState *dominion_state = static_cast<DominionState *>(state.get());

  // Set up a specific state where player has Smithy and 1 card in deck
  dominion_state->players[0].hand.clear();
  dominion_state->players[0].deck.clear();
  dominion_state->players[0].discard.clear();

  // Add Smithy to hand
  dominion_state->players[0].hand.push_back(card_registry::get("Smithy"));
  // Add one Copper to deck
  dominion_state->players[0].deck.push_back(card_registry::get("Copper"));
  // Add two Silvers to discard
  dominion_state->players[0].discard.push_back(card_registry::get("Silver"));
  dominion_state->players[0].discard.push_back(card_registry::get("Silver"));

  // Play Smithy (index 0 in hand)
  dominion_state->PlayCard(0);
  // First card should be drawn from deck
  SPIEL_CHECK_EQ(dominion_state->players[0].hand.size(), 1);
  SPIEL_CHECK_EQ(dominion_state->players[0].hand[0]->name, "Copper");
  // And we haven't shuffled yet
  SPIEL_CHECK_EQ(dominion_state->players[0].deck.size(), 0);
  SPIEL_CHECK_EQ(dominion_state->players[0].discard.size(), 2);
  SPIEL_CHECK_TRUE(dominion_state->IsChanceNode());

  // Apply action to handle shuffle
  dominion_state->ApplyAction(0);
  // After shuffle and drawing remaining 2 cards
  SPIEL_CHECK_EQ(dominion_state->players[0].hand.size(), 3);
  SPIEL_CHECK_EQ(dominion_state->players[0].deck.size(), 0);
  SPIEL_CHECK_EQ(dominion_state->players[0].discard.size(), 0);
  // Verify Smithy is in playing area
  SPIEL_CHECK_EQ(dominion_state->players[0].playing_area.size(), 1);
  SPIEL_CHECK_EQ(dominion_state->players[0].playing_area[0]->name, "Smithy");
}

void CouncilRoomTests() {
  GameParameters params;
  std::shared_ptr<const Game> game = LoadGame("dominion", params);
  std::unique_ptr<State> state = game->NewInitialState();
  DominionState *dominion_state = static_cast<DominionState *>(state.get());

  // Set up specific states for both players
  // Player 0 (active player):
  dominion_state->players[0].hand.clear();
  dominion_state->players[0].deck.clear();
  dominion_state->players[0].discard.clear();
  // Add Council Room to hand
  dominion_state->players[0].hand.push_back(card_registry::get("Council Room"));
  // Add one card to deck
  dominion_state->players[0].deck.push_back(card_registry::get("Copper"));
  // Add three cards to discard (for remaining 3 draws after shuffle)
  dominion_state->players[0].discard.push_back(card_registry::get("Silver"));
  dominion_state->players[0].discard.push_back(card_registry::get("Silver"));
  dominion_state->players[0].discard.push_back(card_registry::get("Silver"));

  // Player 1:
  dominion_state->players[1].hand.clear();
  dominion_state->players[1].deck.clear();
  dominion_state->players[1].discard.clear();
  // Add one card to discard (will need to shuffle to draw)
  dominion_state->players[1].discard.push_back(card_registry::get("Duchy"));

  // Play Council Room (index 0 in hand)
  dominion_state->PlayCard(0);

  // Check initial state after first draw but before shuffle
  SPIEL_CHECK_EQ(dominion_state->players[0].hand.size(), 1);
  SPIEL_CHECK_EQ(dominion_state->players[0].hand[0]->name, "Copper");
  SPIEL_CHECK_EQ(dominion_state->players[0].deck.size(), 0);
  SPIEL_CHECK_EQ(dominion_state->players[0].discard.size(), 3);
  // Player 1 shouldn't have drawn yet
  SPIEL_CHECK_EQ(dominion_state->players[1].hand.size(), 0);
  SPIEL_CHECK_TRUE(dominion_state->IsChanceNode());

  // Apply action to handle player 0's shuffle
  dominion_state->ApplyAction(0);
  // After shuffle and drawing remaining 3 cards for player 0
  SPIEL_CHECK_EQ(dominion_state->players[0].hand.size(), 4);
  SPIEL_CHECK_EQ(dominion_state->players[0].deck.size(), 0);
  SPIEL_CHECK_EQ(dominion_state->players[0].discard.size(), 0);
  // Player 1 still shouldn't have drawn, we only did player 0's shuffle
  SPIEL_CHECK_EQ(dominion_state->players[1].hand.size(), 0);
  SPIEL_CHECK_TRUE(dominion_state->IsChanceNode());

  // Apply action to handle player 1's shuffle
  dominion_state->ApplyAction(0);
  // Player 1 should now have drawn from their shuffled discard
  SPIEL_CHECK_EQ(dominion_state->players[1].hand.size(), 1);
  SPIEL_CHECK_EQ(dominion_state->players[1].hand[0]->name, "Duchy");
  SPIEL_CHECK_EQ(dominion_state->players[1].deck.size(), 0);
  SPIEL_CHECK_EQ(dominion_state->players[1].discard.size(), 0);
  // Verify Council Room is in playing area
  SPIEL_CHECK_EQ(dominion_state->players[0].playing_area.size(), 1);
  SPIEL_CHECK_EQ(dominion_state->players[0].playing_area[0]->name,
                 "Council Room");
  // Verify buy was added
  SPIEL_CHECK_EQ(dominion_state->n_buys, 2);
}

void WorkshopTests() {
  GameParameters params;
  std::shared_ptr<const Game> game = LoadGame("dominion", params);
  std::unique_ptr<State> state = game->NewInitialState();
  DominionState *dominion_state = static_cast<DominionState *>(state.get());

  // Set up initial state
  dominion_state->players[0].hand.clear();
  dominion_state->players[0].hand.push_back(card_registry::get("Workshop"));
  dominion_state->players[0].deck.clear();
  dominion_state->players[0].discard.clear();

  // Play Workshop (index 0 in hand)
  dominion_state->PlayCard(0);

  // Should be in choice state with legal actions for cards costing 4 or less
  SPIEL_CHECK_FALSE(dominion_state->IsChanceNode());
  SPIEL_CHECK_EQ(dominion_state->CurrentPlayer(), 0);
  std::vector<Action> legal_actions = dominion_state->LegalActions();
  // Should at least be copper, silver, estate, workshop
  SPIEL_CHECK_GE(legal_actions.size(), 4);
  for (Action action : legal_actions) {
    DominionAction choice = DominionAction::FromAction(action);
    SPIEL_CHECK_EQ(choice.type, DominionAction::Type::kSelectSupplyCard);
    SPIEL_CHECK_LE(card_registry::get(choice.index)->cost, 4);
  }

  // Choose to gain a Silver (costs 3)
  Action gain_silver = DominionAction(DominionAction::Type::kSelectSupplyCard,
                                      card_registry::get_id("Silver"))
                           .ToAction();
  dominion_state->ApplyAction(gain_silver);

  // Verify Silver was gained to discard
  SPIEL_CHECK_EQ(dominion_state->players[0].discard.size(), 1);
  SPIEL_CHECK_EQ(dominion_state->players[0].discard[0]->name, "Silver");
  // Verify Workshop is in playing area
  SPIEL_CHECK_EQ(dominion_state->players[0].playing_area.size(), 1);
  SPIEL_CHECK_EQ(dominion_state->players[0].playing_area[0]->name, "Workshop");
}

void GameOverTests() {
  card_registry::init();
  GameParameters params;
  std::shared_ptr<const Game> game = LoadGame("dominion", params);
  std::unique_ptr<State> state = game->NewInitialState();
  DominionState *dominion_state = static_cast<DominionState *>(state.get());

  // Game should not be over initially
  SPIEL_CHECK_FALSE(dominion_state->IsGameOver());

  // Game should be over if Province pile is empty
  auto province_it = std::find_if(dominion_state->supply_counts.begin(),
                                  dominion_state->supply_counts.end(),
                                  [](int count) { return count == 0; });
  if (province_it != dominion_state->supply_counts.end()) {
    *province_it = 0;
    SPIEL_CHECK_TRUE(dominion_state->IsGameOver());
  }

  // Game should be over if 3 supply piles are empty
  int empty_piles = 0;
  for (size_t i = 0; i < 3 && i < dominion_state->supply_counts.size(); ++i) {
    dominion_state->supply_counts[i] = 0;
    empty_piles++;
  }
  SPIEL_CHECK_TRUE(dominion_state->IsGameOver());
}

void BasicDominionTests() {
  testing::LoadGameTest("dominion");
  std::shared_ptr<const Game> game = LoadGame("dominion");
  testing::RandomSimTest(*game, /*num_sims=*/10);
}

}  // namespace
}  // namespace dominion
}  // namespace open_spiel

int main(int argc, char **argv) {
  open_spiel::dominion::PlayerStateTests();
  open_spiel::dominion::GameStateTests();
  open_spiel::dominion::SmithyTests();
  open_spiel::dominion::CouncilRoomTests();
  open_spiel::dominion::WorkshopTests();
  open_spiel::dominion::GameOverTests();
  open_spiel::dominion::BasicDominionTests();
}
