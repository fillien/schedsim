#pragma once

/// @file error.hpp
/// @brief IO-specific exception types for the schedsim I/O library.
/// @ingroup io

#include <stdexcept>
#include <string>

namespace schedsim::io {

/// @brief Exception for I/O errors (loading, parsing, validation).
///
/// Thrown by loader functions when JSON input is malformed, required fields
/// are missing, or values fail semantic validation (e.g. negative periods).
///
/// @ingroup io
/// @see load_platform, load_scenario
class LoaderError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;

    /// @brief Construct a LoaderError with a contextual prefix.
    ///
    /// The resulting message is formatted as `"context: message"`.
    ///
    /// @param message  Human-readable description of the error.
    /// @param context  Additional context such as the file path or field name.
    LoaderError(const std::string& message, const std::string& context)
        : std::runtime_error(context + ": " + message) {}
};

} // namespace schedsim::io
