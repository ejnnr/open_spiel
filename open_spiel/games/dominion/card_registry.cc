#include "card_registry.h"

#include <algorithm>  // for transform
#include <string>
#include <vector>

namespace open_spiel {
namespace dominion {
namespace {
std::unordered_map<std::string, Card *> registry{};
std::vector<std::unique_ptr<Card>> registry_vector{};
std::unordered_map<std::string, size_t> name_to_id{};

// Helper function to convert string to lowercase
std::string to_lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return s;
}
}  // namespace

namespace card_registry {
void init() {
  // Guard against multiple initializations
  if (!registry_vector.empty()) {
    return;
  }

  registry_vector.push_back(std::make_unique<Copper>());
  registry_vector.push_back(std::make_unique<Silver>());
  registry_vector.push_back(std::make_unique<Gold>());
  registry_vector.push_back(std::make_unique<Estate>());
  registry_vector.push_back(std::make_unique<Duchy>());
  registry_vector.push_back(std::make_unique<Province>());
  registry_vector.push_back(std::make_unique<Curse>());
  registry_vector.push_back(std::make_unique<Gardens>());
  registry_vector.push_back(std::make_unique<Village>());
  registry_vector.push_back(std::make_unique<Woodcutter>());
  registry_vector.push_back(std::make_unique<Smithy>());
  registry_vector.push_back(std::make_unique<Market>());
  registry_vector.push_back(std::make_unique<Festival>());
  registry_vector.push_back(std::make_unique<Laboratory>());
  registry_vector.push_back(std::make_unique<CouncilRoom>());
  registry_vector.push_back(std::make_unique<Workshop>());
  registry_vector.push_back(std::make_unique<Chapel>());
  registry_vector.push_back(std::make_unique<Cellar>());
  registry_vector.push_back(std::make_unique<Moneylender>());
  registry_vector.push_back(std::make_unique<Remodel>());
  registry_vector.push_back(std::make_unique<ThroneRoom>());
  registry_vector.push_back(std::make_unique<Library>());
  registry_vector.push_back(std::make_unique<Witch>());

  for (size_t i = 0; i < registry_vector.size(); ++i) {
    Card *card = registry_vector[i].get();
    std::string lower_name = to_lower(card->name);
    registry[lower_name] = card;
    name_to_id[lower_name] = i;
  }
}

Card *get(const std::string &name) {
  auto it = registry.find(to_lower(name));
  if (it == registry.end()) {
    throw std::runtime_error("Card not found: " + name);
  }
  return it->second;
}

Card *get(size_t id) {
  if (id >= registry_vector.size()) {
    throw std::runtime_error("Invalid card id: " + std::to_string(id));
  }
  return registry_vector[id].get();
}

bool exists(const std::string &name) {
  return registry.find(to_lower(name)) != registry.end();
}

bool exists(size_t id) { return id < registry_vector.size(); }

size_t get_id(const std::string &name) {
  auto it = name_to_id.find(to_lower(name));
  if (it == name_to_id.end()) {
    throw std::runtime_error("Card not found: " + name);
  }
  return it->second;
}

size_t num_cards() { return registry_vector.size(); }
}  // namespace card_registry
}  // namespace dominion
}  // namespace open_spiel
