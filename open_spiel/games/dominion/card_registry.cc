#include "card_registry.h"

#include <string>
#include <vector>

namespace open_spiel
{
    namespace dominion
    {
        namespace
        {
            std::unordered_map<std::string, Card *> registry{};
            std::vector<std::unique_ptr<Card>> registry_vector{};
            std::unordered_map<std::string, size_t> name_to_id{};
        }

        namespace card_registry
        {
            void init()
            {
                // Guard against multiple initializations
                if (!registry_vector.empty())
                {
                    return;
                }

                registry_vector.push_back(std::make_unique<Copper>());
                registry_vector.push_back(std::make_unique<Silver>());
                registry_vector.push_back(std::make_unique<Gold>());
                registry_vector.push_back(std::make_unique<Estate>());
                registry_vector.push_back(std::make_unique<Duchy>());
                registry_vector.push_back(std::make_unique<Province>());
                // registry_vector.push_back(std::make_unique<Village>());
                // registry_vector.push_back(std::make_unique<Woodcutter>());
                registry_vector.push_back(std::make_unique<Smithy>());
                // registry_vector.push_back(std::make_unique<Market>());
                // registry_vector.push_back(std::make_unique<Festival>());
                // registry_vector.push_back(std::make_unique<Laboratory>());
                registry_vector.push_back(std::make_unique<CouncilRoom>());
                registry_vector.push_back(std::make_unique<Workshop>());

                for (size_t i = 0; i < registry_vector.size(); ++i)
                {
                    Card *card = registry_vector[i].get();
                    registry[card->name] = card;
                    name_to_id[card->name] = i;
                }
            }

            Card *get(const std::string &name)
            {
                auto it = registry.find(name);
                if (it == registry.end())
                {
                    throw std::runtime_error("Card not found: " + name);
                }
                return it->second;
            }

            Card *get(size_t id)
            {
                if (id >= registry_vector.size())
                {
                    throw std::runtime_error("Invalid card id: " + std::to_string(id));
                }
                return registry_vector[id].get();
            }

            bool exists(const std::string &name)
            {
                return registry.find(name) != registry.end();
            }

            bool exists(size_t id)
            {
                return id < registry_vector.size();
            }

            size_t get_id(const std::string &name)
            {
                auto it = name_to_id.find(name);
                if (it == name_to_id.end())
                {
                    throw std::runtime_error("Card not found: " + name);
                }
                return it->second;
            }

            size_t num_cards()
            {
                return registry_vector.size();
            }
        }
    } // namespace dominion
} // namespace open_spiel
