#ifndef GANTT_HPP
#define GANTT_HPP

#include <array>
#include <cstddef>
#include <map>
#include <protocols/hardware.hpp>
#include <protocols/traces.hpp>
#include <variant>
#include <vector>

namespace outputs::gantt {

/**
 * @brief A constant array of color names and their corresponding hexadecimal values.
 */
constexpr std::array<std::pair<const char*, const char*>, 19> colors = {
    {{"red", "#FF0000"},
     {"green", "#008000"},
     {"blue", "#0000FF"},
     {"cyan", "#00FFFF"},
     {"magenta", "#FF00FF"},
     {"yellow", "#FFFF00"},
     {"black", "#000000"},
     {"gray", "#808080"},
     {"white", "#FFFFFF"},
     {"darkgray", "#A9A9A9"},
     {"lightgray", "#D3D3D3"},
     {"brown", "#A52A2A"},
     {"lime", "#00FF00"},
     {"olive", "#808000"},
     {"orange", "#FFA500"},
     {"pink", "#FFC0CB"},
     {"purple", "#800080"},
     {"teal", "#008080"},
     {"violet", "#EE82EE"}}};

/**
 * @brief Retrieves the color name from the `colors` array based on the given index.
 *
 * @param index The index of the desired color in the `colors` array.
 * @return A string representing the color name.
 */
inline auto get_color_name(std::size_t index) -> std::string { return colors.at(index).first; }

/**
 * @brief Retrieves the hexadecimal representation of a color from the `colors` array based on the
 * given index.
 *
 * @param index The index of the desired color in the `colors` array.
 * @return A string representing the hexadecimal color value.
 */
inline auto get_color_hex(std::size_t index) -> std::string { return colors.at(index).second; }

/**
 * @brief Represents an arrival event.
 */
struct Arrival {
        /**
         * @brief The index of the task that arrived.
         */
        std::size_t index;
        /**
         * @brief The timestamp when the task arrived.
         */
        double timestamp;
};

/**
 * @brief Represents a deadline event.
 */
struct Deadline {
        /**
         * @brief The index of the task with the deadline.
         */
        std::size_t index;
        /**
         * @brief The timestamp of the deadline.
         */
        double timestamp;
};

/**
 * @brief Represents a finished event.
 */
struct Finished {
        /**
         * @brief The index of the task that finished.
         */
        std::size_t index;
        /**
         * @brief The timestamp when the task finished.
         */
        double timestamp;
};

/**
 * @brief Represents an execution event.
 */
struct Execution {
        /**
         * @brief The index of the executed task.
         */
        std::size_t index;
        /**
         * @brief The CPU on which the task was executed.
         */
        std::size_t cpu;
        /**
         * @brief The start time of the execution.
         */
        double start;
        /**
         * @brief The stop time of the execution.
         */
        double stop;
        /**
         * @brief The frequency at which the task was executed.
         */
        double frequency;
        /**
         * @brief The maximum allowed frequency for the task.
         */
        double frequency_max;
        /**
         * @brief The minimum allowed frequency for the task.
         */
        double frequency_min;
};

/**
 * @brief Represents a period of non-contiguous activity.
 */
struct ActiveNonCont {
        /**
         * @brief The index of the active task.
         */
        std::size_t index;
        /**
         * @brief The start time of the active period.
         */
        double start;
        /**
         * @brief The stop time of the active period.
         */
        double stop;
};

/**
 * @brief Represents a processor mode where the processor is idle.
 */
struct ProcModeIdle {
        /**
         * @brief The index associated with this idle state (e.g., CPU ID).
         */
        std::size_t index;
        /**
         * @brief The start time of the idle period.
         */
        double start;
        /**
         * @brief The stop time of the idle period.
         */
        double stop;
};

/**
 * @brief Represents a processor mode where the processor is running.
 */
struct ProcModeRunning {
        /**
         * @brief The index associated with this running state (e.g., CPU ID).
         */
        std::size_t index;
        /**
         * @brief The start time of the running period.
         */
        double start;
        /**
         * @brief The stop time of the running period.
         */
        double stop;
};

/**
 * @brief Represents a processor mode where the processor is in sleep state.
 */
struct ProcModeSleep {
        /**
         * @brief The index associated with this sleep state (e.g., CPU ID).
         */
        std::size_t index;
        /**
         * @brief The start time of the sleep period.
         */
        double start;
        /**
         * @brief The stop time of the sleep period.
         */
        double stop;
};

/**
 * @brief A variant type representing a Gantt chart command.
 */
using command = std::variant<
    Arrival,
    Deadline,
    Finished,
    Execution,
    ActiveNonCont,
    ProcModeIdle,
    ProcModeRunning,
    ProcModeSleep>;

/**
 * @brief Represents the structure of a Gantt chart.
 */
struct Gantt {
        /**
         * @brief The number of axes in the Gantt chart.
         */
        std::size_t nb_axis;
        /**
         * @brief The total duration represented by the Gantt chart.
         */
        double duration;
        /**
         * @brief A vector of commands that define the Gantt chart events.
         */
        std::vector<command> commands;
};

/**
 * @brief Generates a Gantt chart from a vector of logs and hardware information.
 *
 * @param logs A vector of pairs, where each pair contains a timestamp and a trace event.
 * @param platform The hardware platform configuration.
 * @return A `Gantt` object representing the generated Gantt chart.
 */
auto generate_gantt(
    const std::vector<std::pair<double, protocols::traces::trace>>& logs,
    const protocols::hardware::Hardware& platform) -> Gantt;

/**
 * @brief Generates a processor mode Gantt chart from a vector of logs and hardware information.
 *
 * @param logs A vector of pairs, where each pair contains a timestamp and a trace event.
 * @param platform The hardware platform configuration.
 * @return A `Gantt` object representing the generated processor mode Gantt chart.
 */
auto generate_proc_mode(
    const std::vector<std::pair<double, protocols::traces::trace>>& logs,
    const protocols::hardware::Hardware& platform) -> Gantt;
}; // namespace outputs::gantt

#endif