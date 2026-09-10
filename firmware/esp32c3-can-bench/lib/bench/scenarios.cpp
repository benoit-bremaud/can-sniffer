#include "scenarios.h"

namespace bench {
const ScenarioDefinition* scenario_definition(Scenario scenario) {
    static const ScenarioDefinition reference125{Bitrate::K125, 10};
    static const ScenarioDefinition reference250{Bitrate::K250, 10};
    static const ScenarioDefinition varied125{Bitrate::K125, 12};
    switch (scenario) {
    case Scenario::Reference125: return &reference125;
    case Scenario::Reference250: return &reference250;
    case Scenario::Varied125: return &varied125;
    default: return nullptr;
    }
}

bool scenario_frame(Scenario scenario, uint32_t index, Frame& output) {
    const auto* definition = scenario_definition(scenario);
    if (!definition || index >= definition->attempts) { return false; }
    Frame frame;
    if (scenario == Scenario::Varied125) {
        const uint32_t ids[] = {0x001ABCDE, 0x001ABCDF, 0x001ABCE0};
        const uint8_t group = static_cast<uint8_t>(index % 3);
        frame.id = ids[group];
        const uint8_t bytes[] = {static_cast<uint8_t>(index), group, 0xAA, 0x55,
                                 0x00, 0xFF, 0x12, 0x34};
        for (unsigned i = 0; i < 8; ++i) { frame.data[i] = bytes[i]; }
    }
    output = frame;
    return true;
}

const char* scenario_name(Scenario scenario) {
    switch (scenario) {
    case Scenario::None: return "none";
    case Scenario::Reference125: return "reference125";
    case Scenario::Reference250: return "reference250";
    case Scenario::Varied125: return "varied125";
    }
    return "unknown";
}
}  // namespace bench
