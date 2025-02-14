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

#include <algorithm>
#include <array>
#include <coroutine>
#include <utility>

#include "open_spiel/game_parameters.h"
#include "open_spiel/games/dominion/card_registry.h"

namespace open_spiel {
namespace dominion {

namespace {
// Default Parameters.
constexpr int kDefaultPlayers = 2;

// Facts about the game
const GameType kGameType{/*short_name=*/"dominion",
                         /*long_name=*/"Dominion",
                         GameType::Dynamics::kSequential,
                         GameType::ChanceMode::kExplicitStochastic,
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
                             {"small_supply", GameParameter(false)},
                         }};

std::shared_ptr<const Game> Factory(const GameParameters &params) {
  return std::shared_ptr<const Game>(new DominionGame(params, kGameType));
}

const DominionGame *UnwrapGame(const Game *game) {
  return down_cast<const DominionGame *>(game);
}
}  // namespace

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
      turn(0),
      continuation_(std::nullopt),
      pending_legal_actions(std::nullopt),
      pending_draw(false),
      pending_action(std::nullopt),
      rng_(std::random_device{}()) {}

std::string DominionState::ActionToString(Player player,
                                          Action action_id) const {
  if (player == kChancePlayerId) {
    return "Draw card " + std::to_string(action_id);
  }
  if (continuation_ && action_id != 0 && action_id < 100) {
    return "Custom action " + std::to_string(action_id);
  }
  DominionAction action = DominionAction::FromAction(action_id);
  switch (action.type) {
    case DominionAction::Type::kEnd:
      return "End";
    case DominionAction::Type::kSelectSupplyCard:
      return "Select supply card " + card_registry::get(action.index)->name;
    case DominionAction::Type::kSelectHandCard:
      // OpenSpiel doesn't allow duplicate action strings, so need to use the
      // index.
      // TODO: make it so there's only one action per card type in hand, given
      // that they're equivalent. return "Select hand card " +
      // CurrentHand()[action.index]->name;
      return "Select hand card " + std::to_string(action.index);
  }
}

int DominionState::CurrentPlayer() const {
  if (pending_draw) {
    return kChancePlayerId;
  } else if (IsTerminal()) {
    return kTerminalPlayerId;
  } else {
    return cur_player_;
  }
}

void DominionState::DoApplyAction(Action action_id) {
  pending_legal_actions.reset();
  if (continuation_) {
    auto continuation_copy = continuation_;
    // Important that we reset continuation_ before resuming rather than after,
    // since resuming may itself set a new continuation_.
    continuation_.reset();
    pending_action = action_id;
    continuation_copy->resume();
    pending_action.reset();
    return;
  }

  DominionAction action = DominionAction::FromAction(action_id);
  if (phase == Phase::Action) {
    if (action.type == DominionAction::Type::kSelectHandCard) {
      assert(action.index < CurrentHand().size());
      assert(CurrentHand()[action.index]->IsAction());
      PlayCard(action.index);
    } else if (action.type == DominionAction::Type::kEnd) {
      NextPhase();
    }
  } else if (phase == Phase::Buy) {
    if (action.type == DominionAction::Type::kSelectSupplyCard) {
      assert(action.index < supply_counts.size());
      Buy(action.index);
    } else if (action.type == DominionAction::Type::kSelectHandCard) {
      assert(action.index < CurrentHand().size());
      assert(CurrentHand()[action.index]->IsTreasure());
      PlayCard(action.index);
    } else if (action.type == DominionAction::Type::kEnd) {
      NextPhase();
    }
  }
}

std::vector<Action> DominionState::LegalActions() const {
  if (IsTerminal()) return {};

  if (continuation_) {
    return pending_legal_actions.value();
  }

  std::vector<Action> actions;

  if (phase == Phase::Action) {
    if (n_actions > 0) {
      for (size_t i = 0; i < CurrentHand().size(); ++i) {
        if (CurrentHand()[i]->IsAction())
          actions.push_back(
              DominionAction(DominionAction::Type::kSelectHandCard, i)
                  .ToAction());
      }
    }
  } else if (phase == Phase::Buy) {
    for (size_t i = 0; i < CurrentHand().size(); ++i) {
      if (CurrentHand()[i]->IsTreasure())
        actions.push_back(
            DominionAction(DominionAction::Type::kSelectHandCard, i)
                .ToAction());
    }
    if (n_buys > 0) {
      for (size_t i = 0; i < supply_counts.size(); ++i) {
        if (supply_counts[i] > 0 && card_registry::get(i)->cost <= n_coins)
          actions.push_back(
              DominionAction(DominionAction::Type::kSelectSupplyCard, i)
                  .ToAction());
      }
    }
  }
  actions.push_back(DominionAction(DominionAction::Type::kEnd).ToAction());
  std::sort(actions.begin(), actions.end());
  return actions;
}

std::string DominionState::InformationStateString(Player player) const {
  SPIEL_CHECK_GE(player, 0);
  SPIEL_CHECK_LT(player, num_players_);

  // TODO: this is not actually an information state, bunch of stuff missing.

  std::stringstream ss;
  ss << phase << " phase"
     << ", " << n_actions << " actions, " << n_buys << " buys, " << n_coins
     << " coins" << std::endl;
  ss << "Supply: ";
  for (size_t i = 0; i < supply_counts.size(); ++i) {
    ss << supply_counts[i] << " " << card_registry::get(i)->name << ", ";
  }
  ss << std::endl;
  ss << "Hand: ";
  for (const auto &card : CurrentHand()) {
    ss << card->name << ", ";
  }
  ss << std::endl;
  ss << "Playing area: ";
  for (const auto &card : CurrentPlayingArea()) {
    ss << card->name << ", ";
  }
  ss << std::endl;
  return ss.str();
}

std::vector<double> DominionState::Returns() const {
  std::vector<double> returns(num_players_, 0.0);

  // TODO: implement returns

  return returns;
}

std::unique_ptr<State> DominionState::Clone() const {
  auto clone = game_->NewInitialState();
  for (const auto &player_and_action : FullHistory()) {
    clone->ApplyAction(player_and_action.action);
  }
  return clone;
}

ActionsAndProbs DominionState::ChanceOutcomes() const {
  SPIEL_CHECK_TRUE(pending_legal_actions.has_value());
  SPIEL_CHECK_GT(pending_legal_actions->size(), 0);
  ActionsAndProbs outcomes;
  outcomes.reserve(pending_legal_actions->size());
  double prob = 1.0 / pending_legal_actions->size();
  for (Action action : *pending_legal_actions) {
    outcomes.push_back({action, prob});
  }
  return outcomes;
}

std::vector<Action> DominionState::LegalChanceOutcomes() const {
  SPIEL_CHECK_TRUE(pending_legal_actions.has_value());
  SPIEL_CHECK_GT(pending_legal_actions->size(), 0);
  return *pending_legal_actions;
}

void DominionState::SampleAllChanceNodes() {
  while (IsChanceNode()) {
    auto action = SampleAction(ChanceOutcomes(), rng_).first;
    ApplyAction(action);
  }
}

PlayerState &DominionState::CurrentPlayerState() {
  return players[cur_player_];
}

const PlayerState &DominionState::CurrentPlayerState() const {
  return players[cur_player_];
}

void DominionState::PlayCard(size_t hand_index) {
  Card &card = CurrentPlayerState().PlayCard(hand_index);
  // Only subtract an action if we're in the action phase, otherwise this is a
  // dual card played as a treasure/night card
  if (phase == Phase::Action && card.IsAction()) {
    --n_actions;
  }
  card.Play(*this);
}

Coroutine DominionState::DrawCardForPlayer(int n, Player player_id) {
  PlayerState &player = players[player_id];
  for (int i = 0; i < n; ++i) {
    if (player.deck.empty() && !player.discard.empty()) {
      std::swap(player.deck, player.discard);
    }
    if (!player.deck.empty()) {
      // Draw a random card. This creates a chance node, so need to await an
      // action before actually drawing the card.
      // The legal actions are the indices of the cards in the deck.
      pending_legal_actions = std::vector<Action>(player.deck.size());
      std::iota(pending_legal_actions->begin(), pending_legal_actions->end(),
                0);
      pending_draw = true;
      // TODO: a bit weird that we're passing in pending_legal_actions, which in
      // this case will just be copied into itself
      auto action =
          co_await ActionAwaiter{*this, pending_legal_actions.value()};
      pending_draw = false;
      SPIEL_CHECK_GE(action, 0);
      SPIEL_CHECK_LT(action, player.deck.size());

      // Draw the card
      if (action != player.deck.size() - 1) {
        std::swap(player.deck[action], player.deck.back());
      }
      player.hand.push_back(player.deck.back());
      player.deck.pop_back();
    }
  }
  co_return;
}

Coroutine DominionState::DrawHandForAllPlayers() {
  for (Player player_id = 0; player_id < num_players_; ++player_id) {
    co_await DrawCardForPlayer(5, player_id);
  }
  co_return;
}

void DominionState::TrashFromHand(size_t hand_index) {
  trash.push_back(CurrentHand()[hand_index]);
  CurrentHand().erase(CurrentHand().begin() + hand_index);
}

void DominionState::TrashFromHand(std::vector<Card *>::iterator it) {
  trash.push_back(*it);
  CurrentHand().erase(it);
}

void DominionState::DiscardFromHand(size_t hand_index) {
  CurrentDiscard().push_back(CurrentHand()[hand_index]);
  CurrentHand().erase(CurrentHand().begin() + hand_index);
}

void DominionState::DiscardFromHand(std::vector<Card *>::iterator it) {
  CurrentDiscard().push_back(*it);
  CurrentHand().erase(it);
}

std::vector<Card *> &DominionState::CurrentDeck() {
  return CurrentPlayerState().deck;
}

const std::vector<Card *> &DominionState::CurrentDeck() const {
  return CurrentPlayerState().deck;
}

std::vector<Card *> &DominionState::CurrentHand() {
  return CurrentPlayerState().hand;
}

const std::vector<Card *> &DominionState::CurrentHand() const {
  return CurrentPlayerState().hand;
}

std::vector<Card *> &DominionState::CurrentPlayingArea() {
  return CurrentPlayerState().playing_area;
}

const std::vector<Card *> &DominionState::CurrentPlayingArea() const {
  return CurrentPlayerState().playing_area;
}

std::vector<Card *> &DominionState::CurrentDiscard() {
  return CurrentPlayerState().discard;
}

const std::vector<Card *> &DominionState::CurrentDiscard() const {
  return CurrentPlayerState().discard;
}

void DominionState::ResetCounters() {
  n_actions = 1;
  n_buys = 1;
  n_coins = 0;
  phase = Phase::Action;
}

void DominionState::Buy(size_t card_index) {
  Card *card = card_registry::get(card_index);
  if (n_buys < 1) throw std::runtime_error("No buys left");
  if (supply_counts[card_index] < 1)
    throw std::runtime_error(card->name + " pile is empty");
  if (card->cost > n_coins)
    throw std::runtime_error(card->name + " costs " +
                             std::to_string(card->cost) + " but only " +
                             std::to_string(n_coins) + " coins are available");
  n_coins -= card->cost;
  --n_buys;
  --supply_counts[card_index];
  CurrentDiscard().push_back(card);
}

bool DominionState::IsTerminal() const {
  // TODO: this should only be true once the final turn is finished
  return IsGameOver();
}

bool DominionState::IsGameOver() const {
  if (supply_counts[card_registry::get_id("Province")] == 0) return true;
  // check for three pile ending
  int n_piles_empty = 0;
  for (const auto count : supply_counts) {
    if (count == 0) ++n_piles_empty;
  }
  return n_piles_empty >= 3;
}

Coroutine DominionState::NextPhase() {
  if (phase == Phase::Action) {
    phase = Phase::Buy;
  } else if (phase == Phase::Buy) {
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
    DrawCardForPlayer(5, cur_player_);

    // next player
    cur_player_ = (cur_player_ + 1) % num_players_;
    if (cur_player_ == 0) ++turn;
    ResetCounters();
  }
}

std::string DominionState::ToString() const {
  std::stringstream ss;
  ss << "Turn " << turn << ", " << "Player " << cur_player_ << ", " << phase
     << " phase"
     << ", " << n_actions << " actions, " << n_buys << " buys, " << n_coins
     << " coins" << std::endl;
  ss << "Supply: ";
  for (size_t i = 0; i < supply_counts.size(); ++i) {
    ss << supply_counts[i] << " " << card_registry::get(i)->name << ", ";
  }
  ss << std::endl;
  ss << "Hand: ";
  for (const auto &card : CurrentHand()) {
    ss << card->name << ", ";
  }
  ss << std::endl;
  ss << "Playing area: ";
  for (const auto &card : CurrentPlayingArea()) {
    ss << card->name << ", ";
  }
  ss << std::endl;
  return ss.str();
}

DominionGame::DominionGame(const GameParameters &params, GameType game_type)
    : Game(game_type, params), num_players_(ParameterValue<int>("players")) {
  SPIEL_CHECK_GE(num_players_, kGameType.min_num_players);
  SPIEL_CHECK_LE(num_players_, kGameType.max_num_players);
  card_registry::init();
}

int DominionGame::NumDistinctActions() const { return 300; }

std::unique_ptr<State> DominionGame::NewInitialState() const {
  std::unique_ptr<DominionState> state(new DominionState(shared_from_this()));

  if (num_players_ < 1)
    throw std::invalid_argument("Number of players must be at least 1");
  // TODO: these should depend on num_players_, and we should deduct starting
  // cards
  // small_supply is a parameter to speed up random playthroughs in testing
  bool small_supply = ParameterValue<bool>("small_supply");
  state->supply_counts[card_registry::get_id("Copper")] =
      small_supply ? 10 : 60;
  state->supply_counts[card_registry::get_id("Silver")] =
      small_supply ? 10 : 40;
  state->supply_counts[card_registry::get_id("Gold")] = small_supply ? 10 : 30;
  state->supply_counts[card_registry::get_id("Estate")] = small_supply ? 4 : 8;
  state->supply_counts[card_registry::get_id("Duchy")] = small_supply ? 4 : 8;
  state->supply_counts[card_registry::get_id("Province")] =
      small_supply ? 4 : 8;

  for (size_t i = 0; i < card_registry::num_cards(); ++i) {
    if (card_registry::get(i)->IsType(CardType::Action))
      state->supply_counts[i] = small_supply ? 5 : 10;
  }

  state->ResetCounters();
  state->players.reserve(num_players_);
  for (int i = 0; i < num_players_; ++i) {
    state->players.push_back(PlayerState{true});
  }
  // We can't just loop over all players and draw a hand for each; this would
  // create a coroutine for each player, and continuation_ would be overriden to
  // the one for the last player. This helper coroutine that draws for all
  // players lets us keep to a single continuation_.
  state->DrawHandForAllPlayers();

  return state;
}

int DominionGame::MaxGameLength() const { return 10000; }

std::string DominionState::Serialize() const {
  std::stringstream ss;

  // Game state
  ss << static_cast<int>(phase) << " " << n_actions << " " << n_buys << " "
     << n_coins << " " << turn << " " << cur_player_ << "\n";

  // Supply counts
  for (int count : supply_counts) {
    ss << count << " ";
  }
  ss << "\n";

  // Trash pile
  ss << trash.size() << " ";
  for (const Card *card : trash) {
    ss << card_registry::get_id(card->name) << " ";
  }
  ss << "\n";

  // Player states
  ss << players.size() << "\n";
  for (const auto &player : players) {
    // Serialize each pile (deck, hand, playing_area, discard)
    // For each pile, first write size, then card indices
    auto serialize_pile = [&ss](const std::vector<Card *> &pile) {
      ss << pile.size() << " ";
      for (const Card *card : pile) {
        ss << card_registry::get_id(card->name) << " ";
      }
      ss << "\n";
    };

    serialize_pile(player.deck);
    serialize_pile(player.hand);
    serialize_pile(player.playing_area);
    serialize_pile(player.discard);
  }

  return ss.str();
}

std::unique_ptr<State> DominionGame::DeserializeState(
    const std::string &str) const {
  std::istringstream ss(str);
  auto state = std::make_unique<DominionState>(shared_from_this());

  // Game state
  int phase_int;
  ss >> phase_int >> state->n_actions >> state->n_buys >> state->n_coins >>
      state->turn >> state->cur_player_;
  state->phase = static_cast<Phase>(phase_int);

  // Supply counts
  state->supply_counts.resize(card_registry::num_cards());
  for (int &count : state->supply_counts) {
    ss >> count;
  }

  // Trash pile
  int trash_size;
  ss >> trash_size;
  state->trash.clear();
  state->trash.reserve(trash_size);
  for (int i = 0; i < trash_size; ++i) {
    int card_id;
    ss >> card_id;
    state->trash.push_back(card_registry::get(card_id));
  }

  // Player states
  int num_players;
  ss >> num_players;
  state->players.clear();
  state->players.reserve(num_players);

  for (int i = 0; i < num_players; ++i) {
    state->players.emplace_back(false);  // Don't do default setup
    auto &player = state->players.back();

    // Helper to deserialize a pile of cards
    auto deserialize_pile = [&ss](std::vector<Card *> &pile) {
      int size;
      ss >> size;
      pile.clear();
      pile.reserve(size);
      for (int j = 0; j < size; ++j) {
        int card_id;
        ss >> card_id;
        pile.push_back(card_registry::get(card_id));
      }
    };

    deserialize_pile(player.deck);
    deserialize_pile(player.hand);
    deserialize_pile(player.playing_area);
    deserialize_pile(player.discard);
  }

  return state;
}

std::string DominionGame::GetRNGState() const {
  // std::ostringstream rng_stream;
  // rng_stream << rng_;
  // return rng_stream.str();
  return "";
}

void DominionGame::SetRNGState(const std::string &rng_state) const {
  // if (rng_state.empty())
  //   return;
  // std::istringstream rng_stream(rng_state);
  // rng_stream >> rng_;
  return;
}

}  // namespace dominion
}  // namespace open_spiel