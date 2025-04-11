#ifndef SCHED_DPMDVFS
#define SCHED_DPMDVFS

#include <simulator/schedulers/parallel.hpp>

namespace scheds {

/**
 * @brief Dynamic Power Management and Dynamic Voltage Frequency Scaling scheduler.
 *
 * This class implements a scheduler that combines Dynamic Power Management (DPM)
 * and Dynamic Voltage Frequency Scaling (DVFS) techniques to manage power consumption
 * in a cluster of processors. It inherits from the Parallel scheduler base class.
 */
class DpmDvfs : public Parallel {
      public:
        /**
         * @brief Constructor for the DpmDvfs scheduler.
         *
         * @param sim A weak pointer to the simulation engine.
         */
        explicit DpmDvfs(const std::weak_ptr<Engine> sim) : Parallel(sim) {}

        /**
         * @brief Clusters a given cluster of processors.
         *
         * This function performs clustering operations on the provided cluster,
         * inheriting from the base class functionality.
         *
         * @param clu A weak pointer to the cluster to be clustered.
         */
        auto cluster(const std::weak_ptr<Cluster>& clu) -> void { Parallel::cluster(clu); }

      protected:
        /**
         * @brief Computes the minimum frequency based on utilization and processor count.
         *
         * This function calculates the minimum allowable frequency for processors
         * considering maximum frequency, total utilization, maximum utilization,
         * and the number of processors.
         *
         * @param freq_max The maximum allowed frequency.
         * @param total_util The total utilization of all processors.
         * @param max_util The maximum allowable utilization per processor.
         * @param nb_procs The number of processors in the cluster.
         * @return The calculated minimum frequency.
         */
        auto compute_freq_min(
            const double& freq_max,
            const double& total_util,
            const double& max_util,
            const double& nb_procs) -> double;

        /**
         * @brief Adjusts the number of active processors in the cluster.
         *
         * This function sets the target number of active processors and adjusts
         * the processor states accordingly.
         *
         * @param target_processors The desired number of active processors.
         */
        void adjust_active_processors(const std::size_t target_processors);

        /**
         * @brief Activates the next core in the cluster.
         */
        void activate_next_core();

        /**
         * @brief Returns the number of active processors in the cluster.
         *
         * @return The number of currently active processors.
         */
        auto nb_active_procs() const -> std::size_t;

        /**
         * @brief DVFS cooldown period.
         *  Represents a delay for frequency scaling operations.
         */
        static constexpr double DVFS_COOLDOWN{Cluster::DVFS_DELAY * 2};

        /**
         * @brief DPM cooldown period.
         * Represents a delay for power management state transitions.
         */
        static constexpr double DPM_COOLDOWN{Processor::DPM_DELAY * 2};

      private:
        /**
         * @brief Changes the state of a processor.
         *
         * This function updates the state of a given processor to the specified next state.
         *
         * @param next_state The desired new state for the processor.
         * @param proc A shared pointer to the processor whose state is being changed.
         */
        void change_state_proc(
            const Processor::State& next_state, const std::shared_ptr<Processor>& proc);

        /**
         * @brief Puts the next core in the cluster into a sleep state (bed).
         */
        void put_next_core_to_bed();
};

} // namespace scheds

#endif