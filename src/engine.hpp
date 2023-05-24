#ifndef ENGINE_HPP
#define ENGINE_HPP

#include "event.hpp"
#include "plateform.hpp"
#include "scheduler.hpp"
#include "tracer.hpp"

#include <cstddef>
#include <cstdio>
#include <map>
#include <memory>

/**
 * @brief According to a platform and a scheduler, the class simulate in order of time each events
 * contains in the future list
 */
class engine : public std::enable_shared_from_this<engine> {
        void handle(const event& evt);

      public:
        /**
         * @brief The attached scheduler.
         */
        std::shared_ptr<scheduler> sched;

        /**
         * @brief A model a the platform on which the scheduler will operate.
         */
        plateform current_plateform;

        /**
         * @brief The tracer that help to generate logs.
         */
        tracer logging_system;

        /**
         * @brief The list of future events, it's a pair of the timestamp of the event and the event
         * to process himself.
         */
        std::multimap<double, const event> future_list{};

        /**
         * @brief A counter of the time passing.
         */
        double current_timestamp{0};

        /**
         * @brief A constructor to help generate the platform.
         * @param nb_processors The number of processors that compose the platform model
         */
        explicit engine(const size_t nb_processors);

        /**
         * @brief Setter to attach a scheduler.
         * @param new_sched
         */
        void set_scheduler(std::shared_ptr<scheduler>& new_sched);

        /**
         * @brief Retrieve a pointer of the engine object, its help a scheduler object to access
         * data of the engine.
         */
        auto get_shared() -> std::shared_ptr<engine> { return shared_from_this(); };

        /**
         * @brief That the main function of the simulator engine, its the event loop that handle the
         * events.
         */
        void simulation();

        /**
         * @brief A setter to add new event to the the future list.
         */
        void add_event(const event& new_evt, const double timestamp);
};

#endif
