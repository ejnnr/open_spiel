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

  // Check that drawing a card triggers a chance node
  SPIEL_CHECK_TRUE(dominion_state->IsChanceNode());
  SPIEL_CHECK_EQ(dominion_state->players[0].deck.size(), 10);
  SPIEL_CHECK_EQ(dominion_state->players[0].hand.size(), 0);
  SPIEL_CHECK_EQ(dominion_state->players[0].discard.size(), 0);
  dominion_state->ApplyAction(0);
  SPIEL_CHECK_EQ(dominion_state->players[0].deck.size(), 9);
  SPIEL_CHECK_EQ(dominion_state->players[0].hand.size(), 1);
  SPIEL_CHECK_EQ(dominion_state->players[0].discard.size(), 0);
  SPIEL_CHECK_TRUE(dominion_state->IsChanceNode());
  dominion_state->SampleAllChanceNodes();
  SPIEL_CHECK_EQ(dominion_state->players[0].deck.size(), 5);
  SPIEL_CHECK_EQ(dominion_state->players[0].hand.size(), 5);
  SPIEL_CHECK_EQ(dominion_state->players[0].discard.size(), 0);
  SPIEL_CHECK_FALSE(dominion_state->IsChanceNode());

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
  dominion_state->SampleAllChanceNodes();

  SPIEL_CHECK_EQ(dominion_state->phase, Phase::Action);
  SPIEL_CHECK_EQ(dominion_state->CurrentPlayer(), 1);  // Next player's turn
  dominion_state->ApplyAction(0);
  SPIEL_CHECK_EQ(dominion_state->phase, Phase::Buy);
  SPIEL_CHECK_EQ(dominion_state->CurrentPlayer(), 1);
  dominion_state->ApplyAction(0);
  dominion_state->SampleAllChanceNodes();

  SPIEL_CHECK_EQ(dominion_state->phase, Phase::Action);
  SPIEL_CHECK_EQ(dominion_state->CurrentPlayer(), 0);
  dominion_state->ApplyAction(0);
  SPIEL_CHECK_EQ(dominion_state->phase, Phase::Buy);
  SPIEL_CHECK_EQ(dominion_state->CurrentPlayer(), 0);
}

void SmithyTests() {
  GameParameters params;
  std::shared_ptr<const Game> game = LoadGame("dominion", params);
  std::unique_ptr<State> state = game->NewInitialState();
  DominionState *dominion_state = static_cast<DominionState *>(state.get());

  dominion_state->SampleAllChanceNodes();
  dominion_state->players[0].hand.clear();
  dominion_state->players[0].deck.clear();
  dominion_state->players[0].discard.clear();

  dominion_state->players[0].hand.push_back(card_registry::get("Smithy"));
  dominion_state->players[0].deck.push_back(card_registry::get("Copper"));
  dominion_state->players[0].discard.push_back(card_registry::get("Silver"));
  dominion_state->players[0].discard.push_back(card_registry::get("Silver"));

  // Play Smithy (index 0 in hand)
  dominion_state->PlayCard(0);
  SPIEL_CHECK_EQ(dominion_state->n_actions, 0);
  SPIEL_CHECK_TRUE(dominion_state->IsChanceNode());
  SPIEL_CHECK_EQ(dominion_state->players[0].hand.size(), 0);
  dominion_state->ApplyAction(0);
  // First card should be drawn from deck
  SPIEL_CHECK_TRUE(dominion_state->IsChanceNode());
  SPIEL_CHECK_EQ(dominion_state->players[0].hand.size(), 1);
  SPIEL_CHECK_EQ(dominion_state->players[0].hand[0]->name, "Copper");
  // We've shuffled but haven't drawn yet
  SPIEL_CHECK_EQ(dominion_state->players[0].deck.size(), 2);
  SPIEL_CHECK_EQ(dominion_state->players[0].discard.size(), 0);

  // Apply action to draw next cards
  dominion_state->ApplyAction(0);
  dominion_state->ApplyAction(0);
  SPIEL_CHECK_FALSE(dominion_state->IsChanceNode());
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
  dominion_state->SampleAllChanceNodes();
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
  SPIEL_CHECK_TRUE(dominion_state->IsChanceNode());

  // Check state after first draw
  dominion_state->ApplyAction(0);
  SPIEL_CHECK_TRUE(dominion_state->IsChanceNode());
  SPIEL_CHECK_EQ(dominion_state->players[0].hand.size(), 1);
  SPIEL_CHECK_EQ(dominion_state->players[0].hand[0]->name, "Copper");
  SPIEL_CHECK_EQ(dominion_state->players[0].deck.size(), 3);
  SPIEL_CHECK_EQ(dominion_state->players[0].discard.size(), 0);
  // Player 1 shouldn't have drawn yet
  SPIEL_CHECK_EQ(dominion_state->players[1].hand.size(), 0);

  // Draw remaining 3 cards
  dominion_state->ApplyAction(0);
  dominion_state->ApplyAction(0);
  dominion_state->ApplyAction(0);
  SPIEL_CHECK_EQ(dominion_state->players[0].hand.size(), 4);
  SPIEL_CHECK_EQ(dominion_state->players[0].deck.size(), 0);
  SPIEL_CHECK_EQ(dominion_state->players[0].discard.size(), 0);
  // Player 1 still shouldn't have drawn
  SPIEL_CHECK_TRUE(dominion_state->IsChanceNode());
  SPIEL_CHECK_EQ(dominion_state->players[1].hand.size(), 0);

  // Apply action to handle player 1's draw
  dominion_state->ApplyAction(0);
  // Player 1 should now have drawn from their shuffled discard
  SPIEL_CHECK_EQ(dominion_state->players[1].hand.size(), 1);
  SPIEL_CHECK_EQ(dominion_state->players[1].hand[0]->name, "Duchy");
  SPIEL_CHECK_EQ(dominion_state->players[1].deck.size(), 0);
  SPIEL_CHECK_EQ(dominion_state->players[1].discard.size(), 0);
  SPIEL_CHECK_FALSE(dominion_state->IsChanceNode());
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
  dominion_state->SampleAllChanceNodes();
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
    DominionAction choice = DominionAction(action);
    SPIEL_CHECK_EQ(choice.type, ActionType::kSelectSupplyCard);
    SPIEL_CHECK_LE(card_registry::get(choice.index)->cost, 4);
  }

  // Choose to gain a Silver (costs 3)
  Action gain_silver = GetActionId(ActionType::kSelectSupplyCard,
                                   card_registry::get_id("Silver"));
  dominion_state->ApplyAction(gain_silver);

  // Verify Silver was gained to discard
  SPIEL_CHECK_EQ(dominion_state->players[0].discard.size(), 1);
  SPIEL_CHECK_EQ(dominion_state->players[0].discard[0]->name, "Silver");
  // Verify Workshop is in playing area
  SPIEL_CHECK_EQ(dominion_state->players[0].playing_area.size(), 1);
  SPIEL_CHECK_EQ(dominion_state->players[0].playing_area[0]->name, "Workshop");
}

void ChapelTests() {
  GameParameters params;
  std::shared_ptr<const Game> game = LoadGame("dominion", params);
  std::unique_ptr<State> state = game->NewInitialState();
  DominionState *dominion_state = static_cast<DominionState *>(state.get());

  // Set up initial state
  dominion_state->SampleAllChanceNodes();
  dominion_state->players[0].hand.clear();
  dominion_state->players[0].hand.push_back(card_registry::get("Chapel"));
  dominion_state->players[0].hand.push_back(card_registry::get("Copper"));
  dominion_state->players[0].hand.push_back(card_registry::get("Estate"));
  dominion_state->players[0].deck.clear();
  dominion_state->players[0].discard.clear();

  // Play Chapel (index 0 in hand)
  dominion_state->PlayCard(0);

  // Should be in choice state with legal actions for each card in hand plus end
  SPIEL_CHECK_FALSE(dominion_state->IsChanceNode());
  SPIEL_CHECK_EQ(dominion_state->CurrentPlayer(), 0);
  std::vector<Action> legal_actions = dominion_state->LegalActions();
  // Should be 3 actions - trash copper (200), trash estate (201), or end (0)
  SPIEL_CHECK_EQ(legal_actions.size(), 3);
  SPIEL_CHECK_EQ(legal_actions[0], 0);
  SPIEL_CHECK_EQ(legal_actions[1], 200);
  SPIEL_CHECK_EQ(legal_actions[2], 201);

  // Choose to trash the Copper
  dominion_state->ApplyAction(200);

  // Verify Copper was trashed
  SPIEL_CHECK_EQ(dominion_state->trash.size(), 1);
  SPIEL_CHECK_EQ(dominion_state->trash[0]->name, "Copper");
  SPIEL_CHECK_EQ(dominion_state->players[0].hand.size(), 1);
  SPIEL_CHECK_FALSE(dominion_state->IsChanceNode());
  SPIEL_CHECK_EQ(dominion_state->LegalActions().size(), 2);
  SPIEL_CHECK_EQ(dominion_state->LegalActions()[0], 0);
  SPIEL_CHECK_EQ(dominion_state->LegalActions()[1], 200);

  // Choose to trash the Estate
  dominion_state->ApplyAction(200);

  // Verify Estate was also trashed
  SPIEL_CHECK_EQ(dominion_state->trash.size(), 2);
  SPIEL_CHECK_EQ(dominion_state->trash[1]->name, "Estate");
  SPIEL_CHECK_EQ(dominion_state->players[0].hand.size(), 0);
  SPIEL_CHECK_FALSE(dominion_state->IsChanceNode());
  SPIEL_CHECK_EQ(dominion_state->LegalActions().size(), 1);
  SPIEL_CHECK_EQ(dominion_state->LegalActions()[0], 0);

  // Choose to end (no more cards to trash)
  dominion_state->ApplyAction(0);
  SPIEL_CHECK_FALSE(dominion_state->IsChanceNode());
}

void ThroneRoomTests() {
  GameParameters params;
  std::shared_ptr<const Game> game = LoadGame("dominion", params);
  std::unique_ptr<State> state = game->NewInitialState();
  DominionState *dominion_state = static_cast<DominionState *>(state.get());

  // Set up initial state
  dominion_state->SampleAllChanceNodes();
  dominion_state->players[0].hand.clear();
  dominion_state->players[0].hand.push_back(card_registry::get("Throne Room"));
  dominion_state->players[0].hand.push_back(card_registry::get("Village"));
  dominion_state->players[0].deck.clear();
  dominion_state->players[0].deck.push_back(card_registry::get("Copper"));
  dominion_state->players[0].deck.push_back(card_registry::get("Copper"));
  dominion_state->players[0].deck.push_back(card_registry::get("Copper"));
  dominion_state->players[0].discard.clear();

  // Play Throne Room
  dominion_state->PlayCard(0);

  // Should be able to select Village or end
  SPIEL_CHECK_EQ(dominion_state->CurrentPlayer(), 0);
  std::vector<Action> legal_actions = dominion_state->LegalActions();
  SPIEL_CHECK_EQ(legal_actions.size(), 2);  // End or Village
  SPIEL_CHECK_EQ(legal_actions[0], 0);      // End
  SPIEL_CHECK_EQ(legal_actions[1], 200);    // Select Village

  // Choose Village
  dominion_state->ApplyAction(200);
  dominion_state->SampleAllChanceNodes();

  // Village should have been played twice, for +2 cards and +4 actions
  SPIEL_CHECK_EQ(dominion_state->players[0].hand.size(), 2);
  SPIEL_CHECK_EQ(dominion_state->n_actions, 4);

  // Test Throne Room on Throne Room
  dominion_state->players[0].hand.clear();
  dominion_state->players[0].hand.push_back(
      card_registry::get("Throne Room"));  // First TR
  dominion_state->players[0].hand.push_back(
      card_registry::get("Throne Room"));  // Second TR
  dominion_state->players[0].hand.push_back(card_registry::get("Village"));
  dominion_state->players[0].hand.push_back(card_registry::get("Village"));
  dominion_state->players[0].deck.clear();
  dominion_state->players[0].deck.push_back(card_registry::get("Copper"));
  dominion_state->players[0].deck.push_back(card_registry::get("Copper"));
  dominion_state->players[0].deck.push_back(card_registry::get("Copper"));
  dominion_state->players[0].deck.push_back(card_registry::get("Copper"));
  dominion_state->players[0].discard.clear();
  dominion_state->n_actions = 1;

  // Play first Throne Room
  dominion_state->PlayCard(0);

  // Choose second Throne Room
  dominion_state->ApplyAction(200);
  dominion_state->SampleAllChanceNodes();
  // only Villages left in hand, nothing drawn yet
  SPIEL_CHECK_EQ(dominion_state->players[0].hand.size(), 2);

  // Choose Village for first doubled Throne Room
  dominion_state->ApplyAction(200);
  dominion_state->SampleAllChanceNodes();
  // drew 2 coppers, 1 village left:
  SPIEL_CHECK_EQ(dominion_state->players[0].hand.size(), 3);
  SPIEL_CHECK_EQ(dominion_state->n_actions, 4);

  // Choose Village for second doubled Throne Room
  dominion_state->ApplyAction(200);
  dominion_state->SampleAllChanceNodes();
  SPIEL_CHECK_EQ(dominion_state->players[0].hand.size(), 4);  // drew 4 coppers
  SPIEL_CHECK_EQ(dominion_state->n_actions, 8);
}

void GameOverTests() {
  GameParameters params;
  std::shared_ptr<const Game> game = LoadGame("dominion", params);
  std::unique_ptr<State> state = game->NewInitialState();
  DominionState *dominion_state = static_cast<DominionState *>(state.get());

  // Game should not be over initially
  SPIEL_CHECK_FALSE(dominion_state->IsGameOver());

  // Game should be over if Province pile is empty
  size_t province_index = card_registry::get_id("Province");
  dominion_state->supply_counts[province_index] = 0;
  SPIEL_CHECK_TRUE(dominion_state->IsGameOver());

  // Game should be over if 3 supply piles are empty
  dominion_state->supply_counts[province_index] = 8;
  SPIEL_CHECK_TRUE(dominion_state->supply_counts.size() >= 4);
  SPIEL_CHECK_FALSE(dominion_state->IsGameOver());
  int emptied_piles = 0;
  for (size_t i = 0; i < dominion_state->supply_counts.size(); ++i) {
    if (i != province_index) {
      dominion_state->supply_counts[i] = 0;
      emptied_piles++;
    }
    if (emptied_piles == 3) break;
  }
  SPIEL_CHECK_EQ(emptied_piles, 3);
  SPIEL_CHECK_TRUE(dominion_state->IsGameOver());
}

void CellarTests() {
  GameParameters params;
  std::shared_ptr<const Game> game = LoadGame("dominion", params);
  std::unique_ptr<State> state = game->NewInitialState();
  DominionState *dominion_state = static_cast<DominionState *>(state.get());

  // Set up initial state
  dominion_state->SampleAllChanceNodes();
  dominion_state->players[0].hand.clear();
  dominion_state->players[0].hand.push_back(card_registry::get("Cellar"));
  dominion_state->players[0].hand.push_back(card_registry::get("Copper"));
  dominion_state->players[0].hand.push_back(card_registry::get("Estate"));
  dominion_state->players[0].deck.clear();
  dominion_state->players[0].deck.push_back(card_registry::get("Silver"));
  dominion_state->players[0].deck.push_back(card_registry::get("Gold"));
  dominion_state->players[0].discard.clear();

  // Play Cellar (index 0 in hand)
  dominion_state->PlayCard(0);
  SPIEL_CHECK_EQ(dominion_state->n_actions, 1);  // +1 action

  // Should be in choice state with legal actions for each card in hand plus end
  SPIEL_CHECK_FALSE(dominion_state->IsChanceNode());
  SPIEL_CHECK_EQ(dominion_state->CurrentPlayer(), 0);
  std::vector<Action> legal_actions = dominion_state->LegalActions();
  // Should be 3 actions - discard copper (200), discard estate (201), or end
  // (0)
  SPIEL_CHECK_EQ(legal_actions.size(), 3);
  SPIEL_CHECK_EQ(legal_actions[0], 0);
  SPIEL_CHECK_EQ(legal_actions[1], 200);
  SPIEL_CHECK_EQ(legal_actions[2], 201);

  // Choose to discard both cards
  dominion_state->ApplyAction(200);  // Discard Copper
  dominion_state->ApplyAction(200);  // Discard Estate
  dominion_state->ApplyAction(0);    // End discarding

  SPIEL_CHECK_EQ(dominion_state->players[0].hand.size(), 0);
  SPIEL_CHECK_EQ(dominion_state->players[0].discard.size(), 2);
  SPIEL_CHECK_EQ(dominion_state->players[0].discard[0]->name, "Copper");
  SPIEL_CHECK_EQ(dominion_state->players[0].discard[1]->name, "Estate");
  SPIEL_CHECK_EQ(dominion_state->players[0].deck.size(), 2);

  // Should now be in chance state for drawing 2 cards
  SPIEL_CHECK_TRUE(dominion_state->IsChanceNode());
  dominion_state->ApplyAction(0);  // Draw Silver
  dominion_state->ApplyAction(0);  // Draw Gold

  // Verify final state
  SPIEL_CHECK_EQ(dominion_state->players[0].hand.size(), 2);
  SPIEL_CHECK_EQ(dominion_state->players[0].hand[0]->name, "Silver");
  SPIEL_CHECK_EQ(dominion_state->players[0].hand[1]->name, "Gold");
  SPIEL_CHECK_EQ(dominion_state->players[0].discard.size(), 2);
  SPIEL_CHECK_EQ(dominion_state->players[0].discard[0]->name, "Copper");
  SPIEL_CHECK_EQ(dominion_state->players[0].discard[1]->name, "Estate");
  SPIEL_CHECK_EQ(dominion_state->players[0].deck.size(), 0);
  SPIEL_CHECK_FALSE(dominion_state->IsChanceNode());
}

void MoneylenderTests() {
  GameParameters params;
  std::shared_ptr<const Game> game = LoadGame("dominion", params);
  std::unique_ptr<State> state = game->NewInitialState();
  DominionState *dominion_state = static_cast<DominionState *>(state.get());

  dominion_state->SampleAllChanceNodes();
  dominion_state->players[0].hand.clear();
  dominion_state->players[0].hand.push_back(card_registry::get("Copper"));
  dominion_state->players[0].hand.push_back(card_registry::get("Moneylender"));
  dominion_state->players[0].hand.push_back(card_registry::get("Estate"));
  dominion_state->players[0].hand.push_back(card_registry::get("Copper"));
  dominion_state->players[0].deck.clear();
  dominion_state->players[0].discard.clear();

  // Play Moneylender
  dominion_state->PlayCard(1);

  SPIEL_CHECK_EQ(dominion_state->n_actions, 0);
  SPIEL_CHECK_EQ(dominion_state->n_coins, 0);
  SPIEL_CHECK_EQ(dominion_state->players[0].hand.size(), 3);
  SPIEL_CHECK_EQ(dominion_state->players[0].hand[0]->name, "Copper");
  SPIEL_CHECK_EQ(dominion_state->players[0].hand[1]->name, "Estate");
  SPIEL_CHECK_EQ(dominion_state->players[0].hand[2]->name, "Copper");
  std::vector<Action> legal_actions = dominion_state->LegalActions();
  SPIEL_CHECK_EQ(legal_actions.size(), 2);
  SPIEL_CHECK_EQ(legal_actions[0], 0);
  SPIEL_CHECK_EQ(legal_actions[1], 1);

  // Choose to trash the Copper
  dominion_state->ApplyAction(1);
  SPIEL_CHECK_EQ(dominion_state->n_coins, 3);
  SPIEL_CHECK_EQ(dominion_state->players[0].hand.size(), 2);
  SPIEL_CHECK_EQ(dominion_state->players[0].hand[0]->name, "Estate");
  SPIEL_CHECK_EQ(dominion_state->players[0].hand[1]->name, "Copper");
  SPIEL_CHECK_EQ(dominion_state->players[0].discard.size(), 0);
  SPIEL_CHECK_EQ(dominion_state->trash.size(), 1);
  SPIEL_CHECK_EQ(dominion_state->trash[0]->name, "Copper");

  legal_actions = dominion_state->LegalActions();
  // Only legal action should be to end action phase
  SPIEL_CHECK_EQ(legal_actions.size(), 1);
  SPIEL_CHECK_EQ(legal_actions[0], 0);

  // Reset everything to test choosing not to trash
  dominion_state->SampleAllChanceNodes();
  dominion_state->n_actions = 1;
  dominion_state->n_coins = 0;
  dominion_state->players[0].hand.clear();
  dominion_state->players[0].hand.push_back(card_registry::get("Copper"));
  dominion_state->players[0].hand.push_back(card_registry::get("Moneylender"));
  dominion_state->players[0].hand.push_back(card_registry::get("Estate"));
  dominion_state->players[0].hand.push_back(card_registry::get("Copper"));
  dominion_state->players[0].deck.clear();
  dominion_state->players[0].discard.clear();
  dominion_state->players[0].playing_area.clear();
  dominion_state->trash.clear();
  // Play Moneylender
  dominion_state->PlayCard(1);

  // Test not trashing the Copper
  dominion_state->ApplyAction(0);
  SPIEL_CHECK_EQ(dominion_state->n_coins, 0);
  SPIEL_CHECK_EQ(dominion_state->players[0].hand.size(), 3);
  SPIEL_CHECK_EQ(dominion_state->players[0].hand[0]->name, "Copper");
  SPIEL_CHECK_EQ(dominion_state->players[0].hand[1]->name, "Estate");
  SPIEL_CHECK_EQ(dominion_state->players[0].hand[2]->name, "Copper");

  // Test playing when no Copper is in hand
  dominion_state->n_actions = 1;
  dominion_state->n_coins = 0;
  dominion_state->players[0].hand.clear();
  dominion_state->players[0].hand.push_back(card_registry::get("Moneylender"));
  dominion_state->players[0].hand.push_back(card_registry::get("Estate"));
  dominion_state->players[0].hand.push_back(card_registry::get("Cellar"));
  dominion_state->PlayCard(0);
  SPIEL_CHECK_EQ(dominion_state->n_coins, 0);
  // Make sure we're back in the normal action phase, no pending choices.
  // Hack to make Cellar playable:
  dominion_state->n_actions = 1;
  legal_actions = dominion_state->LegalActions();
  SPIEL_CHECK_EQ(legal_actions.size(), 2);
  SPIEL_CHECK_EQ(legal_actions[0], 0);
  SPIEL_CHECK_EQ(legal_actions[1], 201);
}

void RemodelTests() {
  std::shared_ptr<const Game> game = LoadGame("dominion");
  std::unique_ptr<State> state = game->NewInitialState();
  auto dominion_state =
      static_cast<open_spiel::dominion::DominionState *>(state.get());
  dominion_state->SampleAllChanceNodes();

  // Set up test state
  dominion_state->players[0].hand.clear();
  dominion_state->players[0].deck.clear();
  dominion_state->players[0].hand.push_back(card_registry::get("Remodel"));
  dominion_state->players[0].hand.push_back(card_registry::get("Estate"));
  dominion_state->players[0].hand.push_back(card_registry::get("Copper"));

  // Play Remodel
  dominion_state->PlayCard(0);

  // Choose Estate to trash (costs 2)
  dominion_state->ApplyAction(GetActionId(ActionType::kSelectHandCard, 0));

  // Should be able to at least gain Copper, Estate, Silver, Remodel
  std::vector<Action> legal_actions = dominion_state->LegalActions();
  SPIEL_CHECK_GE(legal_actions.size(), 4);
  for (Action action : legal_actions) {
    DominionAction choice = DominionAction(action);
    SPIEL_CHECK_EQ(choice.type, ActionType::kSelectSupplyCard);
    SPIEL_CHECK_LE(card_registry::get(choice.index)->cost, 4);
  }

  // Choose Silver to gain (costs 3)
  dominion_state->ApplyAction(GetActionId(ActionType::kSelectSupplyCard, 1));

  // Verify Estate was trashed
  SPIEL_CHECK_EQ(dominion_state->trash.size(), 1);
  SPIEL_CHECK_EQ(dominion_state->trash[0]->name, "Estate");

  // Verify Silver was gained
  SPIEL_CHECK_EQ(dominion_state->players[0].discard.size(), 1);
  SPIEL_CHECK_EQ(dominion_state->players[0].discard[0]->name, "Silver");

  // Test playing with empty hand
  dominion_state->n_actions = 1;
  dominion_state->players[0].hand.clear();
  dominion_state->players[0].hand.push_back(card_registry::get("Remodel"));
  dominion_state->PlayCard(0);
  // Should be back in action phase with no pending choices
  legal_actions = dominion_state->LegalActions();
  SPIEL_CHECK_EQ(legal_actions.size(), 1);
  SPIEL_CHECK_EQ(legal_actions[0], 0);
}

void BasicDominionTests() {
  testing::LoadGameTest("dominion");
  std::shared_ptr<const Game> game = LoadGame("dominion(small_supply=true)");
  testing::RandomSimTest(*game, /*num_sims=*/5);
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
  open_spiel::dominion::ChapelTests();
  open_spiel::dominion::CellarTests();
  open_spiel::dominion::MoneylenderTests();
  open_spiel::dominion::RemodelTests();
  open_spiel::dominion::ThroneRoomTests();
  open_spiel::dominion::BasicDominionTests();
  open_spiel::dominion::GameOverTests();
}
