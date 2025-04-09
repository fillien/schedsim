#ifndef UUNIFAST_DISCARD_WEIBULL_HPP
#define UUNIFAST_DISCARD_WEIBULL_HPP

#include <cstddef>
#include <optional>
#include <protocols/scenario.hpp>

namespace generators {
/**
 * @brief Generate a task set for a scheduling scenario using the UUniFast-Discard algorithm.
 *
 * This function generates a set of tasks with utilizations distributed according to the
 * UUniFast-Discard algorithm, ensuring the sum of utilizations is approximately equal to the
 * specified total utilization (within a rounding tolerance of 0.01). Each task is assigned a period
 * from a predefined set of values {25200, 12600, 8400, 6300, 5040, 4200, 3600, 3150, 2800, 2520}
 * and a Worst-Case Execution Time (WCET) calculated as the product of its utilization and period.
 * Tasks are created using the `generate_task` function, which may incorporate a Weibull
 * distribution for certain parameters, as suggested by the function name.
 *
 * @param nb_tasks The number of tasks to generate. Must be at least 1 and not exceed the maximum
 * value of `std::size_t`.
 * @param total_utilization The total utilization of all tasks. Must be non-negative and not exceed
 * the maximum value of `double`.
 * @param umax The maximum utilization for any single task. Must be between 0 and 1.
 * @param success_rate The success rate for tasks, used in task generation. Must be between 0 and 1.
 * @param compression_rate The compression rate for tasks, used in task generation. Must be between
 * 0 and 1.
 * @param a_special_need An optional parameter specifying special requirements for the tasks,
 * provided as a pair of doubles. Defaults to `std::nullopt`.
 * @return A `protocols::scenario::Setting` object containing the generated tasks.
 * @throws std::invalid_argument If any of the following conditions are met:
 *         - `nb_tasks` is less than 1 or exceeds `std::numeric_limits<std::size_t>::max()`.
 *         - `total_utilization` is negative or exceeds `std::numeric_limits<double>::max()`.
 *         - `umax`, `success_rate`, or `compression_rate` is outside the range [0, 1].
 *         - The total utilization cannot be achieved with the given `nb_tasks` and `umax` (i.e.,
 *         - `nb_tasks * umax < total_utilization`).
 */
auto uunifast_discard_weibull(
    std::size_t nb_tasks,
    double total_utilization,
    double umax,
    double success_rate,
    double compression_rate,
    const std::optional<std::pair<double, double>>& a_special_need = std::nullopt)
    -> protocols::scenario::Setting;

/**
 * @brief Generate multiple task sets in parallel and write them to files.
 *
 * This function generates a specified number of task sets using the `uunifast_discard_weibull`
 * function. Each task set is written to a separate file in the specified output directory, with
 * file names corresponding to the task set index (starting from 1, e.g., "1", "2", etc.). The
 * generation process is performed in parallel using multiple threads, with the workload distributed
 * across the specified number of CPU cores. The number of task sets per thread is calculated as
 * `nb_taskset / nb_cores`, with any remainder distributed among the first few threads.
 *
 * @param output_path The directory where the task set files will be written. Must exist and be a
 * valid directory.
 * @param nb_taskset The number of task sets to generate.
 * @param nb_tasks The number of tasks per task set.
 * @param total_utilization The total utilization for each task set.
 * @param umax The maximum utilization for any task in the task sets.
 * @param success_rate The success rate for tasks, passed to `uunifast_discard_weibull`.
 * @param compression_rate The compression rate for tasks, passed to `uunifast_discard_weibull`.
 * @param a_special_need Optional special requirements for the tasks, passed to
 * `uunifast_discard_weibull`. Defaults to `std::nullopt`.
 * @param nb_cores The number of CPU cores to use for parallel generation.
 * @throws std::invalid_argument If `output_path` does not exist or is not a directory.
 * @note This function does not explicitly handle exceptions from `uunifast_discard_weibull` or file
 * writing within threads. If such exceptions occur, they may terminate the program.
 */
auto generate_tasksets(
    std::string output_path,
    std::size_t nb_taskset,
    std::size_t nb_tasks,
    double total_utilization,
    double umax,
    double success_rate,
    double compression_rate,
    std::optional<std::pair<double, double>> a_special_need = std::nullopt,
    std::size_t nb_cores = 1) -> void;

auto add_taskets(
    const protocols::scenario::Setting& first, const protocols::scenario::Setting& second)
    -> protocols::scenario::Setting;
} // namespace generators

#endif
