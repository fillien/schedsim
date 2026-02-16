#pragma once

#include <schedsim/core/clock_domain.hpp>
#include <schedsim/core/power_domain.hpp>
#include <schedsim/core/processor.hpp>
#include <schedsim/core/processor_type.hpp>
#include <schedsim/core/task.hpp>
#include <schedsim/core/types.hpp>

#include <memory>
#include <string_view>
#include <vector>

namespace schedsim::core {

class Engine;

// Platform is the container for all hardware resources in the simulation.
// Factory methods must be called before finalize(). After finalize(),
// collections are locked and access methods become valid.
//
// Uses vector<unique_ptr<T>> for:
// - Contiguous pointer storage (good cache locality for iteration)
// - Stable references (objects don't move when vector grows)
class Platform {
public:
    explicit Platform(Engine& engine);

    // Factory methods (before finalize)
    ProcessorType& add_processor_type(std::string_view name, double performance,
                                       Duration context_switch_delay = duration_from_seconds(0.0));
    ClockDomain& add_clock_domain(Frequency freq_min, Frequency freq_max,
                                   Duration transition_delay = duration_from_seconds(0.0));
    PowerDomain& add_power_domain(std::vector<CStateLevel> c_states);
    Processor& add_processor(ProcessorType& type, ClockDomain& clock_domain,
                             PowerDomain& power_domain);
    Task& add_task(Duration period, Duration relative_deadline, Duration wcet);
    Task& add_task(std::size_t id, Duration period, Duration relative_deadline, Duration wcet);

    // Collection sizes
    [[nodiscard]] std::size_t processor_type_count() const noexcept { return processor_types_.size(); }
    [[nodiscard]] std::size_t processor_count() const noexcept { return processors_.size(); }
    [[nodiscard]] std::size_t clock_domain_count() const noexcept { return clock_domains_.size(); }
    [[nodiscard]] std::size_t power_domain_count() const noexcept { return power_domains_.size(); }
    [[nodiscard]] std::size_t task_count() const noexcept { return tasks_.size(); }

    // Indexed access
    [[nodiscard]] ProcessorType& processor_type(std::size_t idx) { return *processor_types_[idx]; }
    [[nodiscard]] Processor& processor(std::size_t idx) { return *processors_[idx]; }
    [[nodiscard]] ClockDomain& clock_domain(std::size_t idx) { return *clock_domains_[idx]; }
    [[nodiscard]] PowerDomain& power_domain(std::size_t idx) { return *power_domains_[idx]; }
    [[nodiscard]] Task& task(std::size_t idx) { return *tasks_[idx]; }

    [[nodiscard]] const ProcessorType& processor_type(std::size_t idx) const { return *processor_types_[idx]; }
    [[nodiscard]] const Processor& processor(std::size_t idx) const { return *processors_[idx]; }
    [[nodiscard]] const ClockDomain& clock_domain(std::size_t idx) const { return *clock_domains_[idx]; }
    [[nodiscard]] const PowerDomain& power_domain(std::size_t idx) const { return *power_domains_[idx]; }
    [[nodiscard]] const Task& task(std::size_t idx) const { return *tasks_[idx]; }

    [[nodiscard]] double reference_performance() const noexcept { return reference_performance_; }

    // Finalization (Decision 47)
    void finalize();
    [[nodiscard]] bool is_finalized() const noexcept { return finalized_; }

    // Non-copyable, non-movable (holds Engine&)
    Platform(const Platform&) = delete;
    Platform& operator=(const Platform&) = delete;
    Platform(Platform&&) = delete;
    Platform& operator=(Platform&&) = delete;

private:
    Engine& engine_;
    bool finalized_{false};
    double reference_performance_{1.0};

    // vector<unique_ptr<T>> provides stable references with contiguous pointer storage
    std::vector<std::unique_ptr<ProcessorType>> processor_types_;
    std::vector<std::unique_ptr<ClockDomain>> clock_domains_;
    std::vector<std::unique_ptr<PowerDomain>> power_domains_;
    std::vector<std::unique_ptr<Processor>> processors_;
    std::vector<std::unique_ptr<Task>> tasks_;
};

} // namespace schedsim::core
