// Placeholder payload of a Preset queue entry. WeatherPreset / ApplyPreset belong to plan Feature 6 and do not exist
// yet; this keeps the queue entry kind and its slot so Feature 6 replaces the struct and fills in the apply branch.
#pragma once

namespace CoW {
namespace Weather {

struct WeatherPreset
{
    char id[64] = {};
};

} // namespace Weather
} // namespace CoW
