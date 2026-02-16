#include <schedsim/core/platform.hpp>
#include <schedsim/core/engine.hpp>
#include <schedsim/core/error.hpp>

#include <memory>
#include <utility>

namespace schedsim::core {

Platform::Platform(Engine& engine)
    : engine_(engine) {}

ProcessorType& Platform::add_processor_type(std::string_view name, double performance,
                                             Duration context_switch_delay) {
    if (finalized_) {
        throw AlreadyFinalizedError("Cannot add processor type after finalize()");
    }

    std::size_t id = processor_types_.size();
    processor_types_.push_back(std::make_unique<ProcessorType>(id, name, performance,
                                                                context_switch_delay));
    return *processor_types_.back();
}

ClockDomain& Platform::add_clock_domain(Frequency freq_min, Frequency freq_max,
                                         Duration transition_delay) {
    if (finalized_) {
        throw AlreadyFinalizedError("Cannot add clock domain after finalize()");
    }

    std::size_t id = clock_domains_.size();
    clock_domains_.push_back(std::make_unique<ClockDomain>(id, freq_min, freq_max, transition_delay));
    clock_domains_.back()->set_engine(&engine_);
    return *clock_domains_.back();
}

PowerDomain& Platform::add_power_domain(std::vector<CStateLevel> c_states) {
    if (finalized_) {
        throw AlreadyFinalizedError("Cannot add power domain after finalize()");
    }

    std::size_t id = power_domains_.size();
    power_domains_.push_back(std::make_unique<PowerDomain>(id, std::move(c_states)));
    return *power_domains_.back();
}

Processor& Platform::add_processor(ProcessorType& type, ClockDomain& clock_domain,
                                    PowerDomain& power_domain) {
    if (finalized_) {
        throw AlreadyFinalizedError("Cannot add processor after finalize()");
    }

    std::size_t id = processors_.size();
    processors_.push_back(std::make_unique<Processor>(id, type, clock_domain, power_domain));

    Processor& proc = *processors_.back();
    proc.set_engine(&engine_);

    // Wire processor to clock and power domains
    clock_domain.add_processor(&proc);
    power_domain.add_processor(&proc);

    return proc;
}

Task& Platform::add_task(Duration period, Duration relative_deadline, Duration wcet) {
    if (finalized_) {
        throw AlreadyFinalizedError("Cannot add task after finalize()");
    }

    std::size_t id = tasks_.size();
    tasks_.push_back(std::make_unique<Task>(id, period, relative_deadline, wcet));
    return *tasks_.back();
}

Task& Platform::add_task(std::size_t id, Duration period, Duration relative_deadline, Duration wcet) {
    if (finalized_) {
        throw AlreadyFinalizedError("Cannot add task after finalize()");
    }

    tasks_.push_back(std::make_unique<Task>(id, period, relative_deadline, wcet));
    return *tasks_.back();
}

void Platform::finalize() {
    if (finalized_) {
        return;  // Idempotent
    }

    // Calculate reference performance (highest performance value)
    // Invariant: At least one processor type has performance = 1.0 (the reference type)
    // All performance values are in range (0, 1]
    reference_performance_ = 1.0;
    for (const auto& pt : processor_types_) {
        if (pt->performance() > reference_performance_) {
            reference_performance_ = pt->performance();
        }
    }

    // Set reference performance on all processors
    for (auto& proc : processors_) {
        proc->set_reference_performance(reference_performance_);
    }

    finalized_ = true;
}

} // namespace schedsim::core
