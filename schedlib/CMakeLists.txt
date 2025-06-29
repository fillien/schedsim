add_library(schedlib)

target_compile_features(schedlib PUBLIC cxx_std_23)

if (ENABLE_TRACY)
    target_compile_definitions(schedlib PRIVATE TRACY_ENABLE)
endif()

if (ENABLE_DEBUG)
    target_link_libraries(schedlib PUBLIC json rang TracyClient)
endif()

target_link_libraries(schedlib PUBLIC json rang)

target_sources(schedlib PRIVATE
    src/analyzers/deadline_misses.cpp
    src/analyzers/energy.cpp
    src/analyzers/energy_model.cpp
    src/analyzers/frequency.cpp
    src/analyzers/gantt/gantt.cpp
    src/analyzers/gantt/rtsched.cpp
    src/analyzers/gantt/svg.cpp
    src/analyzers/stats.cpp
    src/analyzers/textual.cpp
    src/generators/uunifast_discard_weibull.cpp
    src/protocols/hardware.cpp
    src/protocols/scenario.cpp
    src/protocols/traces.cpp
    src/simulator/allocator.cpp
    src/simulator/allocators/ff_big_first.cpp
    src/simulator/allocators/ff_little_first.cpp
    src/simulator/allocators/ff_cap.cpp
    src/simulator/allocators/ff_lb.cpp
    src/simulator/engine.cpp
    src/simulator/platform.cpp
    src/simulator/processor.cpp
    src/simulator/scheduler.cpp
    src/simulator/schedulers/csf.cpp
    src/simulator/schedulers/csf_timer.cpp
    src/simulator/schedulers/dpm_dvfs.cpp
    src/simulator/schedulers/ffa.cpp
    src/simulator/schedulers/ffa_timer.cpp
    src/simulator/schedulers/parallel.cpp
    src/simulator/schedulers/power_aware.cpp
    src/simulator/schedulers/power_aware_timer.cpp
    src/simulator/server.cpp
    src/simulator/task.cpp
    src/simulator/timer.cpp
)

target_include_directories(schedlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
