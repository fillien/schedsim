// nanobind Python bindings for schedsim three-library architecture

#include <nanobind/nanobind.h>
#include <nanobind/stl/filesystem.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/string_view.h>
#include <nanobind/stl/vector.h>

// Standard library
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <variant>
#include <vector>

// Core library
#include <schedsim/core/clock_domain.hpp>
#include <schedsim/core/engine.hpp>
#include <schedsim/core/error.hpp>
#include <schedsim/core/job.hpp>
#include <schedsim/core/platform.hpp>
#include <schedsim/core/power_domain.hpp>
#include <schedsim/core/processor.hpp>
#include <schedsim/core/processor_type.hpp>
#include <schedsim/core/task.hpp>
#include <schedsim/core/trace_writer.hpp>
#include <schedsim/core/types.hpp>

// Algo library
#include <schedsim/algo/cbs_server.hpp>
#include <schedsim/algo/cluster.hpp>
#include <schedsim/algo/edf_scheduler.hpp>
#include <schedsim/algo/error.hpp>
#include <schedsim/algo/scheduler.hpp>
#include <schedsim/algo/single_scheduler_allocator.hpp>

// IO library
#include <schedsim/io/error.hpp>
#include <schedsim/io/metrics.hpp>
#include <schedsim/io/platform_loader.hpp>
#include <schedsim/io/scenario_injection.hpp>
#include <schedsim/io/scenario_loader.hpp>
#include <schedsim/io/trace_writers.hpp>

namespace nb = nanobind;
using namespace nb::literals;

// ============================================================================
// FileJsonTraceWriter: owns the ofstream + JsonTraceWriter for Python use
// ============================================================================

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

// ============================================================================
// Custom type casters for strong types
// ============================================================================

namespace nanobind::detail {

// Duration <-> float (seconds)
template <> struct type_caster<schedsim::core::Duration> {
    NB_TYPE_CASTER(schedsim::core::Duration, const_name("float"))

    bool from_python(handle src, uint8_t, cleanup_list*) noexcept {
        double val;
        if (PyFloat_Check(src.ptr()))
            val = PyFloat_AsDouble(src.ptr());
        else if (PyLong_Check(src.ptr()))
            val = PyLong_AsDouble(src.ptr());
        else
            return false;
        value = schedsim::core::duration_from_seconds(val);
        return true;
    }

    static handle from_cpp(schedsim::core::Duration d, rv_policy, cleanup_list*) noexcept {
        return PyFloat_FromDouble(schedsim::core::duration_to_seconds(d));
    }
};

// TimePoint <-> float (seconds)
template <> struct type_caster<schedsim::core::TimePoint> {
    NB_TYPE_CASTER(schedsim::core::TimePoint, const_name("float"))

    bool from_python(handle src, uint8_t, cleanup_list*) noexcept {
        double val;
        if (PyFloat_Check(src.ptr()))
            val = PyFloat_AsDouble(src.ptr());
        else if (PyLong_Check(src.ptr()))
            val = PyLong_AsDouble(src.ptr());
        else
            return false;
        value = schedsim::core::time_from_seconds(val);
        return true;
    }

    static handle from_cpp(schedsim::core::TimePoint tp, rv_policy, cleanup_list*) noexcept {
        return PyFloat_FromDouble(schedsim::core::time_to_seconds(tp));
    }
};

// Frequency <-> float (MHz)
template <> struct type_caster<schedsim::core::Frequency> {
    NB_TYPE_CASTER(schedsim::core::Frequency, const_name("float"))

    bool from_python(handle src, uint8_t, cleanup_list*) noexcept {
        double val;
        if (PyFloat_Check(src.ptr()))
            val = PyFloat_AsDouble(src.ptr());
        else if (PyLong_Check(src.ptr()))
            val = PyLong_AsDouble(src.ptr());
        else
            return false;
        value = schedsim::core::Frequency{val};
        return true;
    }

    static handle from_cpp(schedsim::core::Frequency f, rv_policy, cleanup_list*) noexcept {
        return PyFloat_FromDouble(f.mhz);
    }
};

// Power <-> float (mW)
template <> struct type_caster<schedsim::core::Power> {
    NB_TYPE_CASTER(schedsim::core::Power, const_name("float"))

    bool from_python(handle src, uint8_t, cleanup_list*) noexcept {
        double val;
        if (PyFloat_Check(src.ptr()))
            val = PyFloat_AsDouble(src.ptr());
        else if (PyLong_Check(src.ptr()))
            val = PyLong_AsDouble(src.ptr());
        else
            return false;
        value = schedsim::core::Power{val};
        return true;
    }

    static handle from_cpp(schedsim::core::Power p, rv_policy, cleanup_list*) noexcept {
        return PyFloat_FromDouble(p.mw);
    }
};

// Energy -> float (mJ) - output only
template <> struct type_caster<schedsim::core::Energy> {
    NB_TYPE_CASTER(schedsim::core::Energy, const_name("float"))

    bool from_python(handle src, uint8_t, cleanup_list*) noexcept {
        double val;
        if (PyFloat_Check(src.ptr()))
            val = PyFloat_AsDouble(src.ptr());
        else if (PyLong_Check(src.ptr()))
            val = PyLong_AsDouble(src.ptr());
        else
            return false;
        value = schedsim::core::Energy{val};
        return true;
    }

    static handle from_cpp(schedsim::core::Energy e, rv_policy, cleanup_list*) noexcept {
        return PyFloat_FromDouble(e.mj);
    }
};

} // namespace nanobind::detail

// ============================================================================
// Helper: convert variant map to Python dict
// ============================================================================

static nb::dict variant_map_to_dict(
    const std::unordered_map<std::string, std::variant<double, uint64_t, std::string>>& fields) {
    nb::dict d;
    for (const auto& [key, val] : fields) {
        std::visit([&](auto&& v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, double>)
                d[key.c_str()] = v;
            else if constexpr (std::is_same_v<T, uint64_t>)
                d[key.c_str()] = v;
            else if constexpr (std::is_same_v<T, std::string>)
                d[key.c_str()] = v;
        }, val);
    }
    return d;
}

// ============================================================================
// Module definition
// ============================================================================

NB_MODULE(pyschedsim, m) {
    using namespace schedsim::core;
    using namespace schedsim::algo;
    using namespace schedsim::io;

    // ========================================================================
    // 1. Exception registration
    // ========================================================================

    nb::exception<AlreadyFinalizedError>(m, "AlreadyFinalizedError", PyExc_RuntimeError);
    nb::exception<InvalidStateError>(m, "InvalidStateError", PyExc_RuntimeError);
    nb::exception<HandlerAlreadySetError>(m, "HandlerAlreadySetError", PyExc_RuntimeError);
    nb::exception<OutOfRangeError>(m, "OutOfRangeError", PyExc_ValueError);
    nb::exception<AdmissionError>(m, "AdmissionError", PyExc_ValueError);
    nb::exception<LoaderError>(m, "LoaderError", PyExc_IOError);

    // ========================================================================
    // 2. Core enums
    // ========================================================================

    nb::enum_<ProcessorState>(m, "ProcessorState")
        .value("Idle", ProcessorState::Idle)
        .value("ContextSwitching", ProcessorState::ContextSwitching)
        .value("Running", ProcessorState::Running)
        .value("Sleep", ProcessorState::Sleep)
        .value("Changing", ProcessorState::Changing);

    nb::enum_<CStateScope>(m, "CStateScope")
        .value("PerProcessor", CStateScope::PerProcessor)
        .value("DomainWide", CStateScope::DomainWide);

    // ========================================================================
    // 3. Algo enums
    // ========================================================================

    nb::enum_<AdmissionTest>(m, "AdmissionTest")
        .value("CapacityBound", AdmissionTest::CapacityBound)
        .value("GFB", AdmissionTest::GFB);

    nb::enum_<DeadlineMissPolicy>(m, "DeadlineMissPolicy")
        .value("Continue", DeadlineMissPolicy::Continue)
        .value("AbortJob", DeadlineMissPolicy::AbortJob)
        .value("AbortTask", DeadlineMissPolicy::AbortTask)
        .value("StopSimulation", DeadlineMissPolicy::StopSimulation);

    // ========================================================================
    // 4. Core classes
    // ========================================================================

    nb::class_<TraceWriter>(m, "TraceWriter");

    nb::class_<ProcessorType>(m, "ProcessorType")
        .def_prop_ro("id", &ProcessorType::id)
        .def_prop_ro("performance", &ProcessorType::performance);

    nb::class_<ClockDomain>(m, "ClockDomain")
        .def_prop_ro("id", &ClockDomain::id)
        .def_prop_ro("frequency", &ClockDomain::frequency)
        .def_prop_ro("freq_min", &ClockDomain::freq_min)
        .def_prop_ro("freq_max", &ClockDomain::freq_max)
        .def_prop_ro("is_locked", &ClockDomain::is_locked)
        .def_prop_ro("is_transitioning", &ClockDomain::is_transitioning)
        .def_prop_ro("freq_eff", &ClockDomain::freq_eff)
        .def("set_freq_eff", &ClockDomain::set_freq_eff, "freq"_a)
        .def("set_power_coefficients", [](ClockDomain& self, nb::list coeffs_list) {
            if (nb::len(coeffs_list) != 4) {
                throw std::invalid_argument("Expected a list of 4 floats [a0, a1, a2, a3]");
            }
            std::array<double, 4> coeffs;
            for (int i = 0; i < 4; ++i) {
                coeffs[i] = nb::cast<double>(coeffs_list[i]);
            }
            self.set_power_coefficients(coeffs);
        }, "coeffs"_a);

    nb::class_<PowerDomain>(m, "PowerDomain")
        .def_prop_ro("id", &PowerDomain::id);

    nb::class_<Processor>(m, "Processor")
        .def_prop_ro("id", &Processor::id)
        .def_prop_ro("state", &Processor::state)
        .def_prop_ro("current_cstate_level", &Processor::current_cstate_level)
        .def("type", &Processor::type, nb::rv_policy::reference)
        .def("clock_domain", &Processor::clock_domain, nb::rv_policy::reference)
        .def("power_domain", &Processor::power_domain, nb::rv_policy::reference);

    nb::class_<Task>(m, "Task")
        .def_prop_ro("id", &Task::id)
        .def_prop_ro("period", [](const Task& t) { return t.period(); })
        .def_prop_ro("relative_deadline", [](const Task& t) { return t.relative_deadline(); })
        .def_prop_ro("wcet", [](const Task& t) { return t.wcet(); });

    nb::class_<Job>(m, "Job")
        .def_prop_ro("remaining_work", &Job::remaining_work)
        .def_prop_ro("total_work", &Job::total_work)
        .def_prop_ro("absolute_deadline", &Job::absolute_deadline)
        .def_prop_ro("is_complete", &Job::is_complete)
        .def("task", &Job::task, nb::rv_policy::reference);

    // Platform
    nb::class_<Platform>(m, "Platform")
        .def("add_processor_type",
             [](Platform& self, std::string_view name, double perf, Duration cs_delay) -> ProcessorType& {
                 return self.add_processor_type(name, perf, cs_delay);
             },
             "name"_a, "performance"_a, "context_switch_delay"_a = duration_from_seconds(0.0),
             nb::rv_policy::reference)
        .def("add_clock_domain",
             [](Platform& self, Frequency fmin, Frequency fmax, Duration td) -> ClockDomain& {
                 return self.add_clock_domain(fmin, fmax, td);
             },
             "freq_min"_a, "freq_max"_a, "transition_delay"_a = duration_from_seconds(0.0),
             nb::rv_policy::reference)
        .def("add_power_domain",
             [](Platform& self, nb::list cstates_list) -> PowerDomain& {
                 if (!nb::isinstance<nb::list>(cstates_list)) {
                     throw std::invalid_argument("Expected a list of (level, scope, wake_latency, power) tuples");
                 }
                 std::vector<CStateLevel> c_states;
                 c_states.reserve(nb::len(cstates_list));
                 for (size_t i = 0; i < nb::len(cstates_list); ++i) {
                     nb::tuple item = nb::cast<nb::tuple>(cstates_list[i]);
                     if (nb::len(item) != 4) {
                         throw std::invalid_argument("Expected (level, scope, wake_latency, power) tuple");
                     }
                     int level = nb::cast<int>(item[0]);
                     int scope_int = nb::cast<int>(item[1]);
                     double wake_latency = nb::cast<double>(item[2]);
                     double power = nb::cast<double>(item[3]);

                     CStateScope scope = (scope_int == 0)
                         ? CStateScope::PerProcessor
                         : CStateScope::DomainWide;

                     c_states.push_back(CStateLevel{
                         level, scope,
                         duration_from_seconds(wake_latency),
                         Power{power}
                     });
                 }
                 return self.add_power_domain(std::move(c_states));
             },
             "cstates"_a,
             nb::rv_policy::reference)
        .def("add_processor", &Platform::add_processor,
             "type"_a, "clock_domain"_a, "power_domain"_a,
             nb::rv_policy::reference)
        .def("add_task",
             [](Platform& self, Duration period, Duration rd, Duration wcet) -> Task& {
                 return self.add_task(period, rd, wcet);
             },
             "period"_a, "relative_deadline"_a, "wcet"_a,
             nb::rv_policy::reference)
        .def("add_task",
             [](Platform& self, std::size_t id, Duration period, Duration rd, Duration wcet) -> Task& {
                 return self.add_task(id, period, rd, wcet);
             },
             "id"_a, "period"_a, "relative_deadline"_a, "wcet"_a,
             nb::rv_policy::reference)
        .def_prop_ro("processor_type_count", &Platform::processor_type_count)
        .def_prop_ro("processor_count", &Platform::processor_count)
        .def_prop_ro("clock_domain_count", &Platform::clock_domain_count)
        .def_prop_ro("power_domain_count", &Platform::power_domain_count)
        .def_prop_ro("task_count", &Platform::task_count)
        .def("processor_type",
             [](Platform& self, std::size_t idx) -> ProcessorType& { return self.processor_type(idx); },
             "idx"_a, nb::rv_policy::reference)
        .def("processor",
             [](Platform& self, std::size_t idx) -> Processor& { return self.processor(idx); },
             "idx"_a, nb::rv_policy::reference)
        .def("clock_domain",
             [](Platform& self, std::size_t idx) -> ClockDomain& { return self.clock_domain(idx); },
             "idx"_a, nb::rv_policy::reference)
        .def("power_domain",
             [](Platform& self, std::size_t idx) -> PowerDomain& { return self.power_domain(idx); },
             "idx"_a, nb::rv_policy::reference)
        .def("task",
             [](Platform& self, std::size_t idx) -> Task& { return self.task(idx); },
             "idx"_a, nb::rv_policy::reference)
        .def_prop_ro("reference_performance", &Platform::reference_performance)
        .def("finalize", &Platform::finalize)
        .def_prop_ro("is_finalized", &Platform::is_finalized);

    // Engine
    nb::class_<Engine>(m, "Engine")
        .def(nb::init<>())
        .def_prop_ro("time", &Engine::time)
        .def("run", [](Engine& self) { self.run(); })
        .def("run", [](Engine& self, TimePoint until) { self.run(until); }, "until"_a)
        .def("finalize", &Engine::finalize)
        .def_prop_ro("is_finalized", &Engine::is_finalized)
        .def("schedule_job_arrival", &Engine::schedule_job_arrival,
             "task"_a, "arrival_time"_a, "exec_time"_a)
        .def("enable_context_switch", &Engine::enable_context_switch, "enabled"_a)
        .def_prop_ro("context_switch_enabled", &Engine::context_switch_enabled)
        .def("enable_energy_tracking", &Engine::enable_energy_tracking, "enabled"_a)
        .def_prop_ro("energy_tracking_enabled", &Engine::energy_tracking_enabled)
        .def("processor_energy", &Engine::processor_energy, "proc_id"_a)
        .def("clock_domain_energy", &Engine::clock_domain_energy, "cd_id"_a)
        .def("power_domain_energy", &Engine::power_domain_energy, "pd_id"_a)
        .def("total_energy", &Engine::total_energy)
        .def("set_trace_writer", &Engine::set_trace_writer, "writer"_a.none(),
             nb::rv_policy::none)
        .def_prop_ro("platform", [](Engine& self) -> Platform& { return self.platform(); },
                     nb::rv_policy::reference);

    // ========================================================================
    // 5. Algo classes
    // ========================================================================

    nb::class_<CbsServer>(m, "CbsServer");

    nb::class_<Scheduler>(m, "Scheduler");

    nb::class_<EdfScheduler, Scheduler>(m, "EdfScheduler")
        .def(nb::init<Engine&, std::vector<Processor*>>(),
             "engine"_a, "processors"_a,
             nb::keep_alive<1, 2>())
        .def("can_admit", &EdfScheduler::can_admit, "budget"_a, "period"_a)
        .def_prop_ro("utilization", &EdfScheduler::utilization)
        .def_prop_ro("processor_count", &EdfScheduler::processor_count)
        .def_prop_ro("server_count", &EdfScheduler::server_count)
        .def("add_server",
             [](EdfScheduler& self, Task& task, Duration budget, Duration period) -> CbsServer& {
                 return self.add_server(task, budget, period);
             },
             "task"_a, "budget"_a, "period"_a,
             nb::rv_policy::reference)
        .def("add_server",
             [](EdfScheduler& self, Task& task) -> CbsServer& {
                 return self.add_server(task);
             },
             "task"_a,
             nb::rv_policy::reference)
        .def("set_admission_test", &EdfScheduler::set_admission_test, "test"_a)
        .def("set_deadline_miss_policy", &EdfScheduler::set_deadline_miss_policy, "policy"_a)
        .def("set_expected_arrivals", &EdfScheduler::set_expected_arrivals, "task"_a, "count"_a)
        .def("enable_grub", &EdfScheduler::enable_grub)
        .def("enable_cash", &EdfScheduler::enable_cash)
        .def("enable_power_aware_dvfs", &EdfScheduler::enable_power_aware_dvfs, "cooldown"_a)
        .def("enable_basic_dpm", &EdfScheduler::enable_basic_dpm, "target_cstate"_a = 1)
        .def("enable_ffa", &EdfScheduler::enable_ffa, "cooldown"_a, "sleep_cstate"_a = 1)
        .def("enable_csf", &EdfScheduler::enable_csf, "cooldown"_a, "sleep_cstate"_a = 1)
        .def("enable_ffa_timer", &EdfScheduler::enable_ffa_timer, "cooldown"_a, "sleep_cstate"_a = 1)
        .def("enable_csf_timer", &EdfScheduler::enable_csf_timer, "cooldown"_a, "sleep_cstate"_a = 1)
        .def_prop_ro("active_utilization", &EdfScheduler::active_utilization)
        .def_prop_ro("scheduler_utilization", &EdfScheduler::scheduler_utilization)
        .def_prop_ro("max_scheduler_utilization", &EdfScheduler::max_scheduler_utilization)
        .def_prop_ro("max_server_utilization", &EdfScheduler::max_server_utilization);

    nb::class_<SingleSchedulerAllocator>(m, "SingleSchedulerAllocator")
        .def(nb::init<Engine&, Scheduler&, ClockDomain*>(),
             "engine"_a, "scheduler"_a, "clock_domain"_a = nullptr,
             nb::keep_alive<1, 2>(), nb::keep_alive<1, 3>());

    nb::class_<Cluster>(m, "Cluster")
        .def(nb::init<ClockDomain&, Scheduler&, double, double>(),
             "clock_domain"_a, "scheduler"_a, "perf_score"_a, "reference_freq_max"_a,
             nb::keep_alive<1, 2>(), nb::keep_alive<1, 3>())
        .def("clock_domain",
             [](Cluster& self) -> ClockDomain& { return self.clock_domain(); },
             nb::rv_policy::reference)
        .def("scheduler",
             [](Cluster& self) -> Scheduler& { return self.scheduler(); },
             nb::rv_policy::reference)
        .def_prop_ro("perf", &Cluster::perf)
        .def_prop_ro("scale_speed", &Cluster::scale_speed)
        .def_prop_ro("u_target", &Cluster::u_target)
        .def("set_u_target", &Cluster::set_u_target, "target"_a)
        .def("scaled_utilization", &Cluster::scaled_utilization, "task_util"_a)
        .def_prop_ro("processor_count", &Cluster::processor_count)
        .def_prop_ro("utilization", &Cluster::utilization)
        .def("can_admit", &Cluster::can_admit, "budget"_a, "period"_a);

    // ========================================================================
    // 6. IO structs
    // ========================================================================

    nb::class_<JobParams>(m, "JobParams")
        .def(nb::init<>())
        .def_rw("arrival", &JobParams::arrival)
        .def_rw("duration", &JobParams::duration);

    nb::class_<TaskParams>(m, "TaskParams")
        .def(nb::init<>())
        .def_rw("id", &TaskParams::id)
        .def_rw("period", &TaskParams::period)
        .def_rw("relative_deadline", &TaskParams::relative_deadline)
        .def_rw("wcet", &TaskParams::wcet)
        .def_rw("jobs", &TaskParams::jobs);

    nb::class_<ScenarioData>(m, "ScenarioData")
        .def(nb::init<>())
        .def_rw("tasks", &ScenarioData::tasks);

    // ========================================================================
    // 7. IO functions
    // ========================================================================

    m.def("load_platform", &load_platform, "engine"_a, "path"_a);
    m.def("load_platform_from_string", &load_platform_from_string, "engine"_a, "json"_a);
    m.def("load_scenario", &load_scenario, "path"_a);
    m.def("load_scenario_from_string", &load_scenario_from_string, "json"_a);
    m.def("inject_scenario", &inject_scenario, "engine"_a, "scenario"_a,
          nb::rv_policy::reference);
    m.def("schedule_arrivals", &schedule_arrivals, "engine"_a, "task"_a, "jobs"_a);
    m.def("write_scenario", &write_scenario, "scenario"_a, "path"_a);

    // ========================================================================
    // 8. Trace writers
    // ========================================================================

    nb::class_<NullTraceWriter, TraceWriter>(m, "NullTraceWriter")
        .def(nb::init<>());

    nb::class_<TraceRecord>(m, "TraceRecord")
        .def_ro("time", &TraceRecord::time)
        .def_ro("type", &TraceRecord::type)
        .def_prop_ro("fields", [](const TraceRecord& self) {
            return variant_map_to_dict(self.fields);
        })
        .def("get_field", [](const TraceRecord& self, const std::string& key) -> nb::object {
            auto it = self.fields.find(key);
            if (it == self.fields.end())
                return nb::none();
            return std::visit([](auto&& val) -> nb::object {
                return nb::cast(val);
            }, it->second);
        }, "key"_a)
        .def("get_fields_dict", [](const TraceRecord& self) {
            return variant_map_to_dict(self.fields);
        })
        .def("get_field_names", [](const TraceRecord& self) {
            nb::list names;
            for (const auto& [key, _] : self.fields)
                names.append(key);
            return names;
        });

    nb::class_<MemoryTraceWriter, TraceWriter>(m, "MemoryTraceWriter")
        .def(nb::init<>())
        .def("clear", &MemoryTraceWriter::clear)
        .def_prop_ro("record_count", [](const MemoryTraceWriter& self) {
            return self.records().size();
        })
        .def("record", [](const MemoryTraceWriter& self, std::size_t index) -> const TraceRecord& {
            if (index >= self.records().size())
                throw std::out_of_range("TraceRecord index out of range");
            return self.records()[index];
        }, "index"_a, nb::rv_policy::reference)
        .def("compute_metrics", [](const MemoryTraceWriter& self) {
            return compute_metrics(self.records());
        })
        .def("track_frequency_changes", [](const MemoryTraceWriter& self) {
            return track_frequency_changes(self.records());
        })
        .def("track_core_changes", [](const MemoryTraceWriter& self) {
            return track_core_changes(self.records());
        })
        .def("track_config_changes", [](const MemoryTraceWriter& self) {
            return track_config_changes(self.records());
        });

    nb::class_<FileJsonTraceWriter, TraceWriter>(m, "FileJsonTraceWriter")
        .def(nb::init<const std::string&>(), "filename"_a)
        .def("finalize", &FileJsonTraceWriter::finalize);

    // ========================================================================
    // 9. Metrics
    // ========================================================================

    nb::class_<SimulationMetrics>(m, "SimulationMetrics")
        .def(nb::init<>())
        .def_rw("total_jobs", &SimulationMetrics::total_jobs)
        .def_rw("completed_jobs", &SimulationMetrics::completed_jobs)
        .def_rw("deadline_misses", &SimulationMetrics::deadline_misses)
        .def_rw("preemptions", &SimulationMetrics::preemptions)
        .def_rw("context_switches", &SimulationMetrics::context_switches)
        .def_rw("total_energy_mj", &SimulationMetrics::total_energy_mj)
        .def_rw("average_utilization", &SimulationMetrics::average_utilization)
        .def_rw("rejected_tasks", &SimulationMetrics::rejected_tasks)
        .def_rw("cluster_migrations", &SimulationMetrics::cluster_migrations)
        .def_rw("transitions", &SimulationMetrics::transitions)
        .def_rw("core_state_requests", &SimulationMetrics::core_state_requests)
        .def_rw("frequency_requests", &SimulationMetrics::frequency_requests)
        .def("get_energy_per_processor", [](const SimulationMetrics& self) {
            nb::dict d;
            for (const auto& [k, v] : self.energy_per_processor)
                d[nb::cast(k)] = v;
            return d;
        })
        .def("get_utilization_per_processor", [](const SimulationMetrics& self) {
            nb::dict d;
            for (const auto& [k, v] : self.utilization_per_processor)
                d[nb::cast(k)] = v;
            return d;
        })
        .def("get_response_times_per_task", [](const SimulationMetrics& self) {
            nb::dict d;
            for (const auto& [k, v] : self.response_times_per_task) {
                nb::list lst;
                for (double t : v) lst.append(t);
                d[nb::cast(k)] = lst;
            }
            return d;
        })
        .def("get_deadline_misses_per_task", [](const SimulationMetrics& self) {
            nb::dict d;
            for (const auto& [k, v] : self.deadline_misses_per_task)
                d[nb::cast(k)] = v;
            return d;
        })
        .def("get_waiting_times_per_task", [](const SimulationMetrics& self) {
            nb::dict d;
            for (const auto& [k, v] : self.waiting_times_per_task) {
                nb::list lst;
                for (double t : v) lst.append(t);
                d[nb::cast(k)] = lst;
            }
            return d;
        });

    m.def("compute_metrics_from_file", &compute_metrics_from_file, "path"_a);

    nb::class_<ResponseTimeStats>(m, "ResponseTimeStats")
        .def(nb::init<>())
        .def_rw("min", &ResponseTimeStats::min)
        .def_rw("max", &ResponseTimeStats::max)
        .def_rw("mean", &ResponseTimeStats::mean)
        .def_rw("median", &ResponseTimeStats::median)
        .def_rw("stddev", &ResponseTimeStats::stddev)
        .def_rw("percentile_95", &ResponseTimeStats::percentile_95)
        .def_rw("percentile_99", &ResponseTimeStats::percentile_99);

    m.def("compute_response_time_stats", &compute_response_time_stats, "response_times"_a);

    // ========================================================================
    // 10. Time-series structs
    // ========================================================================

    nb::class_<FrequencyInterval>(m, "FrequencyInterval")
        .def(nb::init<>())
        .def_rw("start", &FrequencyInterval::start)
        .def_rw("stop", &FrequencyInterval::stop)
        .def_rw("frequency", &FrequencyInterval::frequency)
        .def_rw("cluster_id", &FrequencyInterval::cluster_id);

    nb::class_<CoreCountInterval>(m, "CoreCountInterval")
        .def(nb::init<>())
        .def_rw("start", &CoreCountInterval::start)
        .def_rw("stop", &CoreCountInterval::stop)
        .def_rw("active_cores", &CoreCountInterval::active_cores)
        .def_rw("cluster_id", &CoreCountInterval::cluster_id);

    nb::class_<ConfigInterval>(m, "ConfigInterval")
        .def(nb::init<>())
        .def_rw("start", &ConfigInterval::start)
        .def_rw("stop", &ConfigInterval::stop)
        .def_rw("frequency", &ConfigInterval::frequency)
        .def_rw("active_cores", &ConfigInterval::active_cores);

    // ========================================================================
    // 11. Helper functions
    // ========================================================================

    m.def("get_all_processors", [](Engine& engine) {
        auto& platform = engine.platform();
        std::vector<Processor*> procs;
        procs.reserve(platform.processor_count());
        for (std::size_t i = 0; i < platform.processor_count(); ++i)
            procs.push_back(&platform.processor(i));
        return procs;
    }, "engine"_a, nb::rv_policy::reference);

    m.def("get_all_tasks", [](Engine& engine) {
        auto& platform = engine.platform();
        std::vector<Task*> tasks;
        tasks.reserve(platform.task_count());
        for (std::size_t i = 0; i < platform.task_count(); ++i)
            tasks.push_back(&platform.task(i));
        return tasks;
    }, "engine"_a, nb::rv_policy::reference);
}
