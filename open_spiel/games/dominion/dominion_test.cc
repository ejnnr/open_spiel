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
  auto &player = dominion_state->players[0];
  player.ClearAll();

  player.hand.push_back(card_registry::get("Smithy"));
  player.SetupUnknownDeck({card_registry::get("Copper")});
  player.discard = {card_registry::get("Silver"), card_registry::get("Silver")};

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

  dominion_state->SampleAllChanceNodes();

  // Player 0 (active player):
  auto &player0 = dominion_state->players[0];
  player0.ClearAll();

  player0.hand.push_back(card_registry::get("Council Room"));
  player0.SetupUnknownDeck({card_registry::get("Copper")});

  // Add three cards to discard (for remaining 3 draws after shuffle)
  player0.discard = {card_registry::get("Silver"), card_registry::get("Silver"),
                     card_registry::get("Silver")};

  // Player 1:
  auto &player1 = dominion_state->players[1];
  player1.ClearAll();
  // Add one card to discard (will need to shuffle to draw)
  player1.discard.push_back(card_registry::get("Duchy"));

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
  auto &player = dominion_state->players[0];
  player.ClearAll();
  player.hand = {card_registry::get("Chapel"), card_registry::get("Copper"),
                 card_registry::get("Estate")};

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
  auto &player = dominion_state->players[0];
  player.ClearAll();
  player.hand = {card_registry::get("Throne Room"),
                 card_registry::get("Village")};

  player.SetupUnknownDeck({card_registry::get("Copper"),
                           card_registry::get("Copper"),
                           card_registry::get("Copper")});

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
  player.ClearAll();
  player.hand = {card_registry::get("Throne Room"),  // First TR
                 card_registry::get("Throne Room"),  // Second TR
                 card_registry::get("Village"), card_registry::get("Village")};

  player.SetupUnknownDeck(
      {card_registry::get("Copper"), card_registry::get("Copper"),
       card_registry::get("Copper"), card_registry::get("Copper")});
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

void LibraryTests() {
  GameParameters params;
  std::shared_ptr<const Game> game = LoadGame("dominion", params);
  std::unique_ptr<State> state = game->NewInitialState();
  DominionState *dominion_state = static_cast<DominionState *>(state.get());

  // Set up initial state
  dominion_state->SampleAllChanceNodes();
  auto &player = dominion_state->players[0];
  player.ClearAll();
  player.hand = {card_registry::get("Library")};

  // Set up known deck
  std::vector<Card *> deck = {
      card_registry::get("Village"),  // Action
      card_registry::get("Copper"),   // Non-action
      card_registry::get("Market"),   // Action
      card_registry::get("Silver")    // Non-action
  };
  player.SetupUnknownDeck(deck);

  // Play Library
  dominion_state->PlayCard(0);

  // Should be in chance state for first draw
  SPIEL_CHECK_TRUE(dominion_state->IsChanceNode());
  dominion_state->ApplyAction(0);  // Draw Village

  // Should be in decision state for whether to set aside Village
  SPIEL_CHECK_FALSE(dominion_state->IsChanceNode());
  SPIEL_CHECK_EQ(dominion_state->CurrentPlayer(), 0);
  std::vector<Action> legal_actions = dominion_state->LegalActions();
  SPIEL_CHECK_EQ(legal_actions.size(), 2);  // Keep (0) or set aside (1)

  // Choose to set aside Village
  dominion_state->ApplyAction(1);
  // Check that card isn't discarded immediately (which would lead to incorrect
  // shuffle behavior)
  SPIEL_CHECK_EQ(dominion_state->players[0].discard.size(), 0);

  // Draw Copper (non-action, automatically kept)
  SPIEL_CHECK_TRUE(dominion_state->IsChanceNode());
  dominion_state->ApplyAction(0);

  // Draw Market
  SPIEL_CHECK_TRUE(dominion_state->IsChanceNode());
  dominion_state->ApplyAction(0);

  // Choose to keep Market
  dominion_state->ApplyAction(0);

  // Draw Silver (non-action, automatically kept)
  SPIEL_CHECK_TRUE(dominion_state->IsChanceNode());
  dominion_state->ApplyAction(0);

  // Verify final state
  // Copper, Market, Silver:
  SPIEL_CHECK_EQ(dominion_state->players[0].hand.size(), 3);
  SPIEL_CHECK_EQ(dominion_state->players[0].discard.size(), 1);  // Village
  SPIEL_CHECK_EQ(dominion_state->players[0].discard[0]->name, "Village");
}

void CellarTests() {
  GameParameters params;
  std::shared_ptr<const Game> game = LoadGame("dominion", params);
  std::unique_ptr<State> state = game->NewInitialState();
  DominionState *dominion_state = static_cast<DominionState *>(state.get());

  // Set up initial state
  dominion_state->SampleAllChanceNodes();
  auto &player = dominion_state->players[0];
  player.ClearAll();
  player.hand = {card_registry::get("Cellar"), card_registry::get("Copper"),
                 card_registry::get("Estate")};

  player.SetupUnknownDeck(
      {card_registry::get("Silver"), card_registry::get("Gold")});

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
  auto &player = dominion_state->players[0];
  player.ClearAll();
  player.hand = {card_registry::get("Copper"),
                 card_registry::get("Moneylender"),
                 card_registry::get("Estate"), card_registry::get("Copper")};

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
  player.ClearAll();
  player.hand = {card_registry::get("Copper"),
                 card_registry::get("Moneylender"),
                 card_registry::get("Estate"), card_registry::get("Copper")};
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
  player.ClearAll();
  player.hand = {card_registry::get("Moneylender"),
                 card_registry::get("Estate"), card_registry::get("Cellar")};
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
  GameParameters params;
  std::shared_ptr<const Game> game = LoadGame("dominion", params);
  std::unique_ptr<State> state = game->NewInitialState();
  DominionState *dominion_state = static_cast<DominionState *>(state.get());

  dominion_state->SampleAllChanceNodes();
  auto &player = dominion_state->players[0];
  player.ClearAll();
  player.hand = {card_registry::get("Remodel"), card_registry::get("Estate"),
                 card_registry::get("Copper")};

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

  // Get initial supply count for Silver
  size_t silver_id = card_registry::get_id("Silver");
  int initial_silver_count = dominion_state->supply_counts[silver_id];

  // Choose Silver to gain (costs 3)
  dominion_state->ApplyAction(
      GetActionId(ActionType::kSelectSupplyCard, silver_id));

  // Verify Estate was trashed
  SPIEL_CHECK_EQ(dominion_state->trash.size(), 1);
  SPIEL_CHECK_EQ(dominion_state->trash[0]->name, "Estate");

  // Verify Silver was gained
  SPIEL_CHECK_EQ(dominion_state->players[0].discard.size(), 1);
  SPIEL_CHECK_EQ(dominion_state->players[0].discard[0]->name, "Silver");
  // Verify supply count was decreased
  SPIEL_CHECK_EQ(dominion_state->supply_counts[silver_id],
                 initial_silver_count - 1);

  // Test playing with empty hand
  dominion_state->n_actions = 1;
  player.ClearAll();
  player.hand = {card_registry::get("Remodel")};
  dominion_state->PlayCard(0);
  // Should be back in action phase with no pending choices
  legal_actions = dominion_state->LegalActions();
  SPIEL_CHECK_EQ(legal_actions.size(), 1);
  SPIEL_CHECK_EQ(legal_actions[0], 0);
}

void GardensTests() {
  GameParameters params;
  std::shared_ptr<const Game> game = LoadGame("dominion", params);
  std::unique_ptr<State> state = game->NewInitialState();
  DominionState *dominion_state = static_cast<DominionState *>(state.get());

  // Set up initial state
  dominion_state->SampleAllChanceNodes();
  auto &player = dominion_state->players[0];
  player.ClearAll();

  // Test with 0 cards (just Gardens) -> 0 VP
  player.hand = {card_registry::get("Gardens")};
  SPIEL_CHECK_EQ(player.VpCount(), 0);

  // Test with 9 cards -> 0 VP
  std::vector<Card *> known_deck;
  for (int i = 0; i < 8; ++i) {
    known_deck.push_back(card_registry::get("Copper"));
  }
  player.SetupKnownDeck(known_deck);
  SPIEL_CHECK_EQ(player.VpCount(), 0);

  // Test with 10 cards -> 1 VP
  known_deck.push_back(card_registry::get("Copper"));
  player.SetupKnownDeck(known_deck);
  SPIEL_CHECK_EQ(player.VpCount(), 1);

  // Test with 19 cards -> 1 VP
  player.discard.clear();
  for (int i = 0; i < 9; ++i) {
    player.discard.push_back(card_registry::get("Copper"));
  }
  SPIEL_CHECK_EQ(player.VpCount(), 1);

  // Test with 20 cards -> 2 VP
  player.discard.push_back(card_registry::get("Copper"));
  SPIEL_CHECK_EQ(player.VpCount(), 2);

  // Test with cards spread across all zones
  player.ClearAll();

  // Add 25 cards total (2 VP):
  // - 5 in hand (including Gardens)
  // - 8 in deck
  // - 7 in discard
  // - 5 in playing area
  player.hand = {card_registry::get("Gardens"), card_registry::get("Copper"),
                 card_registry::get("Copper"), card_registry::get("Copper"),
                 card_registry::get("Copper")};

  known_deck.clear();
  for (int i = 0; i < 8; ++i) {
    known_deck.push_back(card_registry::get("Copper"));
  }
  player.SetupKnownDeck(known_deck);

  for (int i = 0; i < 7; ++i) {
    player.discard.push_back(card_registry::get("Copper"));
  }
  for (int i = 0; i < 5; ++i) {
    player.playing_area.push_back(card_registry::get("Copper"));
  }
  SPIEL_CHECK_EQ(player.VpCount(), 2);
}

void WitchTests() {
  GameParameters params;
  std::shared_ptr<const Game> game = LoadGame("dominion", params);
  std::unique_ptr<State> state = game->NewInitialState();
  DominionState *dominion_state = static_cast<DominionState *>(state.get());

  // Set up initial state
  dominion_state->SampleAllChanceNodes();
  auto &player0 = dominion_state->players[0];
  auto &player1 = dominion_state->players[1];

  player0.ClearAll();
  player0.hand = {card_registry::get("Witch")};
  std::vector<Card *> deck = {card_registry::get("Copper"),
                              card_registry::get("Silver")};
  player0.SetupUnknownDeck(deck);

  player1.ClearAll();

  // Check initial VP
  SPIEL_CHECK_EQ(dominion_state->players[0].VpCount(), 0);
  SPIEL_CHECK_EQ(dominion_state->players[1].VpCount(), 0);

  // Play Witch
  dominion_state->PlayCard(0);
  dominion_state->SampleAllChanceNodes();

  // Check that player 0 drew 2 cards
  SPIEL_CHECK_EQ(dominion_state->players[0].hand.size(), 2);

  // Check that player 1 got a curse
  SPIEL_CHECK_EQ(dominion_state->players[1].discard.size(), 1);
  SPIEL_CHECK_EQ(dominion_state->players[1].discard[0]->name, "Curse");
  SPIEL_CHECK_EQ(dominion_state->players[1].VpCount(), -1);

  // Empty curse pile and play witch again
  size_t curse_id = card_registry::get_id("Curse");
  dominion_state->supply_counts[curse_id] = 0;

  player0.ClearAll();
  player0.hand = {card_registry::get("Witch")};
  player0.SetupUnknownDeck(deck);

  // Play Witch with empty curse pile
  dominion_state->PlayCard(0);
  dominion_state->SampleAllChanceNodes();
  // Check that player 1 didn't get another curse
  SPIEL_CHECK_EQ(dominion_state->players[1].discard.size(), 1);
  SPIEL_CHECK_EQ(dominion_state->players[1].VpCount(), -1);
}

void KingdomSelectionTests() {
  // Test case-insensitive card names
  {
    GameParameters params;
    params["kingdom_cards"] = GameParameter("village,SMITHY,Market");
    std::shared_ptr<const Game> game = LoadGame("dominion", params);
    std::unique_ptr<State> state = game->NewInitialState();
    DominionState *dominion_state = static_cast<DominionState *>(state.get());

    // Check that specified cards are in supply
    SPIEL_CHECK_GT(
        dominion_state->supply_counts[card_registry::get_id("Village")], 0);
    SPIEL_CHECK_GT(
        dominion_state->supply_counts[card_registry::get_id("Smithy")], 0);
    SPIEL_CHECK_GT(
        dominion_state->supply_counts[card_registry::get_id("Market")], 0);
    // Check that unspecified action cards are not in supply
    SPIEL_CHECK_EQ(
        dominion_state->supply_counts[card_registry::get_id("Laboratory")], 0);
    SPIEL_CHECK_EQ(
        dominion_state->supply_counts[card_registry::get_id("Festival")], 0);
    // Check that basic cards are still in supply
    SPIEL_CHECK_GT(
        dominion_state->supply_counts[card_registry::get_id("Copper")], 0);
    SPIEL_CHECK_GT(
        dominion_state->supply_counts[card_registry::get_id("Estate")], 0);
  }

  // Test random kingdom selection
  {
    GameParameters params;
    params["random_kingdom"] = GameParameter(true);
    std::shared_ptr<const Game> game = LoadGame("dominion", params);
    std::unique_ptr<State> state = game->NewInitialState();
    DominionState *dominion_state = static_cast<DominionState *>(state.get());

    // Count number of kingdom cards (action or victory cards that aren't basic)
    int kingdom_count = 0;
    for (const auto &[card_id, count] : dominion_state->supply_counts) {
      Card *card = card_registry::get(card_id);
      if ((card->IsType(CardType::Action) || card->IsType(CardType::Victory)) &&
          card->name != "Estate" && card->name != "Duchy" &&
          card->name != "Province") {
        if (count > 0) ++kingdom_count;
      }
    }
    SPIEL_CHECK_EQ(kingdom_count, 10);
  }

  // Test partial random fill
  {
    GameParameters params;
    params["kingdom_cards"] = GameParameter("Village,Smithy");
    params["random_kingdom"] = GameParameter(true);
    std::shared_ptr<const Game> game = LoadGame("dominion", params);
    std::unique_ptr<State> state = game->NewInitialState();
    DominionState *dominion_state = static_cast<DominionState *>(state.get());

    // Check specified cards are present
    SPIEL_CHECK_GT(
        dominion_state->supply_counts[card_registry::get_id("Village")], 0);
    SPIEL_CHECK_GT(
        dominion_state->supply_counts[card_registry::get_id("Smithy")], 0);

    // Count total kingdom cards
    int kingdom_count = 0;
    for (const auto &[card_id, count] : dominion_state->supply_counts) {
      Card *card = card_registry::get(card_id);
      if ((card->IsType(CardType::Action) || card->IsType(CardType::Victory)) &&
          card->name != "Estate" && card->name != "Duchy" &&
          card->name != "Province") {
        if (count > 0) ++kingdom_count;
      }
    }
    SPIEL_CHECK_EQ(kingdom_count, 10);
  }

  // Test using all cards
  {
    GameParameters params;
    params["random_kingdom"] = GameParameter(false);
    std::shared_ptr<const Game> game = LoadGame("dominion", params);
    std::unique_ptr<State> state = game->NewInitialState();
    DominionState *dominion_state = static_cast<DominionState *>(state.get());

    // Count number of action cards in supply
    int action_count = 0;
    for (const auto &[card_id, count] : dominion_state->supply_counts) {
      Card *card = card_registry::get(card_id);
      if (card->IsType(CardType::Action)) {
        if (count > 0) ++action_count;
      }
    }
    // Should be more than 10 since we're using all cards
    SPIEL_CHECK_GT(action_count, 10);
  }

  // Test victory card pile sizes
  {
    GameParameters params;
    params["kingdom_cards"] = GameParameter("Gardens");
    std::shared_ptr<const Game> game = LoadGame("dominion", params);
    std::unique_ptr<State> state = game->NewInitialState();
    DominionState *dominion_state = static_cast<DominionState *>(state.get());

    // Gardens should have victory card pile size
    SPIEL_CHECK_EQ(
        dominion_state->supply_counts[card_registry::get_id("Gardens")], 8);
  }
}

void KnownCardPositionTests() {
  GameParameters params;
  std::shared_ptr<const Game> game = LoadGame("dominion");
  std::unique_ptr<State> state = game->NewInitialState();
  DominionState *dominion_state = static_cast<DominionState *>(state.get());

  dominion_state->SampleAllChanceNodes();
  auto &player = dominion_state->players[0];
  player.ClearAll();
  player.deck.push_back(card_registry::get("Copper"));
  player.deck.push_back(std::nullopt);
  player.unknown_cards = {card_registry::get("Estate")};

  // First draw should be chance node since we don't know top card
  // (well, in this case we do because there's only one unknown card, but that's
  // not implemented as a special case)
  dominion_state->DrawCardForPlayer(1, 0);
  SPIEL_CHECK_TRUE(dominion_state->IsChanceNode());

  // Draw the top card
  dominion_state->ApplyAction(0);
  SPIEL_CHECK_EQ(player.hand.back()->name, "Estate");
  SPIEL_CHECK_FALSE(dominion_state->IsChanceNode());

  // Second draw should NOT be chance node since we know the card
  dominion_state->DrawCardForPlayer(1, 0);
  SPIEL_CHECK_FALSE(dominion_state->IsChanceNode());
  SPIEL_CHECK_EQ(player.hand.back()->name, "Copper");
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
  open_spiel::dominion::LibraryTests();
  open_spiel::dominion::GameOverTests();
  open_spiel::dominion::GardensTests();
  open_spiel::dominion::WitchTests();
  open_spiel::dominion::KingdomSelectionTests();
  open_spiel::dominion::BasicDominionTests();
}
