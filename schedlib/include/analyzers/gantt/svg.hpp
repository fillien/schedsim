#ifndef OUTPUTS_SVG_HPP
#define OUTPUTS_SVG_HPP

#include "analyzers/gantt/gantt.hpp"
#include <string>

namespace outputs::gantt::svg {

/**
 * @brief Draws a Gantt chart as an SVG string.
 *
 * This function takes a `Gantt` object and generates its visual representation
 * in the Scalable Vector Graphics (SVG) format.  The resulting string can be
 * used to display or save the chart.
 *
 * @param chart The Gantt chart to draw.
 * @return A string containing the SVG representation of the chart.
 */
auto draw(const outputs::gantt::Gantt& chart) -> std::string;

} // namespace outputs::gantt::svg

namespace outputs::gantt::html {

/**
 * @brief Draws a Gantt chart as an HTML string.
 *
 * This function takes a `Gantt` object and generates its visual representation
 * in the HyperText Markup Language (HTML) format. The resulting string can be
 * used to display or save the chart within a web page.
 *
 * @param chart The Gantt chart to draw.
 * @return A string containing the HTML representation of the chart.
 */
auto draw(const outputs::gantt::Gantt& chart) -> std::string;

} // namespace outputs::gantt::html
#endif