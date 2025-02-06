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

#include <algorithm>
#include <array>
#include <utility>

#include "open_spiel/game_parameters.h"

namespace open_spiel
{
  namespace dominion
  {

    namespace
    {
      // Default Parameters.
      constexpr int kDefaultPlayers = 2;

      // Facts about the game
      const GameType kGameType{
          /*short_name=*/"dominion",
          /*long_name=*/"Dominion",
          GameType::Dynamics::kSequential,
          GameType::ChanceMode::kDeterministic,
          GameType::Information::kImperfectInformation,
          // TODO: only the two-player version is zero sum
          GameType::Utility::kZeroSum,
          GameType::RewardModel::kTerminal,
          // TODO: allow any number of players?
          /*max_num_players=*/kDefaultPlayers,
          /*min_num_players=*/kDefaultPlayers,
          /*provides_information_state_string=*/true,
          /*provides_information_state_tensor=*/false,
          /*provides_observation_string=*/false,
          /*provides_observation_tensor=*/false,
          /*parameter_specification=*/
          // TODO: update this
          {
              {"players", GameParameter(kDefaultPlayers)},
          }};

      std::shared_ptr<const Game> Factory(const GameParameters &params)
      {
        return std::shared_ptr<const Game>(new DominionGame(params, kGameType));
      }

      const DominionGame *UnwrapGame(const Game *game)
      {
        return down_cast<const DominionGame *>(game);
      }
    } // namespace

    REGISTER_SPIEL_GAME(kGameType, Factory);
    // RegisterSingleTensorObserver single_tensor(kGameType.short_name);

    DominionState::DominionState(std::shared_ptr<const Game> game)
        : State(game),
          cur_player_(0),
          phase(Phase::Action),
          supply_counts(card_registry::num_cards(), 0),
          players(),
          trash(),
          n_actions(0),
          n_buys(0),
          n_coins(0),
          turn(0)
    {
    }

    std::string DominionState::ActionToString(Player player,
                                              Action action_id) const
    {
      DominionAction action = DominionAction::FromAction(action_id);
      switch (action.type)
      {
      case DominionAction::Type::kEnd:
        return "End";
      case DominionAction::Type::kSelectSupplyCard:
        return "Select supply card " + std::to_string(action.index);
      case DominionAction::Type::kSelectHandCard:
        return "Select hand card " + std::to_string(action.index);
      }
    }

    int DominionState::CurrentPlayer() const
    {
      if (IsTerminal())
      {
        return kTerminalPlayerId;
      }
      else
      {
        return cur_player_;
      }
    }

    void DominionState::DoApplyAction(Action action_id)
    {
      DominionAction action = DominionAction::FromAction(action_id);
      if (phase == Phase::Action)
      {
        if (action.type == DominionAction::Type::kSelectHandCard)
        {
          assert(action.index < CurrentHand().size());
          assert(CurrentHand()[action.index]->IsAction());
          PlayCard(action.index);
        }
        else if (action.type == DominionAction::Type::kEnd)
        {
          NextPhase();
        }
      }
      else if (phase == Phase::Buy)
      {
        if (action.type == DominionAction::Type::kSelectSupplyCard)
        {
          assert(action.index < supply_counts.size());
          Buy(action.index);
        }
        else if (action.type == DominionAction::Type::kSelectHandCard)
        {
          assert(action.index < CurrentHand().size());
          assert(CurrentHand()[action.index]->IsTreasure());
          PlayCard(action.index);
        }
        else if (action.type == DominionAction::Type::kEnd)
        {
          NextPhase();
        }
      }
    }

    std::vector<Action> DominionState::LegalActions() const
    {
      if (IsTerminal())
        return {};

      std::vector<Action> actions;

      if (phase == Phase::Action)
      {
        if (n_actions > 0)
        {
          for (size_t i = 0; i < CurrentHand().size(); ++i)
          {
            if (CurrentHand()[i]->IsAction())
              actions.push_back(DominionAction(DominionAction::Type::kSelectHandCard, i).ToAction());
          }
        }
      }
      else if (phase == Phase::Buy)
      {
        for (size_t i = 0; i < CurrentHand().size(); ++i)
        {
          if (CurrentHand()[i]->IsTreasure())
            actions.push_back(DominionAction(DominionAction::Type::kSelectHandCard, i).ToAction());
        }
        if (n_buys > 0)
        {
          for (size_t i = 0; i < supply_counts.size(); ++i)
          {
            if (supply_counts[i] > 0 && card_registry::get(i)->cost <= n_coins)
              actions.push_back(DominionAction(DominionAction::Type::kSelectSupplyCard, i).ToAction());
          }
        }
      }
      actions.push_back(DominionAction(DominionAction::Type::kEnd).ToAction());
      return actions;
    }

    std::string DominionState::InformationStateString(Player player) const
    {
      SPIEL_CHECK_GE(player, 0);
      SPIEL_CHECK_LT(player, num_players_);

      // TODO: this is not actually an information state, bunch of stuff missing.

      std::stringstream ss;
      ss << phase << " phase"
         << ", " << n_actions << " actions, " << n_buys << " buys, " << n_coins
         << " coins" << std::endl;
      ss << "Supply: ";
      for (size_t i = 0; i < supply_counts.size(); ++i)
      {
        ss << supply_counts[i] << " " << card_registry::get(i)->name << ", ";
      }
      ss << std::endl;
      ss << "Hand: ";
      for (const auto &card : CurrentHand())
      {
        ss << card->name << ", ";
      }
      ss << std::endl;
      ss << "Playing area: ";
      for (const auto &card : CurrentPlayingArea())
      {
        ss << card->name << ", ";
      }
      ss << std::endl;
      return ss.str();
    }

    std::vector<double> DominionState::Returns() const
    {
      std::vector<double> returns(num_players_, 0.0);

      // TODO: implement returns

      return returns;
    }

    std::unique_ptr<State> DominionState::Clone() const
    {
      return std::unique_ptr<State>(new DominionState(*this));
    }

    PlayerState &DominionState::CurrentPlayerState()
    {
      return players[cur_player_];
    }

    const PlayerState &DominionState::CurrentPlayerState() const
    {
      return players[cur_player_];
    }

    void DominionState::PlayCard(size_t hand_index)
    {
      Card &card = CurrentPlayerState().PlayCard(hand_index);
      // Only subtract an action if we're in the action phase, otherwise this is a
      // dual card played as a treasure/night card
      if (phase == Phase::Action && card.IsAction())
      {
        --n_actions;
      }
      card.Play(*this);
    }

    std::vector<Card *> &DominionState::CurrentDeck()
    {
      return CurrentPlayerState().deck;
    }

    const std::vector<Card *> &DominionState::CurrentDeck() const
    {
      return CurrentPlayerState().deck;
    }

    std::vector<Card *> &DominionState::CurrentHand()
    {
      return CurrentPlayerState().hand;
    }

    const std::vector<Card *> &DominionState::CurrentHand() const
    {
      return CurrentPlayerState().hand;
    }

    std::vector<Card *> &DominionState::CurrentPlayingArea()
    {
      return CurrentPlayerState().playing_area;
    }

    const std::vector<Card *> &DominionState::CurrentPlayingArea() const
    {
      return CurrentPlayerState().playing_area;
    }

    std::vector<Card *> &DominionState::CurrentDiscard()
    {
      return CurrentPlayerState().discard;
    }

    const std::vector<Card *> &DominionState::CurrentDiscard() const
    {
      return CurrentPlayerState().discard;
    }

    void DominionState::ResetCounters()
    {
      n_actions = 1;
      n_buys = 1;
      n_coins = 0;
      phase = Phase::Action;
    }

    void DominionState::Buy(size_t card_index)
    {
      Card *card = card_registry::get(card_index);
      if (n_buys < 1)
        throw std::runtime_error("No buys left");
      if (supply_counts[card_index] < 1)
        throw std::runtime_error(card->name + " pile is empty");
      if (card->cost > n_coins)
        throw std::runtime_error(
            card->name + " costs " + std::to_string(card->cost) + " but only " +
            std::to_string(n_coins) + " coins are available");
      n_coins -= card->cost;
      --n_buys;
      --supply_counts[card_index];
      CurrentDiscard().push_back(card);
    }

    bool DominionState::IsTerminal() const
    {
      // TODO: this should only be true once the final turn is finished
      return IsGameOver();
    }

    bool DominionState::IsGameOver() const
    {
      if (supply_counts[card_registry::get_id("Province")] == 0)
        return true;
      // check for three pile ending
      int n_piles_empty = 0;
      for (const auto count : supply_counts)
      {
        if (count == 0)
          ++n_piles_empty;
      }
      return n_piles_empty >= 3;
    }

    void DominionState::NextPhase()
    {
      if (phase == Phase::Action)
      {
        phase = Phase::Buy;
      }
      else if (phase == Phase::Buy)
      {
        // clean up
        PlayerState &player = CurrentPlayerState();
        // Discard playing area and hand
        player.discard.insert(player.discard.end(), player.playing_area.begin(),
                              player.playing_area.end());
        player.playing_area.clear();
        player.discard.insert(player.discard.end(), player.hand.begin(),
                              player.hand.end());
        player.hand.clear();
        // Draw next hand
        for (int i = 0; i < 5; ++i)
          player.DrawCard();

        // next player
        cur_player_ = (cur_player_ + 1) % num_players_;
        if (cur_player_ == 0)
          ++turn;
        ResetCounters();
      }
    }

    std::string DominionState::ToString() const
    {
      std::stringstream ss;
      ss << "Player " << cur_player_ << ", " << phase << " phase"
         << ", " << n_actions << " actions, " << n_buys << " buys, " << n_coins
         << " coins" << std::endl;
      ss << "Supply: ";
      for (size_t i = 0; i < supply_counts.size(); ++i)
      {
        ss << supply_counts[i] << " " << card_registry::get(i)->name << ", ";
      }
      ss << std::endl;
      ss << "Hand: ";
      for (const auto &card : CurrentHand())
      {
        ss << card->name << ", ";
      }
      ss << std::endl;
      ss << "Playing area: ";
      for (const auto &card : CurrentPlayingArea())
      {
        ss << card->name << ", ";
      }
      ss << std::endl;
      return ss.str();
    }

    DominionGame::DominionGame(const GameParameters &params, GameType game_type)
        : Game(game_type, params),
          num_players_(ParameterValue<int>("players"))
    {
      SPIEL_CHECK_GE(num_players_, kGameType.min_num_players);
      SPIEL_CHECK_LE(num_players_, kGameType.max_num_players);
      card_registry::init();
    }

    int DominionGame::NumDistinctActions() const
    {
      return 300;
    }

    std::unique_ptr<State> DominionGame::NewInitialState() const
    {
      std::unique_ptr<DominionState> state(
          new DominionState(shared_from_this()));

      if (num_players_ < 1)
        throw std::invalid_argument("Number of players must be at least 1");
      // TODO: these should depend on num_players_, and we should deduct starting
      // cards
      state->supply_counts[card_registry::get_id("Copper")] = 60;
      state->supply_counts[card_registry::get_id("Silver")] = 40;
      state->supply_counts[card_registry::get_id("Gold")] = 30;
      state->supply_counts[card_registry::get_id("Estate")] = 8;
      state->supply_counts[card_registry::get_id("Duchy")] = 8;
      state->supply_counts[card_registry::get_id("Province")] = 8;

      for (size_t i = 0; i < card_registry::num_cards(); ++i)
      {
        if (card_registry::get(i)->IsType(CardType::Action))
          state->supply_counts[i] = 10;
      }

      state->ResetCounters();
      state->players.reserve(num_players_);
      for (int i = 0; i < num_players_; ++i)
        state->players.push_back(PlayerState{true});

      return state;
    }

    int DominionGame::MaxGameLength() const
    {
      // A bet for each side and number of total dice, plus "liar" action.
      return 1000;
    }
  } // namespace dominion
} // namespace open_spiel