/* SWIG interface file for pyschedsim - Python bindings for schedsim three-library architecture */

%module pyschedsim

%{
// Standard library headers
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// Core library headers
#include <schedsim/core/types.hpp>
#include <schedsim/core/error.hpp>
#include <schedsim/core/engine.hpp>
#include <schedsim/core/platform.hpp>
#include <schedsim/core/processor.hpp>
#include <schedsim/core/processor_type.hpp>
#include <schedsim/core/clock_domain.hpp>
#include <schedsim/core/power_domain.hpp>
#include <schedsim/core/task.hpp>
#include <schedsim/core/job.hpp>

// Algo library headers
#include <schedsim/algo/error.hpp>
#include <schedsim/algo/scheduler.hpp>
#include <schedsim/algo/edf_scheduler.hpp>
#include <schedsim/algo/single_scheduler_allocator.hpp>

// IO library headers
#include <schedsim/io/error.hpp>
#include <schedsim/io/platform_loader.hpp>
#include <schedsim/io/scenario_loader.hpp>
#include <schedsim/io/scenario_injection.hpp>
%}

// Include standard SWIG typemaps for STL types
%include "std_string.i"
%include "std_vector.i"
%include "stdint.i"

// Include custom typemaps for schedsim types
%include "core_types.i"

// ============================================================================
// Exception handling - map C++ exceptions to Python exceptions
// ============================================================================

%exception {
    try {
        $action
    } catch (const schedsim::core::InvalidStateError& e) {
        SWIG_exception(SWIG_RuntimeError, e.what());
    } catch (const schedsim::core::AlreadyFinalizedError& e) {
        SWIG_exception(SWIG_RuntimeError, e.what());
    } catch (const schedsim::core::OutOfRangeError& e) {
        SWIG_exception(SWIG_ValueError, e.what());
    } catch (const schedsim::core::HandlerAlreadySetError& e) {
        SWIG_exception(SWIG_RuntimeError, e.what());
    } catch (const schedsim::algo::AdmissionError& e) {
        SWIG_exception(SWIG_ValueError, e.what());
    } catch (const schedsim::io::LoaderError& e) {
        SWIG_exception(SWIG_IOError, e.what());
    } catch (const std::invalid_argument& e) {
        SWIG_exception(SWIG_ValueError, e.what());
    } catch (const std::out_of_range& e) {
        SWIG_exception(SWIG_IndexError, e.what());
    } catch (const std::runtime_error& e) {
        SWIG_exception(SWIG_RuntimeError, e.what());
    } catch (const std::exception& e) {
        SWIG_exception(SWIG_RuntimeError, e.what());
    }
}

// ============================================================================
// Core types - tell SWIG these are fully qualified types
// ============================================================================

// Make Duration, TimePoint, Frequency, etc. opaque - typemaps handle conversion
%ignore schedsim::core::Duration;
%ignore schedsim::core::TimePoint;
%ignore schedsim::core::Frequency;
%ignore schedsim::core::Power;
%ignore schedsim::core::Energy;

// CStateLevel - don't expose as a class, just use the typemap for vector<CStateLevel>
%ignore schedsim::core::CStateLevel;

// ============================================================================
// Core types declarations - no constructors (obtained from factories only)
// ============================================================================

namespace schedsim {
namespace core {

// Enums
enum class ProcessorState {
    Idle,
    ContextSwitching,
    Running,
    Sleep,
    Changing
};

enum class CStateScope {
    PerProcessor,
    DomainWide
};

// ProcessorType class - no constructor exposed
%nodefaultctor ProcessorType;
class ProcessorType {
public:
    std::size_t id() const;
    double performance() const;
};

// ClockDomain class - no constructor exposed
%nodefaultctor ClockDomain;
class ClockDomain {
public:
    std::size_t id() const;
    schedsim::core::Frequency frequency() const;
    schedsim::core::Frequency freq_min() const;
    schedsim::core::Frequency freq_max() const;
    bool is_locked() const;
    bool is_transitioning() const;
};

// PowerDomain class - no constructor exposed
%nodefaultctor PowerDomain;
class PowerDomain {
public:
    std::size_t id() const;
};

// Processor class - no constructor exposed
%nodefaultctor Processor;
class Processor {
public:
    std::size_t id() const;
    ProcessorType& type() const;
    ProcessorState state() const;
    ClockDomain& clock_domain() const;
    PowerDomain& power_domain() const;
    int current_cstate_level() const;
};

// Task class - no constructor exposed
%nodefaultctor Task;
class Task {
public:
    std::size_t id() const;
    schedsim::core::Duration period() const;
    schedsim::core::Duration relative_deadline() const;
    schedsim::core::Duration wcet() const;
};

// Job class - no constructor exposed
%nodefaultctor Job;
class Job {
public:
    Task& task() const;
    schedsim::core::Duration remaining_work() const;
    schedsim::core::Duration total_work() const;
    schedsim::core::TimePoint absolute_deadline() const;
    bool is_complete() const;
};

// Platform class - no constructor exposed (obtained from Engine)
%nodefaultctor Platform;
class Platform {
public:
    // Factory methods - provide both overloads explicitly
    ProcessorType& add_processor_type(std::string_view name, double performance,
                                       schedsim::core::Duration context_switch_delay);
    ProcessorType& add_processor_type(std::string_view name, double performance);

    ClockDomain& add_clock_domain(schedsim::core::Frequency freq_min, schedsim::core::Frequency freq_max,
                                   schedsim::core::Duration transition_delay);
    ClockDomain& add_clock_domain(schedsim::core::Frequency freq_min, schedsim::core::Frequency freq_max);

    Processor& add_processor(ProcessorType& type, ClockDomain& clock_domain,
                             PowerDomain& power_domain);

    Task& add_task(schedsim::core::Duration period, schedsim::core::Duration relative_deadline, schedsim::core::Duration wcet);

    // add_power_domain exposed via %extend below

    // Collection sizes
    std::size_t processor_type_count() const;
    std::size_t processor_count() const;
    std::size_t clock_domain_count() const;
    std::size_t power_domain_count() const;
    std::size_t task_count() const;

    // Indexed access
    ProcessorType& processor_type(std::size_t idx);
    Processor& processor(std::size_t idx);
    ClockDomain& clock_domain(std::size_t idx);
    PowerDomain& power_domain(std::size_t idx);
    Task& task(std::size_t idx);

    double reference_performance() const;

    void finalize();
    bool is_finalized() const;

    // Wrapper for add_power_domain that takes Python list of tuples
    %extend {
        PowerDomain& add_power_domain(PyObject* cstates_list) {
            if (!PyList_Check(cstates_list)) {
                throw std::invalid_argument("Expected a list of (level, scope, wake_latency, power) tuples");
            }
            std::vector<schedsim::core::CStateLevel> c_states;
            Py_ssize_t size = PyList_Size(cstates_list);
            c_states.reserve(size);
            for (Py_ssize_t i = 0; i < size; ++i) {
                PyObject* item = PyList_GetItem(cstates_list, i);
                if (!PyTuple_Check(item) || PyTuple_Size(item) != 4) {
                    throw std::invalid_argument("Expected (level, scope, wake_latency, power) tuple");
                }
                int level = static_cast<int>(PyLong_AsLong(PyTuple_GetItem(item, 0)));
                int scope_int = static_cast<int>(PyLong_AsLong(PyTuple_GetItem(item, 1)));
                double wake_latency = PyFloat_AsDouble(PyTuple_GetItem(item, 2));
                double power = PyFloat_AsDouble(PyTuple_GetItem(item, 3));

                schedsim::core::CStateScope scope = (scope_int == 0)
                    ? schedsim::core::CStateScope::PerProcessor
                    : schedsim::core::CStateScope::DomainWide;

                c_states.push_back(schedsim::core::CStateLevel{
                    level, scope,
                    schedsim::core::Duration{wake_latency},
                    schedsim::core::Power{power}
                });
            }
            return $self->add_power_domain(std::move(c_states));
        }
    }
};

// Engine class - this one has a public constructor
class Engine {
public:
    Engine();
    ~Engine();

    schedsim::core::TimePoint time() const;

    void run();
    void run(schedsim::core::TimePoint until);

    void finalize();
    bool is_finalized() const;

    void schedule_job_arrival(Task& task, schedsim::core::TimePoint arrival_time, schedsim::core::Duration exec_time);

    void enable_context_switch(bool enabled);
    bool context_switch_enabled() const;

    void enable_energy_tracking(bool enabled);
    bool energy_tracking_enabled() const;

    schedsim::core::Energy processor_energy(std::size_t proc_id) const;
    schedsim::core::Energy clock_domain_energy(std::size_t cd_id) const;
    schedsim::core::Energy power_domain_energy(std::size_t pd_id) const;
    schedsim::core::Energy total_energy() const;

    %extend {
        Platform& get_platform() {
            return $self->platform();
        }
    }
};

} // namespace core
} // namespace schedsim

// ============================================================================
// Algo library declarations
// ============================================================================

namespace schedsim {
namespace algo {

// DeadlineMissPolicy enum
enum class DeadlineMissPolicy {
    Continue,
    AbortJob,
    AbortTask,
    StopSimulation
};

// CbsServer - no constructor exposed
%nodefaultctor CbsServer;
class CbsServer;

// Abstract Scheduler class - needed for SingleSchedulerAllocator
%nodefaultctor Scheduler;
class Scheduler {
public:
    virtual ~Scheduler();
};

// EdfScheduler class - use %extend for the constructor to handle vector<Processor*>
%nodefaultctor EdfScheduler;
class EdfScheduler : public Scheduler {
public:
    ~EdfScheduler();

    bool can_admit(schedsim::core::Duration budget, schedsim::core::Duration period) const;
    double utilization() const;
    std::size_t processor_count() const;
    std::size_t server_count() const;

    // Server management
    CbsServer& add_server(schedsim::core::Task& task, schedsim::core::Duration budget, schedsim::core::Duration period);
    CbsServer& add_server(schedsim::core::Task& task);
};

// Extend with Python-friendly constructor
%extend EdfScheduler {
    EdfScheduler(schedsim::core::Engine& engine, PyObject* proc_list) {
        if (!PyList_Check(proc_list)) {
            throw std::invalid_argument("Expected a list of Processor objects");
        }
        std::vector<schedsim::core::Processor*> processors;
        Py_ssize_t size = PyList_Size(proc_list);
        processors.reserve(size);
        for (Py_ssize_t i = 0; i < size; ++i) {
            PyObject* item = PyList_GetItem(proc_list, i);
            void* ptr = nullptr;
            swig_type_info* ty = SWIG_TypeQuery("schedsim::core::Processor *");
            if (!ty) {
                throw std::runtime_error("Could not find SWIG type for Processor");
            }
            int res = SWIG_ConvertPtr(item, &ptr, ty, 0);
            if (!SWIG_IsOK(res)) {
                throw std::invalid_argument("List item is not a Processor");
            }
            processors.push_back(reinterpret_cast<schedsim::core::Processor*>(ptr));
        }
        return new schedsim::algo::EdfScheduler(engine, std::move(processors));
    }
}

// SingleSchedulerAllocator class
class SingleSchedulerAllocator {
public:
    SingleSchedulerAllocator(schedsim::core::Engine& engine, Scheduler& scheduler);
    ~SingleSchedulerAllocator();
};

} // namespace algo
} // namespace schedsim

// ============================================================================
// IO library declarations
// ============================================================================

namespace schedsim {
namespace io {

// JobParams struct
struct JobParams {
    schedsim::core::TimePoint arrival;
    schedsim::core::Duration duration;
};

// TaskParams struct
struct TaskParams {
    uint64_t id;
    schedsim::core::Duration period;
    schedsim::core::Duration relative_deadline;
    schedsim::core::Duration wcet;
    std::vector<JobParams> jobs;
};

// ScenarioData struct
struct ScenarioData {
    std::vector<TaskParams> tasks;
};

// Platform loading functions
void load_platform(schedsim::core::Engine& engine, const std::filesystem::path& path);
void load_platform_from_string(schedsim::core::Engine& engine, std::string_view json);

// Scenario loading functions
ScenarioData load_scenario(const std::filesystem::path& path);
ScenarioData load_scenario_from_string(std::string_view json);

// Scenario injection functions
std::vector<schedsim::core::Task*> inject_scenario(schedsim::core::Engine& engine, const ScenarioData& scenario);
void schedule_arrivals(schedsim::core::Engine& engine, schedsim::core::Task& task,
                       const std::vector<JobParams>& jobs);

// Scenario writing functions
void write_scenario(const ScenarioData& scenario, const std::filesystem::path& path);

} // namespace io
} // namespace schedsim

// ============================================================================
// Template instantiations
// ============================================================================

namespace std {
    %template(TaskParamsVector) vector<schedsim::io::TaskParams>;
    %template(JobParamsVector) vector<schedsim::io::JobParams>;
    %template(TaskPtrVector) vector<schedsim::core::Task*>;
}

// ============================================================================
// Python helper code
// ============================================================================

%pythoncode %{
# Convenience constants for CStateScope
CSTATE_PER_PROCESSOR = CStateScope_PerProcessor
CSTATE_DOMAIN_WIDE = CStateScope_DomainWide

# Convenience constants for ProcessorState
PROC_IDLE = ProcessorState_Idle
PROC_CONTEXT_SWITCHING = ProcessorState_ContextSwitching
PROC_RUNNING = ProcessorState_Running
PROC_SLEEP = ProcessorState_Sleep
PROC_CHANGING = ProcessorState_Changing

def get_all_processors(engine):
    """Helper to get list of all processors from an engine."""
    platform = engine.get_platform()
    return [platform.processor(i) for i in range(platform.processor_count())]

def get_all_tasks(engine):
    """Helper to get list of all tasks from an engine."""
    platform = engine.get_platform()
    return [platform.task(i) for i in range(platform.task_count())]
%}
