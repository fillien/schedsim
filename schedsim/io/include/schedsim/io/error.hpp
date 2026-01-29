#pragma once

#include <stdexcept>
#include <string>

namespace schedsim::io {

// Exception for I/O errors (loading, parsing, validation)
class LoaderError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;

    // Convenience constructor with context
    LoaderError(const std::string& message, const std::string& context)
        : std::runtime_error(context + ": " + message) {}
};

} // namespace schedsim::io
