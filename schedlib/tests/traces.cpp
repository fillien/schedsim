#include "gtest/gtest.h"
#include <filesystem>
#include <protocols/traces.hpp>
#include <rapidjson/document.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/writer.h>

using namespace protocols::traces;

class TraceEventTest : public ::testing::Test {};
