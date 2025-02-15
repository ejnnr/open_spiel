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
const GameType kGameType{
    /*short_name=*/"dominion",
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
    {
        {"players", GameParameter(kDefaultPlayers)},
        {"small_supply", GameParameter(false)},
        // List of kingdom card names to use, empty means use all cards
        {"kingdom_cards", GameParameter(std::string(""))},
        // Whether to randomly select 10 kingdom cards if not enough specified
        {"random_kingdom", GameParameter(false)},
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
  DominionAction action = DominionAction(action_id);
  switch (action.type) {
    case ActionType::kEnd:
      return "End";
    case ActionType::kSelectSupplyCard:
      return "Select supply card " + card_registry::get(action.index)->name;
    case ActionType::kSelectHandCard:
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
  } else {
    DominionAction action = DominionAction(action_id);
    if (phase == Phase::Action) {
      if (action.type == ActionType::kSelectHandCard) {
        assert(action.index < CurrentHand().size());
        assert(CurrentHand()[action.index]->IsAction());
        PlayCard(action.index);
      } else if (action.type == ActionType::kEnd) {
        NextPhase();
      }
    } else if (phase == Phase::Buy) {
      if (action.type == ActionType::kSelectSupplyCard) {
        assert(action.index < supply_counts.size());
        Buy(action.index);
      } else if (action.type == ActionType::kSelectHandCard) {
        assert(action.index < CurrentHand().size());
        assert(CurrentHand()[action.index]->IsTreasure());
        PlayCard(action.index);
      } else if (action.type == ActionType::kEnd) {
        NextPhase();
      }
    }
  }

  // If there's only one legal action, we auto-pick it. This will recursively
  // keep auto-picking actions until there's an actual decision to be made.
  auto legal_actions = LegalActions();
  if (legal_actions.size() == 1) {
    // Note DoApplyAction instead of ApplyAction; we don't count these
    // auto-picked actions towards history. (If we did, the history would in
    // fact be wrong because the auto-picked action would be inserted before the
    // current one.)
    DoApplyAction(legal_actions[0]);
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
          actions.push_back(GetActionId(ActionType::kSelectHandCard, i));
      }
    }
  } else if (phase == Phase::Buy) {
    for (size_t i = 0; i < CurrentHand().size(); ++i) {
      if (CurrentHand()[i]->IsTreasure())
        actions.push_back(GetActionId(ActionType::kSelectHandCard, i));
    }
    if (n_buys > 0) {
      for (const auto &[card_id, count] : supply_counts) {
        if (count > 0 && card_registry::get(card_id)->cost <= n_coins)
          actions.push_back(
              GetActionId(ActionType::kSelectSupplyCard, card_id));
      }
    }
  }
  actions.push_back(GetActionId(ActionType::kEnd));
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
  for (const auto &[card_id, count] : supply_counts) {
    ss << count << " " << card_registry::get(card_id)->name << ", ";
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

Coroutine<void> DominionState::DrawCardForPlayer(int n, Player player_id) {
  PlayerState &player = players[player_id];
  for (int i = 0; i < n; ++i) {
    if (player.deck.empty() && !player.discard.empty()) {
      // When shuffling, all cards become unknown
      std::swap(player.unknown_cards, player.discard);
      player.deck.resize(player.unknown_cards.size(), std::nullopt);
    }
    if (!player.deck.empty()) {
      // If top card is known, draw it directly
      if (player.deck.back().has_value()) {
        player.hand.push_back(*player.deck.back());
        player.deck.pop_back();
      } else {
        // Draw a random card. This creates a chance node, so need to await an
        // action before actually drawing the card.
        // The legal actions are the indices into unknown_cards
        pending_legal_actions =
            std::vector<Action>(player.unknown_cards.size());
        std::iota(pending_legal_actions->begin(), pending_legal_actions->end(),
                  0);
        pending_draw = true;
        // TODO: a bit weird that we're passing in pending_legal_actions, which
        // in this case will just be copied into itself
        auto action =
            co_await ActionAwaiter{*this, pending_legal_actions.value()};
        pending_draw = false;
        SPIEL_CHECK_GE(action, 0);
        SPIEL_CHECK_LT(action, player.unknown_cards.size());

        // Draw the selected card
        Card *drawn_card = player.unknown_cards[action];
        if (action != player.unknown_cards.size() - 1) {
          std::swap(player.unknown_cards[action], player.unknown_cards.back());
        }
        player.unknown_cards.pop_back();
        player.hand.push_back(drawn_card);
        player.deck.pop_back();
      }
    }
  }
  co_return;
}

Coroutine<void> DominionState::DrawHandForAllPlayers() {
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

bool DominionState::GainCard(size_t card_index, Player player_id) {
  if (card_index >= supply_counts.size())
    throw std::runtime_error("Invalid card index");
  if (supply_counts[card_index] < 1) return false;

  if (player_id == -1) player_id = cur_player_;

  --supply_counts[card_index];
  players[player_id].discard.push_back(card_registry::get(card_index));
  return true;
}

std::vector<std::optional<Card *>> &DominionState::CurrentDeck() {
  return CurrentPlayerState().deck;
}

const std::vector<std::optional<Card *>> &DominionState::CurrentDeck() const {
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
  if (!GainCard(card_index))
    throw std::runtime_error("Failed to gain card (this shouldn't happen)");
}

bool DominionState::IsTerminal() const {
  // TODO: this should only be true once the final turn is finished
  return IsGameOver();
}

bool DominionState::IsGameOver() const {
  if (supply_counts.at(card_registry::get_id("Province")) == 0) return true;
  // check for three pile ending
  int n_piles_empty = 0;
  for (const auto &[card_id, count] : supply_counts) {
    if (count == 0) ++n_piles_empty;
  }
  return n_piles_empty >= 3;
}

Coroutine<void> DominionState::NextPhase() {
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
  std::vector<std::pair<std::string, int>> sorted_supply;
  for (const auto &[card_id, count] : supply_counts) {
    sorted_supply.emplace_back(card_registry::get(card_id)->name, count);
  }
  std::sort(sorted_supply.begin(), sorted_supply.end(),
            [](const auto &a, const auto &b) { return a.first < b.first; });
  for (const auto &[name, count] : sorted_supply) {
    ss << count << " " << name << ", ";
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

  // Parse kingdom cards parameter
  std::string kingdom_cards_str = ParameterValue<std::string>("kingdom_cards");
  if (!kingdom_cards_str.empty()) {
    // Split string on commas
    std::stringstream ss(kingdom_cards_str);
    std::string card_name;
    while (std::getline(ss, card_name, ',')) {
      // Trim whitespace
      card_name.erase(0, card_name.find_first_not_of(" \t\n\r\f\v"));
      card_name.erase(card_name.find_last_not_of(" \t\n\r\f\v") + 1);
      if (!card_registry::exists(card_name)) {
        SpielFatalError("Invalid kingdom card: " + card_name);
      }
      kingdom_cards_.push_back(card_registry::get_id(card_name));
    }
  }

  bool random_kingdom = ParameterValue<bool>("random_kingdom");
  if (kingdom_cards_.empty() && !random_kingdom) {
    // Use all cards except basic treasures and victory cards
    for (size_t i = 0; i < card_registry::num_cards(); ++i) {
      Card *card = card_registry::get(i);
      if (card->IsType(CardType::Action) || card->IsType(CardType::Victory)) {
        if (card->name != "Estate" && card->name != "Duchy" &&
            card->name != "Province") {
          kingdom_cards_.push_back(i);
        }
      }
    }
  } else if (random_kingdom && kingdom_cards_.size() < 10) {
    // If random_kingdom is true and we don't have enough cards, randomly select
    // more
    std::vector<size_t> available_kingdom_cards;
    for (size_t i = 0; i < card_registry::num_cards(); ++i) {
      Card *card = card_registry::get(i);
      if (card->IsType(CardType::Action) || card->IsType(CardType::Victory)) {
        if (card->name != "Estate" && card->name != "Duchy" &&
            card->name != "Province") {
          // Only add if not already selected
          if (std::find(kingdom_cards_.begin(), kingdom_cards_.end(), i) ==
              kingdom_cards_.end()) {
            available_kingdom_cards.push_back(i);
          }
        }
      }
    }

    if (!available_kingdom_cards.empty()) {
      // Shuffle available cards
      std::random_device rd;
      std::mt19937 gen(rd());
      std::shuffle(available_kingdom_cards.begin(),
                   available_kingdom_cards.end(), gen);

      // Add random cards until we have 10
      while (kingdom_cards_.size() < 10 && !available_kingdom_cards.empty()) {
        kingdom_cards_.push_back(available_kingdom_cards.back());
        available_kingdom_cards.pop_back();
      }
    }
  }
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

  // Initialize basic cards
  state->supply_counts[card_registry::get_id("Copper")] =
      small_supply ? 10 : 60;
  state->supply_counts[card_registry::get_id("Silver")] =
      small_supply ? 10 : 40;
  state->supply_counts[card_registry::get_id("Gold")] = small_supply ? 10 : 30;
  state->supply_counts[card_registry::get_id("Estate")] = small_supply ? 4 : 8;
  state->supply_counts[card_registry::get_id("Duchy")] = small_supply ? 4 : 8;
  state->supply_counts[card_registry::get_id("Province")] =
      small_supply ? 4 : 8;
  state->supply_counts[card_registry::get_id("Curse")] =
      small_supply ? 5
                   : (num_players_ == 2 ? 10 : (num_players_ == 3 ? 20 : 30));

  // Initialize kingdom cards
  for (size_t card_id : kingdom_cards_) {
    if (card_registry::get(card_id)->IsType(CardType::Victory)) {
      state->supply_counts[card_id] = small_supply ? 4 : 8;
    } else {
      state->supply_counts[card_id] = small_supply ? 5 : 10;
    }
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
  ss << supply_counts.size() << "\n";
  for (const auto &[card_id, count] : supply_counts) {
    ss << card_id << " " << count << " ";
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
    auto serialize_known_pile = [&ss](const std::vector<Card *> &pile) {
      ss << pile.size() << " ";
      for (const Card *card : pile) {
        ss << card_registry::get_id(card->name) << " ";
      }
      ss << "\n";
    };

    // Special serialization for deck with optional values
    ss << player.deck.size() << " ";
    for (const auto &card_opt : player.deck) {
      ss << (card_opt.has_value() ? "1" : "0") << " ";
      if (card_opt.has_value()) {
        ss << card_registry::get_id((*card_opt)->name) << " ";
      }
    }
    ss << "\n";

    // Serialize unknown cards
    ss << player.unknown_cards.size() << " ";
    for (const Card *card : player.unknown_cards) {
      ss << card_registry::get_id(card->name) << " ";
    }
    ss << "\n";

    serialize_known_pile(player.hand);
    serialize_known_pile(player.playing_area);
    serialize_known_pile(player.discard);
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
  size_t supply_size;
  ss >> supply_size;
  state->supply_counts.clear();
  state->supply_counts.reserve(supply_size);
  for (size_t i = 0; i < supply_size; ++i) {
    size_t card_id;
    int count;
    ss >> card_id >> count;
    state->supply_counts[card_id] = count;
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

    // Helper to deserialize a regular pile of cards
    auto deserialize_known_pile = [&ss](std::vector<Card *> &pile) {
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

    // Special deserialization for deck with optional values
    int deck_size;
    ss >> deck_size;
    player.deck.clear();
    player.deck.reserve(deck_size);
    for (int j = 0; j < deck_size; ++j) {
      std::string has_value;
      ss >> has_value;
      if (has_value == "1") {
        int card_id;
        ss >> card_id;
        player.deck.push_back(card_registry::get(card_id));
      } else {
        player.deck.push_back(std::nullopt);
      }
    }

    // Deserialize unknown cards
    deserialize_known_pile(player.unknown_cards);
    deserialize_known_pile(player.hand);
    deserialize_known_pile(player.playing_area);
    deserialize_known_pile(player.discard);
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