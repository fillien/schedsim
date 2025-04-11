#ifndef OUTPUT_TEXTUAL_HPP
#define OUTPUT_TEXTUAL_HPP

#include <protocols/traces.hpp>

#include <iostream>
#include <vector>

namespace outputs::textual {

/**
 * @brief Serializes a single trace to an output stream.
 *
 * This function takes a reference to an output stream and a constant reference
 * to a `protocols::traces::trace` object, and writes the trace data to the stream
 * in a textual format. The specific formatting is not defined here but is assumed
 * to be implemented within this function.
 *
 * @param out The output stream to write to.
 * @param tra The trace to serialize.
 */
void serialize(std::ostream& out, const protocols::traces::trace& tra);

/**
 * @brief Prints a vector of time-trace pairs to an output stream.
 *
 * This function iterates through a vector of `std::pair<double, protocols::traces::trace>`,
 * and for each pair, it writes the time (the double value) and then serializes the associated
 * trace using the `serialize` function.  The output is written to the provided output stream.
 *
 * @param out The output stream to write to.
 * @param in The vector of time-trace pairs to print.
 */
void print(std::ostream& out, const std::vector<std::pair<double, protocols::traces::trace>>& in);

}; // namespace outputs::textual

#endif