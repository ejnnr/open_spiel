#pragma once
#include <memory>
#include <unordered_map>

#include "cards.h"

namespace open_spiel {
namespace dominion {
namespace card_registry {
// Initialize the registry - call this once at program start (idempotent)
void init();

// Get a card by name or id (returns raw pointer since registry owns the cards)
Card *get(const std::string &name);
Card *get(size_t id);

bool exists(const std::string &name);
bool exists(size_t id);

size_t get_id(const std::string &name);

size_t num_cards();
}  // namespace card_registry
}  // namespace dominion
}  // namespace open_spiel
