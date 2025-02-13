#ifndef ENTITY_HPP
#define ENTITY_HPP

#include <cassert>
#include <memory>

class engine;

/**
 * @brief Represents an empty class that can be handled by the event system.
 */
class Entity {
      public:
        std::weak_ptr<engine> simulator; /**< Weak pointer to the engine for event handling. */

        /**
         * @brief Constructs an entity with a weak pointer to the engine.
         * @param sim Weak pointer to the engine.
         */
        explicit Entity(const std::weak_ptr<engine> sim) : simulator(sim) {}

        /**
         * @brief Retrieves a shared pointer to the engine.
         * @return Shared pointer to the engine.
         *
         * Asserts that the weak pointer to the engine is still valid.
         */
        [[nodiscard]] auto sim() const -> std::shared_ptr<engine>
        {
                assert(!simulator.expired());
                return simulator.lock();
        }
};

#endif
