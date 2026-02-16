/* SWIG interface file for pyschedsim - Python bindings for schedsim three-library architecture */

%module pyschedsim

%{
// Standard library headers
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <string>
#include <variant>
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
#include <schedsim/core/trace_writer.hpp>

// Algo library headers
#include <schedsim/algo/error.hpp>
#include <schedsim/algo/scheduler.hpp>
#include <schedsim/algo/edf_scheduler.hpp>
#include <schedsim/algo/single_scheduler_allocator.hpp>
#include <schedsim/algo/cluster.hpp>

// IO library headers
#include <schedsim/io/error.hpp>
#include <schedsim/io/platform_loader.hpp>
#include <schedsim/io/scenario_loader.hpp>
#include <schedsim/io/scenario_injection.hpp>
#include <schedsim/io/trace_writers.hpp>
#include <schedsim/io/metrics.hpp>

// ==========================================================================
// FileJsonTraceWriter: owns the ofstream + JsonTraceWriter for Python use
// ==========================================================================
class FileJsonTraceWriter : public schedsim::core::TraceWriter {
public:
    explicit FileJsonTraceWriter(const std::string& filename)
        : stream_(filename), writer_(stream_) {
        if (!stream_.is_open()) {
            throw std::runtime_error("Cannot open file: " + filename);
        }
    }
    ~FileJsonTraceWriter() override { writer_.finalize(); }

    void begin(schedsim::core::TimePoint time) override { writer_.begin(time); }
    void type(std::string_view name) override { writer_.type(name); }
    void field(std::string_view key, double value) override { writer_.field(key, value); }
    void field(std::string_view key, uint64_t value) override { writer_.field(key, value); }
    void field(std::string_view key, std::string_view value) override { writer_.field(key, value); }
    void end() override { writer_.end(); }
    void finalize() { writer_.finalize(); }

private:
    std::ofstream stream_;
    schedsim::io::JsonTraceWriter writer_;
};
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
// Ignore problematic fields before declarations
// ============================================================================

// TraceRecord::fields uses variant - provide %extend accessors instead
%ignore schedsim::io::TraceRecord::fields;

// SimulationMetrics map/vector members - provide %extend accessors instead
%ignore schedsim::io::SimulationMetrics::energy_per_processor;
%ignore schedsim::io::SimulationMetrics::utilization_per_processor;
%ignore schedsim::io::SimulationMetrics::response_times_per_task;
%ignore schedsim::io::SimulationMetrics::deadline_misses_per_task;
%ignore schedsim::io::SimulationMetrics::waiting_times_per_task;
%ignore schedsim::io::SimulationMetrics::frequency_changes;

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

// TraceWriter abstract base class
%nodefaultctor TraceWriter;
class TraceWriter {
public:
    virtual ~TraceWriter();
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
    schedsim::core::Frequency freq_eff() const;
    void set_freq_eff(schedsim::core::Frequency freq);

    %extend {
        void set_power_coefficients(PyObject* coeffs_list) {
            if (!PyList_Check(coeffs_list) || PyList_Size(coeffs_list) != 4) {
                throw std::invalid_argument("Expected a list of 4 floats [a0, a1, a2, a3]");
            }
            std::array<double, 4> coeffs;
            for (Py_ssize_t i = 0; i < 4; ++i) {
                PyObject* item = PyList_GetItem(coeffs_list, i);
                coeffs[i] = PyFloat_AsDouble(item);
                if (PyErr_Occurred()) {
                    throw std::invalid_argument("All coefficients must be numeric");
                }
            }
            $self->set_power_coefficients(coeffs);
        }
    }
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
    Task& add_task(std::size_t id, schedsim::core::Duration period, schedsim::core::Duration relative_deadline, schedsim::core::Duration wcet);

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

    // Trace API
    void set_trace_writer(TraceWriter* writer);

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

    // Configuration
    void set_deadline_miss_policy(DeadlineMissPolicy policy);
    void set_expected_arrivals(const schedsim::core::Task& task, std::size_t count);

    // Policy convenience methods
    void enable_grub();
    void enable_cash();
    void enable_power_aware_dvfs(schedsim::core::Duration cooldown);
    void enable_basic_dpm(int target_cstate = 1);
    void enable_ffa(schedsim::core::Duration cooldown, int sleep_cstate = 1);
    void enable_csf(schedsim::core::Duration cooldown, int sleep_cstate = 1);
    void enable_ffa_timer(schedsim::core::Duration cooldown, int sleep_cstate = 1);
    void enable_csf_timer(schedsim::core::Duration cooldown, int sleep_cstate = 1);

    // Utilization queries
    double active_utilization() const;
    double scheduler_utilization() const;
    double max_scheduler_utilization() const;
    double max_server_utilization() const;
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

// Cluster class
class Cluster {
public:
    Cluster(schedsim::core::ClockDomain& clock_domain, Scheduler& scheduler,
            double perf_score, double reference_freq_max);

    schedsim::core::ClockDomain& clock_domain();
    Scheduler& scheduler();

    double perf() const;
    double scale_speed() const;
    double u_target() const;
    void set_u_target(double target);
    double scaled_utilization(double task_util) const;

    std::size_t processor_count() const;
    double utilization() const;
    bool can_admit(schedsim::core::Duration budget, schedsim::core::Duration period) const;
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

// --------------------------------------------------------------------------
// Trace writers
// --------------------------------------------------------------------------

// NullTraceWriter - discards all traces (for performance testing)
class NullTraceWriter : public schedsim::core::TraceWriter {
public:
    NullTraceWriter();
    ~NullTraceWriter() override;
};

// TraceRecord - structured in-memory trace record
struct TraceRecord {
    double time;
    std::string type;
    // fields member ignored - use get_field/get_fields_dict %extend methods
};

%extend TraceRecord {
    PyObject* get_field(const std::string& key) {
        auto it = $self->fields.find(key);
        if (it == $self->fields.end()) {
            Py_RETURN_NONE;
        }
        return std::visit([](auto&& val) -> PyObject* {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_same_v<T, double>) {
                return PyFloat_FromDouble(val);
            } else if constexpr (std::is_same_v<T, uint64_t>) {
                return PyLong_FromUnsignedLongLong(val);
            } else if constexpr (std::is_same_v<T, std::string>) {
                return PyUnicode_FromString(val.c_str());
            }
        }, it->second);
    }

    PyObject* get_fields_dict() {
        PyObject* dict = PyDict_New();
        for (const auto& [key, val] : $self->fields) {
            PyObject* py_val = std::visit([](auto&& v) -> PyObject* {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, double>) {
                    return PyFloat_FromDouble(v);
                } else if constexpr (std::is_same_v<T, uint64_t>) {
                    return PyLong_FromUnsignedLongLong(v);
                } else if constexpr (std::is_same_v<T, std::string>) {
                    return PyUnicode_FromString(v.c_str());
                }
            }, val);
            PyDict_SetItemString(dict, key.c_str(), py_val);
            Py_DECREF(py_val);
        }
        return dict;
    }

    PyObject* get_field_names() {
        PyObject* list = PyList_New(0);
        for (const auto& [key, val] : $self->fields) {
            PyObject* py_key = PyUnicode_FromString(key.c_str());
            PyList_Append(list, py_key);
            Py_DECREF(py_key);
        }
        return list;
    }
}

// MemoryTraceWriter - buffers traces in memory
class MemoryTraceWriter : public schedsim::core::TraceWriter {
public:
    MemoryTraceWriter();
    ~MemoryTraceWriter() override;
    void clear();
};

%extend MemoryTraceWriter {
    std::size_t record_count() {
        return $self->records().size();
    }

    const schedsim::io::TraceRecord& record(std::size_t index) {
        if (index >= $self->records().size()) {
            throw std::out_of_range("TraceRecord index out of range");
        }
        return $self->records()[index];
    }

    // Convenience: compute metrics from this writer's recorded traces
    schedsim::io::SimulationMetrics compute_metrics() {
        return schedsim::io::compute_metrics($self->records());
    }

    // Time-series analysis on this writer's traces
    std::vector<schedsim::io::FrequencyInterval> track_frequency_changes() {
        return schedsim::io::track_frequency_changes($self->records());
    }

    std::vector<schedsim::io::CoreCountInterval> track_core_changes() {
        return schedsim::io::track_core_changes($self->records());
    }

    std::vector<schedsim::io::ConfigInterval> track_config_changes() {
        return schedsim::io::track_config_changes($self->records());
    }
}

// --------------------------------------------------------------------------
// Metrics
// --------------------------------------------------------------------------

// SimulationMetrics struct - scalar fields directly accessible,
// map/vector fields via %extend methods
struct SimulationMetrics {
    uint64_t total_jobs;
    uint64_t completed_jobs;
    uint64_t deadline_misses;
    uint64_t preemptions;
    uint64_t context_switches;
    double total_energy_mj;
    double average_utilization;
    uint64_t rejected_tasks;
    uint64_t cluster_migrations;
    uint64_t transitions;
    uint64_t core_state_requests;
    uint64_t frequency_requests;
};

%extend SimulationMetrics {
    // energy_per_processor -> Python dict {proc_id: energy_mj}
    PyObject* get_energy_per_processor() {
        PyObject* dict = PyDict_New();
        for (const auto& [proc_id, energy] : $self->energy_per_processor) {
            PyObject* key = PyLong_FromUnsignedLongLong(proc_id);
            PyObject* val = PyFloat_FromDouble(energy);
            PyDict_SetItem(dict, key, val);
            Py_DECREF(key);
            Py_DECREF(val);
        }
        return dict;
    }

    // utilization_per_processor -> Python dict {proc_id: utilization}
    PyObject* get_utilization_per_processor() {
        PyObject* dict = PyDict_New();
        for (const auto& [proc_id, util] : $self->utilization_per_processor) {
            PyObject* key = PyLong_FromUnsignedLongLong(proc_id);
            PyObject* val = PyFloat_FromDouble(util);
            PyDict_SetItem(dict, key, val);
            Py_DECREF(key);
            Py_DECREF(val);
        }
        return dict;
    }

    // response_times_per_task -> Python dict {task_id: [float, ...]}
    PyObject* get_response_times_per_task() {
        PyObject* dict = PyDict_New();
        for (const auto& [tid, times] : $self->response_times_per_task) {
            PyObject* key = PyLong_FromUnsignedLongLong(tid);
            PyObject* list = PyList_New(times.size());
            for (std::size_t i = 0; i < times.size(); ++i) {
                PyList_SET_ITEM(list, i, PyFloat_FromDouble(times[i]));
            }
            PyDict_SetItem(dict, key, list);
            Py_DECREF(key);
            Py_DECREF(list);
        }
        return dict;
    }

    // deadline_misses_per_task -> Python dict {task_id: count}
    PyObject* get_deadline_misses_per_task() {
        PyObject* dict = PyDict_New();
        for (const auto& [tid, count] : $self->deadline_misses_per_task) {
            PyObject* key = PyLong_FromUnsignedLongLong(tid);
            PyObject* val = PyLong_FromUnsignedLongLong(count);
            PyDict_SetItem(dict, key, val);
            Py_DECREF(key);
            Py_DECREF(val);
        }
        return dict;
    }

    // waiting_times_per_task -> Python dict {task_id: [float, ...]}
    PyObject* get_waiting_times_per_task() {
        PyObject* dict = PyDict_New();
        for (const auto& [tid, times] : $self->waiting_times_per_task) {
            PyObject* key = PyLong_FromUnsignedLongLong(tid);
            PyObject* list = PyList_New(times.size());
            for (std::size_t i = 0; i < times.size(); ++i) {
                PyList_SET_ITEM(list, i, PyFloat_FromDouble(times[i]));
            }
            PyDict_SetItem(dict, key, list);
            Py_DECREF(key);
            Py_DECREF(list);
        }
        return dict;
    }
}

// Compute metrics from JSON trace file
SimulationMetrics compute_metrics_from_file(const std::filesystem::path& path);

// Response time statistics
struct ResponseTimeStats {
    double min;
    double max;
    double mean;
    double median;
    double stddev;
    double percentile_95;
    double percentile_99;
};

// --------------------------------------------------------------------------
// Time-series structs
// --------------------------------------------------------------------------

struct FrequencyInterval {
    double start;
    double stop;
    double frequency;
    uint64_t cluster_id;
};

struct CoreCountInterval {
    double start;
    double stop;
    uint64_t active_cores;
    uint64_t cluster_id;
};

struct ConfigInterval {
    double start;
    double stop;
    double frequency;
    uint64_t active_cores;
};

} // namespace io
} // namespace schedsim

// FileJsonTraceWriter - writes JSON traces to a file (owns the file handle)
class FileJsonTraceWriter : public schedsim::core::TraceWriter {
public:
    FileJsonTraceWriter(const std::string& filename);
    ~FileJsonTraceWriter() override;
    void finalize();
};

// ============================================================================
// Template instantiations
// ============================================================================

namespace std {
    %template(TaskParamsVector) vector<schedsim::io::TaskParams>;
    %template(JobParamsVector) vector<schedsim::io::JobParams>;
    %template(TaskPtrVector) vector<schedsim::core::Task*>;
    %template(FrequencyIntervalVector) vector<schedsim::io::FrequencyInterval>;
    %template(CoreCountIntervalVector) vector<schedsim::io::CoreCountInterval>;
    %template(ConfigIntervalVector) vector<schedsim::io::ConfigInterval>;
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

# Convenience constants for DeadlineMissPolicy
DM_CONTINUE = DeadlineMissPolicy_Continue
DM_ABORT_JOB = DeadlineMissPolicy_AbortJob
DM_ABORT_TASK = DeadlineMissPolicy_AbortTask
DM_STOP_SIMULATION = DeadlineMissPolicy_StopSimulation

def get_all_processors(engine):
    """Helper to get list of all processors from an engine."""
    platform = engine.get_platform()
    return [platform.processor(i) for i in range(platform.processor_count())]

def get_all_tasks(engine):
    """Helper to get list of all tasks from an engine."""
    platform = engine.get_platform()
    return [platform.task(i) for i in range(platform.task_count())]
%}
