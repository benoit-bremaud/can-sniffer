#pragma once
#include "bench.h"

namespace bench {
enum class Scenario { None, Reference125, Reference250, Varied125 };
struct ScenarioDefinition {
    Bitrate bitrate;
    uint32_t attempts;
};
/// Invalid/None returns null; definitions have static lifetime.
const ScenarioDefinition* scenario_definition(Scenario scenario);
/// Generate a bounded synthetic frame; invalid input leaves output unchanged.
bool scenario_frame(Scenario scenario, uint32_t index, Frame& output);
const char* scenario_name(Scenario scenario);
}  // namespace bench
