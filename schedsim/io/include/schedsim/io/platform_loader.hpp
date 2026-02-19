#pragma once

/// @file platform_loader.hpp
/// @brief Functions for loading hardware platform descriptions from JSON.
/// @ingroup io_loaders

#include <schedsim/core/engine.hpp>

#include <filesystem>
#include <string_view>

namespace schedsim::io {

/// @brief Load a platform definition from a JSON file.
///
/// Reads the JSON file at @p path, auto-detects the format, and populates
/// the engine's platform with processor types, clock domains, power domains,
/// and processor instances.
///
/// @note Does **not** call `Platform::finalize()` -- the caller must do that
///       after optionally adding tasks or further configuration.
///
/// @param engine  The simulation engine whose platform will be populated.
/// @param path    Filesystem path to the JSON platform description.
///
/// @throws LoaderError  If the file cannot be read or contains invalid JSON.
///
/// @see load_platform_from_string, core::Platform
void load_platform(core::Engine& engine, const std::filesystem::path& path);

/// @brief Load a platform definition from a JSON string.
///
/// Parses @p json directly and populates the engine's platform. Behaves
/// identically to load_platform except the input comes from a string
/// rather than a file.
///
/// @param engine  The simulation engine whose platform will be populated.
/// @param json    JSON content describing the platform.
///
/// @throws LoaderError  If the JSON is malformed or fails validation.
///
/// @see load_platform, core::Platform
void load_platform_from_string(core::Engine& engine, std::string_view json);

} // namespace schedsim::io
