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

#ifndef OPEN_SPIEL_GAMES_DOMINION_H_
#define OPEN_SPIEL_GAMES_DOMINION_H_

#include <array>
#include <memory>
#include <string>
#include <vector>
#include <coroutine>
#include <random>

#include "open_spiel/spiel.h"
#include "open_spiel/spiel_utils.h"
#include "player_state.h"
#include "effects.h"
// A simple game that includes chance and imperfect information
// https://en.wikipedia.org/wiki/Liar%27s_dice
//
// Currently only supports a single round and two players.
// The highest face (`dice_sides`) is wild.
//
// Parameters:
//   "bidding_rule" string   bidding variants ("reset-face" or
//                           ("reset-quantity")              (def. "reset-face")
//   "dice_sides"   int      number of sides on each die            (def. = 6)
//   "numdice"      int      number of dice per player              (def. = 1)
//   "numdiceX"     int      overridden number of dice for player X (def. = 1)
//   "players"      int      number of players                      (def. = 2)

namespace open_spiel
{
  namespace dominion
  {
    class DominionGame;

    struct DominionAction
    {
      enum Type
      {
        kEnd,
        kSelectSupplyCard,
        kSelectHandCard,
      };
      Type type;
      size_t index;

      DominionAction(Type type, size_t index = 0) : type(type), index(index) {}

      Action ToAction() const
      {
        return static_cast<Action>(type * 100 + index);
      };
      static DominionAction FromAction(Action action)
      {
        return DominionAction(static_cast<Type>(action / 100), action % 100);
      }
    };

    class DominionState : public State
    {
    public:
      explicit DominionState(std::shared_ptr<const Game> game);
      DominionState(const DominionState &) = default;

      // OpenSpiel interface
      void Reset(const GameParameters &params);
      Player CurrentPlayer() const override;
      std::string ActionToString(Player player, Action action_id) const override;
      std::string ToString() const override;
      bool IsTerminal() const override;
      std::vector<double> Returns() const override;
      std::string InformationStateString(Player player) const override;
      std::unique_ptr<State> Clone() const override;
      std::vector<Action> LegalActions() const override;
      ActionsAndProbs ChanceOutcomes() const override;
      std::string Serialize() const override;

      // Dominion-specific helper functions
      void PlayCard(size_t hand_index);
      Coroutine DrawCardForPlayer(int n, Player player_id);
      Coroutine DrawCard(int n) { return DrawCardForPlayer(n, cur_player_); };
      void Buy(size_t card_index);
      Coroutine NextPhase();
      void ResetCounters();
      bool IsGameOver() const;

      // Convenience aliases
      std::vector<Card *> &CurrentDeck();
      const std::vector<Card *> &CurrentDeck() const;
      std::vector<Card *> &CurrentHand();
      const std::vector<Card *> &CurrentHand() const;
      std::vector<Card *> &CurrentPlayingArea();
      const std::vector<Card *> &CurrentPlayingArea() const;
      std::vector<Card *> &CurrentDiscard();
      const std::vector<Card *> &CurrentDiscard() const;
      PlayerState &CurrentPlayerState();
      const PlayerState &CurrentPlayerState() const;

      // Initialized to invalid values. Use Game::NewInitialState().
      Phase phase;
      std::vector<int> supply_counts;
      std::vector<PlayerState> players;
      std::vector<Card *> trash;
      int n_actions;
      int n_buys;
      int n_coins;
      int turn;
      Player cur_player_; // Player whose turn it is.
      std::optional<std::coroutine_handle<>> continuation_;
      std::optional<std::vector<Action>> pending_legal_actions;
      bool pending_shuffle;
      std::optional<Action> pending_action;
      mutable std::mt19937 rng_; // Random number generator

    protected:
      void DoApplyAction(Action action_id) override;

    private:
    };

    class DominionGame : public Game
    {
    public:
      explicit DominionGame(const GameParameters &params, GameType game_type);
      int NumDistinctActions() const override;
      std::unique_ptr<State> NewInitialState() const override;
      int NumPlayers() const override { return num_players_; }
      double MinUtility() const override { return -1; }
      double MaxUtility() const override { return 1; }
      absl::optional<double> UtilitySum() const override { return 0; }
      int MaxGameLength() const override;
      int MaxChanceOutcomes() const override { return 1; }
      std::unique_ptr<State> DeserializeState(const std::string &str) const override;
      std::string GetRNGState() const override;
      void SetRNGState(const std::string &rng_state) const override;

    private:
      // Number of players.
      int num_players_;
    };
  } // namespace dominion
} // namespace open_spiel

#endif // OPEN_SPIEL_GAMES_DOMINION_H_
