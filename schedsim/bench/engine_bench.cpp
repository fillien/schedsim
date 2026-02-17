#include <schedsim/algo/edf_scheduler.hpp>
#include <schedsim/algo/grub_policy.hpp>

#include <schedsim/core/clock_domain.hpp>
#include <schedsim/core/engine.hpp>
#include <schedsim/core/platform.hpp>
#include <schedsim/core/processor.hpp>
#include <schedsim/core/task.hpp>
#include <schedsim/core/types.hpp>

#include <benchmark/benchmark.h>

using namespace schedsim::core;
using namespace schedsim::algo;

// ---------------------------------------------------------------------------
// BM_EventQueue: insert + pop N timers
// ---------------------------------------------------------------------------

static void BM_EventQueue(benchmark::State& state) {
    const int64_t n = state.range(0);

    for (auto _ : state) {
        state.PauseTiming();
        Engine engine;
        engine.platform().add_processor_type("cpu", 1.0);
        auto& cd = engine.platform().add_clock_domain(Frequency{1000.0}, Frequency{1000.0});
        auto& pd = engine.platform().add_power_domain({
            {0, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{100.0}}
        });
        engine.platform().add_processor(
            engine.platform().processor_type(0), cd, pd);
        engine.platform().finalize();

        int fired = 0;
        for (int64_t i = 0; i < n; ++i) {
            double t = static_cast<double>(i + 1) * 0.001;
            engine.add_timer(time_from_seconds(t), [&fired]() { ++fired; });
        }
        state.ResumeTiming();

        engine.run();
        benchmark::DoNotOptimize(fired);
    }
}
BENCHMARK(BM_EventQueue)->Arg(1000)->Arg(10000);

// ---------------------------------------------------------------------------
// BM_SingleSim_Grub: 5 tasks, GRUB, no trace, run to completion
// ---------------------------------------------------------------------------

static void BM_SingleSim_Grub(benchmark::State& state) {
    for (auto _ : state) {
        Engine engine;
        auto& pt = engine.platform().add_processor_type("cpu", 1.0);
        auto& cd = engine.platform().add_clock_domain(Frequency{200.0}, Frequency{2000.0});
        cd.set_frequency_modes({
            Frequency{200.0}, Frequency{500.0}, Frequency{800.0},
            Frequency{1000.0}, Frequency{1500.0}, Frequency{2000.0}
        });
        auto& pd = engine.platform().add_power_domain({
            {0, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{100.0}}
        });

        for (int i = 0; i < 4; ++i) {
            engine.platform().add_processor(pt, cd, pd);
        }

        Task* tasks[5];
        for (int i = 0; i < 5; ++i) {
            tasks[i] = &engine.platform().add_task(
                duration_from_seconds(0.5), duration_from_seconds(0.5),
                duration_from_seconds(0.1));
        }
        engine.platform().finalize();

        std::vector<Processor*> procs;
        for (std::size_t i = 0; i < engine.platform().processor_count(); ++i) {
            procs.push_back(&engine.platform().processor(i));
        }

        EdfScheduler sched(engine, procs);
        sched.enable_grub();

        for (int i = 0; i < 5; ++i) {
            sched.add_server(*tasks[i], duration_from_seconds(0.1),
                             duration_from_seconds(0.5));
        }

        engine.set_job_arrival_handler([&](Task& t, Job job) {
            sched.on_job_arrival(t, std::move(job));
        });

        // Schedule 3 jobs per task
        for (int i = 0; i < 5; ++i) {
            for (int j = 0; j < 3; ++j) {
                engine.schedule_job_arrival(
                    *tasks[i],
                    time_from_seconds(j * 0.5),
                    duration_from_seconds(0.05));
            }
            sched.set_expected_arrivals(*tasks[i], 3);
        }

        engine.run();
        benchmark::DoNotOptimize(engine.time());
    }
}
BENCHMARK(BM_SingleSim_Grub);

// ---------------------------------------------------------------------------
// BM_SingleSim_GrubPA: same + PA DVFS
// ---------------------------------------------------------------------------

static void BM_SingleSim_GrubPA(benchmark::State& state) {
    for (auto _ : state) {
        Engine engine;
        auto& pt = engine.platform().add_processor_type("cpu", 1.0);
        auto& cd = engine.platform().add_clock_domain(Frequency{200.0}, Frequency{2000.0});
        cd.set_frequency_modes({
            Frequency{200.0}, Frequency{500.0}, Frequency{800.0},
            Frequency{1000.0}, Frequency{1500.0}, Frequency{2000.0}
        });
        cd.set_freq_eff(Frequency{1000.0});
        cd.set_power_coefficients({50.0, 100.0, 0.0, 0.0});
        auto& pd = engine.platform().add_power_domain({
            {0, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{100.0}}
        });

        for (int i = 0; i < 4; ++i) {
            engine.platform().add_processor(pt, cd, pd);
        }

        Task* tasks[5];
        for (int i = 0; i < 5; ++i) {
            tasks[i] = &engine.platform().add_task(
                duration_from_seconds(0.5), duration_from_seconds(0.5),
                duration_from_seconds(0.1));
        }
        engine.platform().finalize();

        std::vector<Processor*> procs;
        for (std::size_t i = 0; i < engine.platform().processor_count(); ++i) {
            procs.push_back(&engine.platform().processor(i));
        }

        EdfScheduler sched(engine, procs);
        sched.enable_grub();
        sched.enable_power_aware_dvfs();

        for (int i = 0; i < 5; ++i) {
            sched.add_server(*tasks[i], duration_from_seconds(0.1),
                             duration_from_seconds(0.5));
        }

        engine.set_job_arrival_handler([&](Task& t, Job job) {
            sched.on_job_arrival(t, std::move(job));
        });

        for (int i = 0; i < 5; ++i) {
            for (int j = 0; j < 3; ++j) {
                engine.schedule_job_arrival(
                    *tasks[i],
                    time_from_seconds(j * 0.5),
                    duration_from_seconds(0.05));
            }
            sched.set_expected_arrivals(*tasks[i], 3);
        }

        engine.run();
        benchmark::DoNotOptimize(engine.time());
    }
}
BENCHMARK(BM_SingleSim_GrubPA);

// ---------------------------------------------------------------------------
// BM_BatchSim: N independent small simulations
// ---------------------------------------------------------------------------

static void BM_BatchSim(benchmark::State& state) {
    const int64_t batch_size = state.range(0);

    for (auto _ : state) {
        int completed = 0;
        for (int64_t b = 0; b < batch_size; ++b) {
            Engine engine;
            auto& pt = engine.platform().add_processor_type("cpu", 1.0);
            auto& cd = engine.platform().add_clock_domain(
                Frequency{1000.0}, Frequency{1000.0});
            auto& pd = engine.platform().add_power_domain({
                {0, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{100.0}}
            });
            engine.platform().add_processor(pt, cd, pd);

            auto& task = engine.platform().add_task(
                duration_from_seconds(1.0), duration_from_seconds(1.0),
                duration_from_seconds(0.5));
            engine.platform().finalize();

            std::vector<Processor*> procs = {&engine.platform().processor(0)};
            EdfScheduler sched(engine, procs);
            sched.add_server(task);

            engine.set_job_arrival_handler([&](Task& t, Job job) {
                sched.on_job_arrival(t, std::move(job));
            });
            engine.schedule_job_arrival(task, time_from_seconds(0.0),
                                        duration_from_seconds(0.3));
            sched.set_expected_arrivals(task, 1);

            engine.run();
            ++completed;
        }
        benchmark::DoNotOptimize(completed);
    }
}
BENCHMARK(BM_BatchSim)->Arg(100)->Arg(1000);
