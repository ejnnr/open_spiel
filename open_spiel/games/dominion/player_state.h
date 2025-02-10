#pragma once

#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace open_spiel {
namespace dominion {
class Card;

enum class Phase {
  Action,
  Buy,
};

inline std::ostream &operator<<(std::ostream &os, const Phase &phase) {
  switch (phase) {
    case Phase::Action:
      os << "Action";
      break;
    case Phase::Buy:
      os << "Buy";
      break;
    default:
      os << "Unknown";
  }
  return os;
}

class PlayerState {
 public:
  PlayerState(bool default_setup = true);
  Card &PlayCard(size_t hand_index);
  int VpCount() const;

  std::vector<Card *> deck{};
  std::vector<Card *> hand{};
  std::vector<Card *> playing_area{};
  std::vector<Card *> discard{};
};
}  // namespace dominion
}  // namespace open_spiel
