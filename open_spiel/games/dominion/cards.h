#pragma once

#include <set>
#include <string>

#include "effects.h"

namespace open_spiel {
namespace dominion {
class DominionState;
class PlayerState;

enum class CardType {
  Treasure,
  Victory,
  Action,
};

Coroutine<std::optional<size_t>> SelectSupplyCard(DominionState &state,
                                                  bool allow_none = false);
Coroutine<std::optional<size_t>> SelectSupplyCard(DominionState &state,
                                                  int max_cost,
                                                  bool allow_none = false);
Coroutine<std::optional<size_t>> SelectHandCard(DominionState &state,
                                                bool allow_none = false);

class Card {
 public:
  const std::set<CardType> card_types;
  const int cost;
  const std::string name;

  virtual Coroutine<void> Play(DominionState &state) const { co_return; }
  virtual int GetVictoryPoints(const PlayerState &player_state) const {
    return 0;
  }

  bool IsType(CardType type) const;
  bool IsPlayable() const;
  bool IsTreasure() const { return IsType(CardType::Treasure); }
  bool IsVictory() const { return IsType(CardType::Victory); }
  bool IsAction() const { return IsType(CardType::Action); }

 protected:
  Card(std::set<CardType> card_types, int cost, const std::string &name)
      : card_types{std::move(card_types)}, cost{cost}, name{name} {}
};

class Village : public Card {
 public:
  Village() : Card{{CardType::Action}, 3, "Village"} {}
  Coroutine<void> Play(DominionState &state) const override;
};

class Woodcutter : public Card {
 public:
  Woodcutter() : Card{{CardType::Action}, 3, "Woodcutter"} {}
  Coroutine<void> Play(DominionState &state) const override;
};

class Smithy : public Card {
 public:
  Smithy() : Card{{CardType::Action}, 4, "Smithy"} {}
  Coroutine<void> Play(DominionState &state) const override;
};

class Market : public Card {
 public:
  Market() : Card{{CardType::Action}, 5, "Market"} {}
  Coroutine<void> Play(DominionState &state) const override;
};

class Festival : public Card {
 public:
  Festival() : Card{{CardType::Action}, 5, "Festival"} {}
  Coroutine<void> Play(DominionState &state) const override;
};

class Laboratory : public Card {
 public:
  Laboratory() : Card{{CardType::Action}, 5, "Laboratory"} {}
  Coroutine<void> Play(DominionState &state) const override;
};

class CouncilRoom : public Card {
 public:
  CouncilRoom() : Card{{CardType::Action}, 5, "Council Room"} {}
  Coroutine<void> Play(DominionState &state) const override;
};

class Workshop : public Card {
 public:
  Workshop() : Card{{CardType::Action}, 3, "Workshop"} {}
  Coroutine<void> Play(DominionState &state) const override;
};

class Chapel : public Card {
 public:
  Chapel() : Card{{CardType::Action}, 2, "Chapel"} {}
  Coroutine<void> Play(DominionState &state) const override;
};

class Moneylender : public Card {
 public:
  Moneylender() : Card{{CardType::Action}, 4, "Moneylender"} {}
  Coroutine<void> Play(DominionState &state) const override;
};

class Remodel : public Card {
 public:
  Remodel() : Card{{CardType::Action}, 4, "Remodel"} {}
  Coroutine<void> Play(DominionState &state) const override;
};

class ThroneRoom : public Card {
 public:
  ThroneRoom() : Card{{CardType::Action}, 4, "Throne Room"} {}
  Coroutine<void> Play(DominionState &state) const override;
};

class Library : public Card {
 public:
  Library() : Card{{CardType::Action}, 5, "Library"} {}
  Coroutine<void> Play(DominionState &state) const override;
};

class Witch : public Card {
 public:
  Witch() : Card{{CardType::Action}, 5, "Witch"} {}
  Coroutine<void> Play(DominionState &state) const override;
};

class Artisan : public Card {
 public:
  Artisan() : Card{{CardType::Action}, 6, "Artisan"} {}
  Coroutine<void> Play(DominionState &state) const override;
};

class Gardens : public Card {
 public:
  Gardens() : Card{{CardType::Victory}, 4, "Gardens"} {}
  int GetVictoryPoints(const PlayerState &player_state) const override;
};

class BasicTreasure : public Card {
 public:
  BasicTreasure(int cost, int value, const std::string &name)
      : Card{{CardType::Treasure}, cost, name}, value{value} {}
  Coroutine<void> Play(DominionState &state) const override;

 private:
  int value;
};

class Copper : public BasicTreasure {
 public:
  Copper() : BasicTreasure{0, 1, "Copper"} {}
};

class Silver : public BasicTreasure {
 public:
  Silver() : BasicTreasure{3, 2, "Silver"} {}
};

class Gold : public BasicTreasure {
 public:
  Gold() : BasicTreasure{6, 3, "Gold"} {}
};

class BasicVictory : public Card {
 public:
  BasicVictory(int cost, int vp, const std::string &name)
      : Card{{CardType::Victory}, cost, name}, vp_{vp} {}

  int GetVictoryPoints(const PlayerState &player_state) const override {
    return vp_;
  }

 private:
  const int vp_;
};

class Estate : public BasicVictory {
 public:
  Estate() : BasicVictory{2, 1, "Estate"} {}
};

class Duchy : public BasicVictory {
 public:
  Duchy() : BasicVictory{5, 3, "Duchy"} {}
};

class Province : public BasicVictory {
 public:
  Province() : BasicVictory{8, 6, "Province"} {}
};

class Curse : public Card {
 public:
  Curse() : Card{{}, 0, "Curse"} {}
  int GetVictoryPoints(const PlayerState &player_state) const override {
    return -1;
  }
};

class Cellar : public Card {
 public:
  Cellar() : Card{{CardType::Action}, 2, "Cellar"} {}
  Coroutine<void> Play(DominionState &state) const override;
};
}  // namespace dominion
}  // namespace open_spiel