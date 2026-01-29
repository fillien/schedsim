#pragma once

#include <schedsim/core/engine.hpp>

#include <filesystem>
#include <string_view>

namespace schedsim::io {

// Load platform from JSON file (auto-detects format)
// Does NOT call platform.finalize() - caller must do that
void load_platform(core::Engine& engine, const std::filesystem::path& path);

// Load platform from JSON string
void load_platform_from_string(core::Engine& engine, std::string_view json);

} // namespace schedsim::io
