#ifndef ENTITY_HPP
#define ENTITY_HPP

#include <cassert>
#include <memory>

class engine;

/**
 * @brief A empty class that can be handled by the event system.
 */
class entity {
public:
	std::weak_ptr<engine> simulator;

	entity(const std::weak_ptr<engine> sim): simulator(sim) {}

	auto sim() const -> std::shared_ptr<engine> {
                assert(!simulator.expired());
                return simulator.lock();
        }
};

#endif
