#ifndef OUTPUTS_RTSCHED_HPP
#define OUTPUTS_RTSCHED_HPP

#include "gantt.hpp"
#include <string>

namespace outputs::gantt::rtsched {

/**
 * @brief Draws a Gantt chart representation suitable for real-time scheduling visualization.
 *
 * This function takes a `Gantt` object as input and generates a string
 * representing the chart in a format appropriate for displaying real-time
 * scheduling information. The exact formatting is implementation-specific,
 * but it should clearly show task start times, durations, and potentially
 * resource allocation.
 *
 * @param chart The Gantt chart to draw.  Must be a valid `outputs::gantt::Gantt` object.
 * @return A string containing the textual representation of the Gantt chart.
 */
auto draw(const outputs::gantt::Gantt& chart) -> std::string;

} // namespace outputs::gantt::rtsched

#endif