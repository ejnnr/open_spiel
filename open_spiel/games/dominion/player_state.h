#pragma once

#include <iostream>
#include <memory>
#include <optional>
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

  // Helper for tests to set up a known deck state
  void SetupKnownDeck(const std::vector<Card *> &cards);
  // Helper for tests to set up an unknown deck state
  void SetupUnknownDeck(const std::vector<Card *> &cards);
  // Clear all card piles (hand, deck, discard, playing_area)
  void ClearAll();

  std::vector<std::optional<Card *>> deck{};  // Cards in deck, known or unknown
  std::vector<Card *>
      unknown_cards{};  // Set of cards that could be in unknown positions
  std::vector<Card *> hand{};
  std::vector<Card *> playing_area{};
  std::vector<Card *> discard{};
};
}  // namespace dominion
}  // namespace open_spiel
