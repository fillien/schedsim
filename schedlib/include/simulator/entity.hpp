#ifndef ENTITY_HPP
#define ENTITY_HPP

class Engine;

/**
 * @brief Represents an empty class that can be handled by the event system.
 */
class Entity {
      public:
        /**
         * @brief Constructs an entity with a reference to the engine.
         * @param sim Reference to the engine.
         */
        explicit Entity(Engine& sim) : engine_(sim) {}

        Entity(const Entity&) = delete;
        Entity& operator=(const Entity&) = delete;
        Entity(Entity&&) = delete;
        Entity& operator=(Entity&&) = delete;

        /**
         * @brief Retrieves a reference to the engine.
         * @return Reference to the engine.
         */
        [[nodiscard]] auto sim() -> Engine& { return engine_; }
        [[nodiscard]] auto sim() const -> const Engine& { return engine_; }

      private:
        Engine& engine_;
};

#endif
