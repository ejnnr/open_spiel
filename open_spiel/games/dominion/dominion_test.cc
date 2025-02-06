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

#include "open_spiel/spiel.h"
#include "open_spiel/tests/basic_tests.h"
#include "open_spiel/games/dominion/dominion.h"
#include "open_spiel/games/dominion/cards.h"
#include "open_spiel/games/dominion/card_registry.h"
#include "open_spiel/games/dominion/player_state.h"

namespace open_spiel
{
  namespace dominion
  {
    namespace
    {

      namespace testing = open_spiel::testing;

      void BasicDominionTests()
      {
        testing::LoadGameTest("dominion");
        std::shared_ptr<const Game> game = LoadGame("dominion");
        testing::RandomSimTest(*game, /*num_sims=*/10);
      }

      void PlayerStateTests()
      {
        card_registry::init();

        // Test default setup
        {
          PlayerState player{true};
          SPIEL_CHECK_EQ(player.hand.size(), 5);
          SPIEL_CHECK_EQ(player.deck.size(), 5);
          SPIEL_CHECK_TRUE(player.discard.empty());
          SPIEL_CHECK_TRUE(player.playing_area.empty());
        }

        // Test draw card
        {
          PlayerState player{false};
          SPIEL_CHECK_TRUE(player.hand.empty());
          SPIEL_CHECK_TRUE(player.deck.empty());

          player.deck.push_back(card_registry::get("Copper"));
          player.DrawCard();
          SPIEL_CHECK_EQ(player.hand.size(), 1);
          SPIEL_CHECK_EQ(player.deck.size(), 0);
          SPIEL_CHECK_TRUE(player.hand.back()->IsType(CardType::Treasure));
        }

        // Test play card
        {
          PlayerState player{false};
          player.hand.push_back(card_registry::get("Copper"));
          Card &played_card = player.PlayCard(0);
          SPIEL_CHECK_TRUE(player.hand.empty());
          SPIEL_CHECK_EQ(player.playing_area.size(), 1);
          SPIEL_CHECK_TRUE(player.playing_area.back()->IsType(CardType::Treasure));
          SPIEL_CHECK_TRUE(&played_card == player.playing_area.back());
        }
      }

      void GameStateTests()
      {
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
        auto copper_it = std::find_if(hand.begin(), hand.end(),
                                      [](const Card *card)
                                      { return card->IsTreasure(); });
        SPIEL_CHECK_TRUE(copper_it != hand.end());
        size_t copper_index = copper_it - hand.begin();
        dominion_state->PlayCard(copper_index);
        SPIEL_CHECK_EQ(dominion_state->n_coins, 1);

        // Test phase transitions
        dominion_state->NextPhase();
        SPIEL_CHECK_EQ(dominion_state->phase, Phase::Buy);
        dominion_state->NextPhase();
        SPIEL_CHECK_EQ(dominion_state->phase, Phase::Action);
        SPIEL_CHECK_EQ(dominion_state->CurrentPlayer(), 1); // Next player's turn
      }

      void GameOverTests()
      {
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
                                        [](int count)
                                        { return count == 0; });
        if (province_it != dominion_state->supply_counts.end())
        {
          *province_it = 0;
          SPIEL_CHECK_TRUE(dominion_state->IsGameOver());
        }

        // Game should be over if 3 supply piles are empty
        int empty_piles = 0;
        for (size_t i = 0; i < 3 && i < dominion_state->supply_counts.size(); ++i)
        {
          dominion_state->supply_counts[i] = 0;
          empty_piles++;
        }
        SPIEL_CHECK_TRUE(dominion_state->IsGameOver());
      }

    } // namespace
  } // namespace dominion
} // namespace open_spiel

int main(int argc, char **argv)
{
  open_spiel::dominion::BasicDominionTests();
  open_spiel::dominion::PlayerStateTests();
  open_spiel::dominion::GameStateTests();
  open_spiel::dominion::GameOverTests();
}
