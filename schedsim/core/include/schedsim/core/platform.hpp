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

/// @brief Container for all hardware resources in the simulation.
///
/// The Platform owns processor types, clock domains, power domains,
/// processors, and tasks. Factory methods (add_*) must be called
/// before Engine::finalize(). After finalization, collections are
/// locked and indexed access methods become valid.
///
/// Internally uses `vector<unique_ptr<T>>` for contiguous pointer
/// storage (good cache locality for iteration) with stable references
/// (objects do not move when the vector grows).
///
/// @see Engine, Processor, ClockDomain, PowerDomain, Task
/// @ingroup core_hardware
class Platform {
public:
    /// @brief Construct a Platform owned by the given engine.
    /// @param engine The simulation engine that owns this platform.
    explicit Platform(Engine& engine);

    /// @name Factory Methods
    /// @brief Add hardware components. Must be called before finalize().
    /// @{

    /// @brief Add a processor type.
    /// @param name Display name (e.g. "Cortex-A15").
    /// @param performance Relative performance factor (1.0 = reference).
    /// @param context_switch_delay Overhead per context switch.
    /// @return Reference to the created ProcessorType.
    ProcessorType& add_processor_type(std::string_view name, double performance,
                                       Duration context_switch_delay = duration_from_seconds(0.0));

    /// @brief Add a clock domain with frequency range.
    /// @param freq_min Minimum operating frequency.
    /// @param freq_max Maximum operating frequency.
    /// @param transition_delay Time to complete a frequency change.
    /// @return Reference to the created ClockDomain.
    ClockDomain& add_clock_domain(Frequency freq_min, Frequency freq_max,
                                   Duration transition_delay = duration_from_seconds(0.0));

    /// @brief Add a power domain with C-state levels.
    /// @param c_states Vector of available C-state levels (C0, C1, ...).
    /// @return Reference to the created PowerDomain.
    PowerDomain& add_power_domain(std::vector<CStateLevel> c_states);

    /// @brief Add a processor assigned to a type, clock domain, and power domain.
    /// @param type Processor type describing the micro-architecture.
    /// @param clock_domain Clock domain for frequency scaling.
    /// @param power_domain Power domain for C-state management.
    /// @return Reference to the created Processor.
    Processor& add_processor(ProcessorType& type, ClockDomain& clock_domain,
                             PowerDomain& power_domain);

    /// @brief Add a task with auto-assigned ID.
    /// @param period Task period.
    /// @param relative_deadline Relative deadline (from arrival).
    /// @param wcet Worst-case execution time.
    /// @return Reference to the created Task.
    Task& add_task(Duration period, Duration relative_deadline, Duration wcet);

    /// @brief Add a task with an explicit ID (for matching scenario JSON).
    /// @param id Task identifier (typically 1-based from JSON).
    /// @param period Task period.
    /// @param relative_deadline Relative deadline (from arrival).
    /// @param wcet Worst-case execution time.
    /// @return Reference to the created Task.
    Task& add_task(std::size_t id, Duration period, Duration relative_deadline, Duration wcet);

    /// @}

    /// @name Collection Sizes
    /// @{

    /// @brief Return the number of processor types.
    /// @return Processor type count.
    [[nodiscard]] std::size_t processor_type_count() const noexcept { return processor_types_.size(); }
    /// @brief Return the number of processors.
    /// @return Processor count.
    [[nodiscard]] std::size_t processor_count() const noexcept { return processors_.size(); }
    /// @brief Return the number of clock domains.
    /// @return Clock domain count.
    [[nodiscard]] std::size_t clock_domain_count() const noexcept { return clock_domains_.size(); }
    /// @brief Return the number of power domains.
    /// @return Power domain count.
    [[nodiscard]] std::size_t power_domain_count() const noexcept { return power_domains_.size(); }
    /// @brief Return the number of tasks.
    /// @return Task count.
    [[nodiscard]] std::size_t task_count() const noexcept { return tasks_.size(); }

    /// @}

    /// @name Indexed Access
    /// @{

    /// @brief Access a processor type by index.
    /// @param idx Zero-based index.
    /// @return Reference to the ProcessorType.
    [[nodiscard]] ProcessorType& processor_type(std::size_t idx) { return *processor_types_[idx]; }
    /// @brief Access a processor by index.
    /// @param idx Zero-based index.
    /// @return Reference to the Processor.
    [[nodiscard]] Processor& processor(std::size_t idx) { return *processors_[idx]; }
    /// @brief Access a clock domain by index.
    /// @param idx Zero-based index.
    /// @return Reference to the ClockDomain.
    [[nodiscard]] ClockDomain& clock_domain(std::size_t idx) { return *clock_domains_[idx]; }
    /// @brief Access a power domain by index.
    /// @param idx Zero-based index.
    /// @return Reference to the PowerDomain.
    [[nodiscard]] PowerDomain& power_domain(std::size_t idx) { return *power_domains_[idx]; }
    /// @brief Access a task by index.
    /// @param idx Zero-based index.
    /// @return Reference to the Task.
    [[nodiscard]] Task& task(std::size_t idx) { return *tasks_[idx]; }

    /// @brief Access a processor type by index (const).
    /// @param idx Zero-based index.
    /// @return Const reference to the ProcessorType.
    [[nodiscard]] const ProcessorType& processor_type(std::size_t idx) const { return *processor_types_[idx]; }
    /// @brief Access a processor by index (const).
    /// @param idx Zero-based index.
    /// @return Const reference to the Processor.
    [[nodiscard]] const Processor& processor(std::size_t idx) const { return *processors_[idx]; }
    /// @brief Access a clock domain by index (const).
    /// @param idx Zero-based index.
    /// @return Const reference to the ClockDomain.
    [[nodiscard]] const ClockDomain& clock_domain(std::size_t idx) const { return *clock_domains_[idx]; }
    /// @brief Access a power domain by index (const).
    /// @param idx Zero-based index.
    /// @return Const reference to the PowerDomain.
    [[nodiscard]] const PowerDomain& power_domain(std::size_t idx) const { return *power_domains_[idx]; }
    /// @brief Access a task by index (const).
    /// @param idx Zero-based index.
    /// @return Const reference to the Task.
    [[nodiscard]] const Task& task(std::size_t idx) const { return *tasks_[idx]; }

    /// @}

    /// @brief Returns the reference performance used for heterogeneous scaling.
    /// @return Reference performance factor.
    [[nodiscard]] double reference_performance() const noexcept { return reference_performance_; }

    /// @brief Finalize the platform, locking all collections.
    void finalize();

    /// @brief Returns true if the platform has been finalized.
    /// @return True after finalize() has been called.
    [[nodiscard]] bool is_finalized() const noexcept { return finalized_; }

    Platform(const Platform&) = delete;
    Platform& operator=(const Platform&) = delete;
    Platform(Platform&&) = delete;
    Platform& operator=(Platform&&) = delete;

private:
    Engine& engine_;
    bool finalized_{false};
    double reference_performance_{1.0};

    std::vector<std::unique_ptr<ProcessorType>> processor_types_;
    std::vector<std::unique_ptr<ClockDomain>> clock_domains_;
    std::vector<std::unique_ptr<PowerDomain>> power_domains_;
    std::vector<std::unique_ptr<Processor>> processors_;
    std::vector<std::unique_ptr<Task>> tasks_;
};

} // namespace schedsim::core
