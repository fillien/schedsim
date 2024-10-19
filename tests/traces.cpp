#include "gtest/gtest.h"
#include <filesystem>
#include <rapidjson/document.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/writer.h>
#include <protocols/traces.hpp>

using namespace protocols::traces;

class TraceEventTest : public ::testing::Test {};

